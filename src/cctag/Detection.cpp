#define png_infopp_NULL (png_infopp)NULL
#define int_p_NULL (int*)NULL
#include <boost/gil/extension/io/png_io.hpp>

#include <cctag/utils/FileDebug.hpp>
#include <cctag/EllipseGrowing.hpp>
#include <cctag/Detection.hpp>
#include <cctag/Vote.hpp>
#include <cctag/utils/VisualDebug.hpp>
#include <cctag/Multiresolution.hpp>
#include <cctag/Fitting.hpp>
#include <cctag/CCTagFlowComponent.hpp>
#include <cctag/geometry/Point.hpp>
#include <cctag/Statistic.hpp>
#include <cctag/geometry/Ellipse.hpp>
#include <cctag/geometry/EllipseFromPoints.hpp>
#include <cctag/CCTag.hpp>
#include <cctag/Identification.hpp>
#include <cctag/Fitting.hpp>
//#include <cctag/filter/gilTools.hpp>
#include <cctag/Types.hpp>
#include <cctag/Canny.hpp>
#include <cctag/utils/Defines.hpp>
#include <cctag/utils/Talk.hpp> // for DO_TALK macro
#ifdef WITH_CUDA
#include "cuda/tag.h"
#endif

#include <boost/foreach.hpp>
#include <boost/math/constants/constants.hpp>
#include <boost/math/special_functions/round.hpp>
#include <boost/mpl/bool.hpp>
#include <boost/ptr_container/ptr_vector.hpp>
#include <boost/unordered/unordered_set.hpp>
#include <boost/timer.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

#include <opencv2/opencv.hpp>

#include <cmath>
#include <exception>
#include <fstream>
#include <sstream>
#include <list>
#include <utility>
#include <memory>
#include <omp.h>
#ifdef WITH_CUDA
#include <cuda_runtime.h> // only for debugging
#endif // WITH_CUDA

using namespace std;

namespace cctag
{

namespace { using CandidatePtr = std::unique_ptr<Candidate>; }

/* These are the CUDA pipelines that we instantiate for parallel processing.
 * We need at least one.
 * It is uncertain whether the CPU code can handle parallel pipe, but the CUDA
 * code should be able to.
 * BEWARE: this is untested
 */
std::vector<popart::TagPipe*> cudaPipelines;

void constructFlowComponentFromSeed(
        EdgePoint * seed,
        const EdgePointsImage& edgesMap,
        std::vector<CandidatePtr> & vCandidateLoopOne,
        const Parameters & params)
{
  assert( seed );
  // Check if the seed has already been processed, i.e. belongs to an already
  // reconstructed flow component.
  if (!seed->_processedIn)
  {

    CandidatePtr candidate(new Candidate);

    candidate->_seed = seed;
    std::list<EdgePoint*> & convexEdgeSegment = candidate->_convexEdgeSegment;

    // Convex edge linking from the seed in both directions. The linking
    // is performed until the convexity is lost.
    edgeLinking(edgesMap, convexEdgeSegment, seed,
            params._windowSizeOnInnerEllipticSegment, params._averageVoteMin);

    // Compute the average number of received points.
    int nReceivedVote = 0;
    int nVotedPoints = 0;

    BOOST_FOREACH(EdgePoint * p, convexEdgeSegment)
    {
      nReceivedVote += p->_voters.size();
      if (p->_voters.size() > 0)
      {
        ++nVotedPoints;
      }
    }

    // All flow components WILL BE next sorted based on this characteristic (scalar); descending order
#if 1
    candidate->_averageReceivedVote = (float) (nReceivedVote*nReceivedVote) / (float) nVotedPoints;
    auto it = std::lower_bound(vCandidateLoopOne.begin(), vCandidateLoopOne.end(), candidate,
      [](const CandidatePtr& c1, const CandidatePtr& c2) { return c1->_averageReceivedVote > c2->_averageReceivedVote; });
    vCandidateLoopOne.insert(it, std::move(candidate));
#else
    if (vCandidateLoopOne.size() > 0)
    {
      auto it = vCandidateLoopOne.begin();
      while ( ((*it)._averageReceivedVote > candidate._averageReceivedVote) 
              && ( it != vCandidateLoopOne.end() ) )
      {
        ++it;
      }
      vCandidateLoopOne.insert(it, candidate);
    }
    else
    {
      vCandidateLoopOne.push_back(candidate);
    }
#endif
  }
}

void completeFlowComponent(
        Candidate & candidate,
        const std::vector<EdgePoint> & points,
        const EdgePointsImage& edgesMap,
        std::vector<Candidate> & vCandidateLoopTwo,
        std::size_t& nSegmentOut,
        std::size_t runId,
        const Parameters & params)
{
  try
  {
    try
    {
      std::list<EdgePoint*> childrens;

      childrensOf(candidate._convexEdgeSegment, childrens);

      if (childrens.size() < params._minPointsSegmentCandidate)
      {
        return;
      }

      candidate._score = childrens.size();

      float SmFinal = 1e+10;

      std::vector<EdgePoint*> & filteredChildrens = candidate._filteredChildrens;

      outlierRemoval(
              childrens, 
              filteredChildrens,
              SmFinal, 
              params._threshRobustEstimationOfOuterEllipse,
              kWeight,
              60);

      // todo@lilian see the case in outlierRemoval
      // where filteredChildrens.size()==0
      if (filteredChildrens.size() < 5)
      {
        DO_TALK( CCTAG_COUT_DEBUG(" filteredChildrens.size() < 5 "); )
        return;
      }

      std::size_t nLabel = -1;

      {
        ssize_t nSegmentCommon = -1; // std::size_t nSegmentCommon = -1;

        BOOST_FOREACH(EdgePoint * p, filteredChildrens)
        {
          if (p->_nSegmentOut != -1)
          {
            nSegmentCommon = p->_nSegmentOut;
            break;
          }
        }

        if (nSegmentCommon == -1)
        {
#pragma omp critical (G3633564c0b9c11e69a77448a5b9a696f)
          {
            nLabel = nSegmentOut;
            ++nSegmentOut;
          }
        }
        else
        {
          nLabel = nSegmentCommon;
        }

        BOOST_FOREACH(EdgePoint * p, filteredChildrens)
        {
          p->_nSegmentOut = nLabel;
        }
      }

      std::vector<EdgePoint*> & outerEllipsePoints = candidate._outerEllipsePoints;
      cctag::numerical::geometry::Ellipse & outerEllipse = candidate._outerEllipse;

      bool goodInit = false;

      goodInit = ellipseGrowingInit(points, filteredChildrens, outerEllipse);

      ellipseGrowing2(edgesMap, filteredChildrens, outerEllipsePoints, outerEllipse,
                      params._ellipseGrowingEllipticHullWidth, runId, goodInit);
       
      candidate._nLabel = nLabel;

      std::vector<float> vDistFinal;
      vDistFinal.clear();
      vDistFinal.reserve(outerEllipsePoints.size());

      // Clean point (egdePoints*) to ellipse distance with inheritance 
      // -- the current solution is dirty --
      float distMax = 0;

      BOOST_FOREACH(EdgePoint * p, outerEllipsePoints)
      {
        float distFinal = numerical::distancePointEllipse(*p, outerEllipse, 1.f);
        vDistFinal.push_back(distFinal);

        if (distFinal > distMax)
        {
          distMax = distFinal;
        }
      }

      // todo@Lilian : sort => need to be replace by nInf
      SmFinal = numerical::medianRef(vDistFinal);

      if (SmFinal > params._thrMedianDistanceEllipse)
      {
        DO_TALK( CCTAG_COUT_DEBUG("SmFinal < params._thrMedianDistanceEllipse -- after ellipseGrowing"); )
        return;
      }

      float quality = (float) outerEllipsePoints.size() / (float) rasterizeEllipsePerimeter(outerEllipse);
      if (quality > 1.1)
      {
        DO_TALK( CCTAG_COUT_DEBUG("Quality too high!"); )
        return;
      }

      float ratioSemiAxes = outerEllipse.a() / outerEllipse.b();
      if ((ratioSemiAxes < 0.05) || (ratioSemiAxes > 20))
      {
        DO_TALK( CCTAG_COUT_DEBUG("Too high ratio between semi-axes!"); )
        return;
      }

#pragma omp critical (G3633564c0b9c11e69a77448a5b9a696f)
      vCandidateLoopTwo.push_back(candidate);

#ifdef CCTAG_SERIALIZE
      // Add childrens to output the filtering results (from outlierRemoval)
      vCandidateLoopTwo.back().setChildrens(childrens);

      // Write all selectedFlowComponent
      CCTagFlowComponent flowComponent(outerEllipsePoints, childrens, filteredChildrens,
                                       outerEllipse, candidate._convexEdgeSegment,
                                      *(candidate._seed), params._nCircles);
      CCTagFileDebug::instance().outputFlowComponentInfos(flowComponent);
#endif

    }
    catch (cv::Exception& e)
    {
      DO_TALK( CCTAG_COUT_DEBUG( "OpenCV exception" ); )

      const char* err_msg = e.what();
    }
  }
  catch (...)
  {
    DO_TALK( CCTAG_COUT_DEBUG( "Exception raised in the second main loop." ); )
  }
}

/* Brief: Aims to assemble two flow components if they lie on the same image CCTag
 quality: quality of the outer ellipse (fraction of the ellipse perimeter covered 
 by the extracted edge points, i.e. those contained in outerEllipsePoints
 candidate: flow component being assembled
 vCandidateLoopTwo: set of all flow component detected in the image
 outerEllipse: outerEllipse of the flow component being assembled
 outerEllipsePoints: edge points lying extracted on the outer ellipse
 cctagPoints: set of points constituting the final cctag 
 params: parameters of the system's algorithm */
void flowComponentAssembling(
        float & quality,
        const Candidate & candidate,
        const std::vector<Candidate> & vCandidateLoopTwo,
        numerical::geometry::Ellipse & outerEllipse,
        std::vector<EdgePoint*>& outerEllipsePoints,
        std::vector< std::vector< DirectedPoint2d<Eigen::Vector3f> > >& cctagPoints,
        const Parameters & params
#ifndef CCTAG_SERIALIZE
        )
#else
        , std::vector<Candidate> & componentCandidates)
#endif
{
  boost::posix_time::ptime tstart(boost::posix_time::microsec_clock::local_time());
  
  DO_TALK( CCTAG_COUT_DEBUG("================= Look for another segment =================="); )

  int score = -1;
  int iMax = 0;
  int i = 0;

  float ratioExpension = 2.5;
  numerical::geometry::Circle circularResearchArea(
         Point2d<Eigen::Vector3f>( candidate._seed->x(), candidate._seed->y() ),
         candidate._seed->_flowLength * ratioExpension);

  // Search for another segment
  BOOST_FOREACH(const Candidate & anotherCandidate, vCandidateLoopTwo)
  {
    if (&candidate != &anotherCandidate)
    {
      if (candidate._nLabel != anotherCandidate._nLabel)
      {
        if ((anotherCandidate._seed->_flowLength / candidate._seed->_flowLength > 0.666)
                && (anotherCandidate._seed->_flowLength / candidate._seed->_flowLength < 1.5))
        {
          if (isInEllipse(circularResearchArea, 
                  cctag::Point2d<Eigen::Vector3f>(float(anotherCandidate._seed->x()), float(anotherCandidate._seed->y()))))
          {
            if (anotherCandidate._score > score)
            {
              score = anotherCandidate._score;
              iMax = i;
            }
          }
          else
          {
            CCTagFileDebug::instance().setResearchArea(circularResearchArea);
            CCTagFileDebug::instance().outputFlowComponentAssemblingInfos(NOT_IN_RESEARCH_AREA);
          }
        }
        else
        {
          CCTagFileDebug::instance().outputFlowComponentAssemblingInfos(FLOW_LENGTH);
        }
      }
      else
      {
        CCTagFileDebug::instance().outputFlowComponentAssemblingInfos(SAME_LABEL);
      }
    }
    ++i;
#if defined CCTAG_SERIALIZE && defined DEBUG
    if (i < vCandidateLoopTwo.size())
    {
      CCTagFileDebug::instance().incrementFlowComponentIndex(1);
    }
#endif
  }

  DO_TALK( CCTAG_COUT_VAR_DEBUG(iMax); )
  DO_TALK( CCTAG_COUT_VAR_DEBUG(*(vCandidateLoopTwo[iMax])._seed); )

  if (score > 0)
  {
    const Candidate & selectedCandidate = vCandidateLoopTwo[iMax];
    DO_TALK( CCTAG_COUT_VAR_DEBUG(selectedCandidate._outerEllipse); )

    if( isAnotherSegment(outerEllipse, outerEllipsePoints, 
            selectedCandidate._filteredChildrens, selectedCandidate,
            cctagPoints, params._nCrowns * 2,
            params._thrMedianDistanceEllipse) )
    {
      quality = (float) outerEllipsePoints.size() / (float) rasterizeEllipsePerimeter(outerEllipse);

#ifdef CCTAG_SERIALIZE
      componentCandidates.push_back(selectedCandidate);
#endif
      CCTagFileDebug::instance().setFlowComponentAssemblingState(true, iMax);
    }
  }

  boost::posix_time::ptime tstop(boost::posix_time::microsec_clock::local_time());
  boost::posix_time::time_duration d = tstop - tstart;
  const float spendTime = d.total_milliseconds();
}


void cctagDetectionFromEdges(
        CCTag::List&            markers,
        std::vector<EdgePoint>& points,
        const cv::Mat&          src,
        const std::vector<EdgePoint*>& seeds,
        const EdgePointsImage& edgesMap,
        const std::size_t frame,
        int pyramidLevel,
        float scale,
        const Parameters & providedParams,
        cctag::logtime::Mgmt* durations )
{
  // using namespace boost::gil;
  const Parameters& params = Parameters::OverrideLoaded ?
    Parameters::Override : providedParams;

  // Call for debug only. Write the vote result as an image.
  createImageForVoteResultDebug(src, pyramidLevel); //todo@Lilian: change this function to put a cv::Mat as input.

  // Set some timers
  boost::timer t3;
  boost::posix_time::ptime tstart0(boost::posix_time::microsec_clock::local_time());

  std::size_t nSegmentOut = 0;

#ifdef CCTAG_SERIALIZE
  std::stringstream outFlowComponents;
  outFlowComponents << "flowComponentsLevel" << pyramidLevel << ".txt";
  CCTagFileDebug::instance().newSession(outFlowComponents.str());
#endif

  if( seeds.size() <= 0 )
  {
    // No seeds to process
    return;
  }

  const std::size_t nMaximumNbSeeds = std::max(src.rows/2, (int) params._maximumNbSeeds);
  
  const std::size_t nSeedsToProcess = std::min(seeds.size(), nMaximumNbSeeds);

  std::vector<CandidatePtr> vCandidateLoopOne;

  // Process all the nSeedsToProcess-first seeds.
  // In the following loop, a seed will lead to a flow component if it lies
  // on the inner ellipse of a CCTag.
  // The edge points lying on the inner ellipse and their voters (lying on the outer ellipse
  // will be collected and constitute the initial data of a flow component.
  for (int iSeed = 0; iSeed < nSeedsToProcess; ++iSeed)
  {
    assert( seeds[iSeed] );
    constructFlowComponentFromSeed(seeds[iSeed], edgesMap, vCandidateLoopOne, params);
  }

  const std::size_t nFlowComponentToProcessLoopTwo = 
          std::min(vCandidateLoopOne.size(), params._maximumNbCandidatesLoopTwo);

  std::vector<Candidate> vCandidateLoopTwo;
  vCandidateLoopTwo.reserve(nFlowComponentToProcessLoopTwo);

  // Second main loop:
  // From the flow components selected in the first loop, the outer ellipse will
  // be here entirely recovered.
  // The GPU implementation should stop at this point => layers ->  EdgePoint* creation.

  CCTagVisualDebug::instance().initBackgroundImage(src);
  CCTagVisualDebug::instance().newSession( "completeFlowComponent" );
  
#pragma omp parallel for schedule(dynamic, 8)
  for (size_t iCandidate = 0; iCandidate < nFlowComponentToProcessLoopTwo; ++iCandidate)
  {
    size_t runId = iCandidate;
    completeFlowComponent(*vCandidateLoopOne[iCandidate], points, edgesMap, vCandidateLoopTwo, nSegmentOut, runId, params);
  }

  DO_TALK(
    CCTAG_COUT_VAR_DEBUG(vCandidateLoopTwo.size());
    CCTAG_COUT_DEBUG("================= List of seeds =================");
    BOOST_FOREACH(const Candidate & anotherCandidate, vCandidateLoopTwo)
    {
      CCTAG_COUT_DEBUG("X = [ " << anotherCandidate._seed->x() << " , " << anotherCandidate._seed->y() << "]");
    }
  )

  boost::posix_time::ptime tstop1(boost::posix_time::microsec_clock::local_time());
  boost::posix_time::time_duration d1 = tstop1 - tstart0;
  const float spendTime1 = d1.total_milliseconds();

#if defined CCTAG_SERIALIZE && defined DEBUG
  std::stringstream outFlowComponentsAssembling;
  outFlowComponentsAssembling << "flowComponentsAssemblingLevel" << pyramidLevel << ".txt";
  CCTagFileDebug::instance().newSession(outFlowComponentsAssembling.str());
  CCTagFileDebug::instance().initFlowComponentsIndex(2);
#endif

  const size_t candidateLoopTwoCount = vCandidateLoopTwo.size();
  
#pragma omp parallel for schedule(dynamic, 8)
  for (size_t iCandidate = 0; iCandidate < candidateLoopTwoCount; ++iCandidate)
  {
    const Candidate& candidate = vCandidateLoopTwo[iCandidate];

#ifdef CCTAG_SERIALIZE
    CCTagFileDebug::instance().resetFlowComponent();
    std::vector<Candidate> componentCandidates;
#ifdef DEBUG
    CCTagFileDebug::instance().resetFlowComponent();
#endif
#endif

    // TODO: remove copying
    std::vector<EdgePoint*> outerEllipsePoints = candidate._outerEllipsePoints;
    cctag::numerical::geometry::Ellipse outerEllipse = candidate._outerEllipse;

    std::vector< std::vector< DirectedPoint2d<Eigen::Vector3f> > > cctagPoints;

    try
    {
      float quality = (float) outerEllipsePoints.size() / (float) rasterizeEllipsePerimeter(outerEllipse);

      if (params._searchForAnotherSegment)
      {
        if ((quality > 0.25) && (quality < 0.7))
        {
          // Search for another segment
          flowComponentAssembling( quality, candidate, vCandidateLoopTwo,
                  outerEllipse, outerEllipsePoints, cctagPoints, params
#ifndef CCTAG_SERIALIZE
                  );
#else
                  , componentCandidates);
#endif
        }
      }

      // Add the flowComponent from candidate to cctagPoints // Add intermediary points - required ? todo@Lilian
      // cctagPoints may be not empty, i.e. when the assembling succeed.
      if (! addCandidateFlowtoCCTag(candidate._filteredChildrens, 
              candidate._outerEllipsePoints, outerEllipse,
              cctagPoints, params._nCrowns * 2))
      {
        DO_TALK( CCTAG_COUT_DEBUG("Points outside the outer ellipse OR CCTag not valid : bad gradient orientations"); )
        CCTagFileDebug::instance().outputFlowComponentAssemblingInfos(PTSOUTSIDE_OR_BADGRADORIENT);
        CCTagFileDebug::instance().incrementFlowComponentIndex(0);
        continue;
      }
      else
      {
#ifdef CCTAG_SERIALIZE
        componentCandidates.push_back(candidate);
#endif
        DO_TALK( CCTAG_COUT_DEBUG("Points inside the outer ellipse and good gradient orientations"); )
      }
      // Create ellipse with its real size from original image.
      cctag::numerical::geometry::Ellipse rescaleEllipse(outerEllipse.center(), outerEllipse.a() * scale, outerEllipse.b() * scale, outerEllipse.angle());
      
      int realPixelPerimeter = rasterizeEllipsePerimeter(rescaleEllipse);

      float realSizeOuterEllipsePoints = quality*realPixelPerimeter;

      // Naive reject condition todo@Lilian
      if ( ( ( quality <= 0.35 ) && ( realSizeOuterEllipsePoints >= 300.0 ) ) ||//0.35
               ( ( quality <= 0.45f ) && ( realSizeOuterEllipsePoints >= 200.0 ) && ( realSizeOuterEllipsePoints < 300.0 ) ) ||
               ( ( quality <= 0.5f ) && ( realSizeOuterEllipsePoints >= 100.0 ) && ( realSizeOuterEllipsePoints < 200.0 ) ) ||
               ( ( quality <= 0.5f ) && ( realSizeOuterEllipsePoints >= 70.0  ) && ( realSizeOuterEllipsePoints < 100.0 ) ) ||
               ( ( quality <= 0.96f ) && ( realSizeOuterEllipsePoints >= 50.0  ) && ( realSizeOuterEllipsePoints < 70.0 ) ) ||
               ( realSizeOuterEllipsePoints < 50.0  ) )
      {
              DO_TALK( CCTAG_COUT_DEBUG( "Not enough outer ellipse points: realSizeOuterEllipsePoints : " << realSizeOuterEllipsePoints << ", rasterizeEllipsePerimeter : " << rasterizeEllipsePerimeter( outerEllipse )*scale << ", quality : " << quality ); )
              continue;
      }

      cctag::Point2d<Eigen::Vector3f> markerCenter;
      Eigen::Matrix3f markerHomography = Eigen::Matrix3f::Zero();

      const float ratioSemiAxes = outerEllipse.a() / outerEllipse.b();

      if (ratioSemiAxes > 8.0 || ratioSemiAxes < 0.125)
      {
        CCTagFileDebug::instance().outputFlowComponentAssemblingInfos(RATIO_SEMIAXIS);
        CCTagFileDebug::instance().incrementFlowComponentIndex(0);
        DO_TALK( CCTAG_COUT_DEBUG("Too high ratio between semi-axes!"); )
        continue;
      }

      std::vector<float> vDistFinal;
      vDistFinal.clear();
      vDistFinal.reserve(outerEllipsePoints.size());

      float resSquare = 0;
      float distMax = 0;

      // TODO: omp parallel reduction
      BOOST_FOREACH(EdgePoint * p, outerEllipsePoints)
      {
        float distFinal = numerical::distancePointEllipse(*p, outerEllipse, 1.f);
        resSquare += distFinal; //*distFinal;

        if (distFinal > distMax)
        {
          distMax = distFinal;
        }
      }

      resSquare = sqrt(resSquare);
      resSquare /= outerEllipsePoints.size();


      numerical::geometry::Ellipse qIn, qOut;
      computeHull(outerEllipse, 3.6, qIn, qOut);

      bool isValid = true;

      BOOST_FOREACH(const EdgePoint * p, outerEllipsePoints)
      {
        if (!isInHull(qIn, qOut, p))
        {
          isValid = false;
          break;
        }
      }
      if (!isValid)
      {
        CCTagFileDebug::instance().outputFlowComponentAssemblingInfos(PTS_OUTSIDE_ELLHULL);
        CCTagFileDebug::instance().incrementFlowComponentIndex(0);

        DO_TALK( CCTAG_COUT_DEBUG("Distance max to high!"); )
        continue;
      }

      std::vector< Point2d<Eigen::Vector3i> > vPoint;

      float quality2 = 0;

      BOOST_FOREACH(const EdgePoint* p, outerEllipsePoints)
      {
        quality2 += p->normGradient(); // ***
        
        //float theta = atan2(p->y() - outerEllipse.center().y(), p->x() - outerEllipse.center().x()); // cf. supp.
        //quality2 += std::abs(-sin(theta)*p->gradient().x() + cos(theta)*p->gradient().y()); // cf. supp.
      }

      //quality2 = outerEllipsePoints.size()/quality2;
      //quality2 *= quality;
      
      quality2 *= scale; // ***
      
      // New quality

      CCTag* tag = new CCTag( -1,
                              outerEllipse.center(),
                              cctagPoints,
                              outerEllipse,
                              markerHomography,
                              pyramidLevel,
                              scale,
                              quality2 );
#ifdef CCTAG_SERIALIZE
      tag->setFlowComponents( componentCandidates ); // markers.back().setFlowComponents(componentCandidates);
#endif
      
#pragma omp critical (G3d2d664c0c4911e68911305a3a7ae691)
      {
        markers.push_back( tag ); // markers takes responsibility for delete
      }
#ifdef CCTAG_SERIALIZE
#ifdef DEBUG

      CCTagFileDebug::instance().outputFlowComponentAssemblingInfos(PASS_ALLTESTS);
      CCTagFileDebug::instance().incrementFlowComponentIndex(0);
#endif
#endif

      DO_TALK( CCTAG_COUT_DEBUG("------------------------------Added marker------------------------------"); )
    }
    catch (...)
    {
      CCTagFileDebug::instance().outputFlowComponentAssemblingInfos(RAISED_EXCEPTION);
      CCTagFileDebug::instance().incrementFlowComponentIndex(0);
      // Ellipse fitting don't pass.
      CCTAG_COUT_CURRENT_EXCEPTION;
      DO_TALK( CCTAG_COUT_DEBUG( "Exception raised" ); )
    }
  }

  boost::posix_time::ptime tstop2(boost::posix_time::microsec_clock::local_time());
  boost::posix_time::time_duration d2 = tstop2 - tstop1;
  const float spendTime2 = d2.total_milliseconds();
}


void createImageForVoteResultDebug(
        const cv::Mat & src,
        std::size_t nLevel)
{
#if defined(CCTAG_SERIALIZE) && 0 // lilian fixme!
  {
    std::size_t mx = 0;
    
    cv::Mat imgVote(src.rows, src.cols, CV_8UC1, cv::Scalar(0,0,0));

    for (WinnerMap::const_iterator itr = winners.begin(); itr != winners.end(); ++itr)
    {
      EdgePoint* winner = itr->first;
      const std::vector<EdgePoint*>& v = itr->second;
      if (mx < v.size())
      {
        mx = v.size();
      }
    }

    for (WinnerMap::const_iterator itr = winners.begin(); itr != winners.end(); ++itr)
    {
      EdgePoint* winner = itr->first;
      const std::vector<EdgePoint*>& v = itr->second;
      imgVote.at<uchar>(winner->y(),winner->x()) = (unsigned char) ((v.size() * 10.0));
    }

    //std::stringstream outFilenameVote;
    //outFilenameVote << "/home/lilian/data/vote_" << nLevel << ".png";
    //imwrite(outFilenameVote.str(), imgVote);
    
    std::stringstream outFilenameVote;
    outFilenameVote << "voteLevel" << CCTagVisualDebug::instance().getPyramidLevel();
    CCTagVisualDebug::instance().initBackgroundImage(imgVote);
    CCTagVisualDebug::instance().newSession(outFilenameVote.str());
  }
#endif
}

#ifdef WITH_CUDA
popart::TagPipe* initCuda( int      pipeId,
                           uint32_t width,
                           uint32_t height, 
                           const Parameters & params,
                           cctag::logtime::Mgmt* durations )
{
    if( cudaPipelines.size() <= pipeId ) {
        cudaPipelines.resize( pipeId+1 );
    }

    popart::TagPipe* pipe1 = cudaPipelines[pipeId];

    if( not pipe1 ) {
        pipe1 = new popart::TagPipe( params );
        pipe1->initialize( width, height, durations );
        cudaPipelines[pipeId] = pipe1;
    } else {
        if( width  != pipe1->getWidth(0) ||
            height != pipe1->getHeight(0) ) {
            std::cerr << "We cannot change the input frame resolution (yet)" << std::endl;
            exit( -1 );
        }
    }
    return pipe1;
}
#endif // WITH_CUDA

void cctagDetection(CCTag::List& markers,
        const std::size_t frame, 
        const cv::Mat & imgGraySrc,
        const Parameters & providedParams,
        const cctag::CCTagMarkersBank & bank,
        const bool bDisplayEllipses,
        cctag::logtime::Mgmt* durations )

{
    using namespace cctag;
    using namespace boost::numeric::ublas;
    
    const Parameters& params = Parameters::OverrideLoaded ?
      Parameters::Override : providedParams;

    if( durations ) durations->log( "start" );
  
    std::srand(1);

#ifdef WITH_CUDA
    bool cuda_allocates = params._useCuda;
#else
    bool cuda_allocates = false;
#endif
  
    ImagePyramid imagePyramid( imgGraySrc.cols,
                               imgGraySrc.rows,
                               params._numberOfProcessedMultiresLayers,
                               cuda_allocates );

    popart::TagPipe* pipe1 = 0;
#ifdef WITH_CUDA
    if( params._useCuda ) {
        pipe1 = initCuda( 0,
                          imgGraySrc.size().width,
	                      imgGraySrc.size().height,
	                      params,
	                      durations );

        if( durations ) durations->log( "after initCuda" );

        assert( imgGraySrc.elemSize() == 1 );
        assert( imgGraySrc.isContinuous() );
        assert( imgGraySrc.type() == CV_8U );
        unsigned char* pix = imgGraySrc.data;

        pipe1->load( pix );

        if( durations ) {
            cudaDeviceSynchronize();
            durations->log( "after CUDA load" );
        }

        pipe1->tagframe( ); // pix, w, h, params );

        if( durations ) durations->log( "after CUDA stages" );

#ifndef NDEBUG
        pipe1->debug( pix, params );
        if( durations ) durations->log( "after CUDA debug" );
#endif // not NDEBUG
    } else { // not params.useCuda
#endif // WITH_CUDA

        imagePyramid.build( imgGraySrc,
                            params._cannyThrLow,
                            params._cannyThrHigh,
                            &params );

#ifdef WITH_CUDA
    } // not params.useCuda
#endif // WITH_CUDA
  
    if( durations ) durations->log( "before cctagMultiresDetection" );

    cctagMultiresDetection( markers,
                            imgGraySrc,
                            imagePyramid,
                            frame,
                            pipe1,
                            params,
                            durations );

    if( durations ) durations->log( "after cctagMultiresDetection" );

#ifdef WITH_CUDA
    /* identification in CUDA requires a host-side nearby point struct
     * in pinned memory for safe, non-blocking memcpy.
     */
    for( CCTag& tag : markers ) {
        tag.acquireNearbyPointMemory( );
    }
#endif // WITH_CUDA
  
    CCTagVisualDebug::instance().initBackgroundImage(imagePyramid.getLevel(0)->getSrc());

    // Identification step
    if (params._doIdentification)
    {
      CCTagVisualDebug::instance().resetMarkerIndex();

        const int numTags  = markers.size();

#ifdef WITH_CUDA
        if( pipe1 && numTags > 0 ) {
            pipe1->checkTagAllocations( numTags, params );
        }
#endif // WITH_CUDA

        std::vector<cctag::ImageCut> vSelectedCuts[ numTags ];
        int                          detected[ numTags ];
        int                          tagIndex = 0;

        for( const CCTag& cctag : markers ) {
            detected[tagIndex] = cctag::identification::identify_step_1(
                tagIndex,
                cctag,
                vSelectedCuts[tagIndex],
                imagePyramid.getLevel(0)->getSrc(),
                params );

            tagIndex++;
        }

#ifdef WITH_CUDA
        if( pipe1 && numTags > 0 ) {
            pipe1->uploadCuts( numTags, vSelectedCuts, params );
            pipe1->makeCudaStreams( numTags );

            tagIndex = 0;
            for( CCTag& cctag : markers ) {
                if( detected[tagIndex] == status::id_reliable ) {
                    pipe1->imageCenterOptLoop(
                        tagIndex,
                        cctag.rescaledOuterEllipse(),
                        cctag.centerImg(),
                        vSelectedCuts[tagIndex].size(),
                        params,
                        cctag.getNearbyPointBuffer() );
                }

                tagIndex++;
            }
            cudaDeviceSynchronize();
        }
#endif // WITH_CUDA

        tagIndex = 0;

        CCTag::List::iterator it = markers.begin();
        while (it != markers.end())
        {
            CCTag & cctag = *it;

            if( detected[tagIndex] == status::id_reliable ) {
                detected[tagIndex] = cctag::identification::identify_step_2(
                    tagIndex,
                    cctag,
                    vSelectedCuts[tagIndex],
                    bank.getMarkers(),
                    imagePyramid.getLevel(0)->getSrc(),
                    pipe1,
                    params );
            }

            cctag.setStatus( detected[tagIndex] );
            ++it;

            tagIndex++;
        }
        if( durations ) durations->log( "after cctag::identification::identify" );
    }

#ifdef WITH_CUDA
    /* Releasing all points in all threads in the process.
     */
    CCTag::releaseNearbyPointMemory();
#endif
  
    markers.sort();

    CCTagVisualDebug::instance().initBackgroundImage(imagePyramid.getLevel(0)->getSrc());
    CCTagVisualDebug::instance().writeIdentificationView(markers);
    CCTagFileDebug::instance().newSession("identification.txt");

    BOOST_FOREACH(const CCTag & marker, markers)
    {
        CCTagFileDebug::instance().outputMarkerInfos(marker);
    }
}

} // namespace cctag
