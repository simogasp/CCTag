#ifndef VISION_CCTAG_PARAMS_HPP_
#define VISION_CCTAG_PARAMS_HPP_

#include <boost/math/constants/constants.hpp>
#include <boost/serialization/access.hpp>
#include <boost/serialization/nvp.hpp>

#include <cmath>
#include <cstddef>
#include <string>

#define NO_WEIGHT 0
#define INV_GRAD_WEIGHT 1
#define INV_SQRT_GRAD_WEIGHT 2

namespace cctag
{
namespace vision
{
namespace marker
{

static const std::string kDefaultCCTagBankFilename( "param3.xml" );
static const std::size_t kDefaultDistSearch = 30;
static const std::size_t kDefaultNumCrowns  = 3;
static const std::size_t kDefaultNumCircles = 6;
static const int kDefaultThrGradientMagInVote = 2500;
static const float kDefaultAngleVoting        = 0.f;
static const float kDefaultRatioVoting        = 4.f;
static const float kDefaultAverageVoteMin        = 1.2f;
static const double kDefaultThrMedianDistanceEllipse = 3.0;
static const std::size_t kDefaultMaximumNbSeeds = 40;
static const std::size_t kDefaultMaximumNbCandidatesLoopTwo = 40;
static const float kDefaultCannyThrLow      =  0.2f ;
static const float kDefaultCannyThrHigh     =  0.4f ;
static const std::size_t kDefaultMinPointsSegmentCandidate =  15;
static const std::size_t kDefaultMinVotesToSelectCandidate =  4;
static const double kDefaultThreshRobustEstimationOfOuterEllipse =  40.0;
static const double kDefaultEllipseGrowingEllipticHullWidth =  3.0;
static const std::size_t kDefaultWindowSizeOnInnerEllipticSegment =  20;
static const std::size_t kDefaultNumberOfMultiresLayers = 3;
static const std::size_t kDefaultNumberOfProcessedMultiresLayers = 3;
static const std::size_t kDefaultNumCutsInIdentStep = 12;
static const std::size_t kDefaultNumSamplesOuterEdgePointsRefinement = 10;
static const std::size_t kDefaultCutsSelectionTrials = 2000;
static const std::size_t kDefaultSampleCutLength = 100;
static const double kDefaultMinIdentProba = 1e-14;
static const bool kDefaultUseLMDif = false;
static const bool kDefaultSearchForAnotherSegment = true;
static const bool kDefaultWriteOutput = false;
static const bool kDefaultDoIdentification = true;

static const std::string kParamCCTagBankFilename( "kParamCCTagBankFilename" );
static const std::string kParamCannyThrLow( "kParamCannyThrLow" );
static const std::string kParamCannyThrHigh( "kParamCannyThrHigh" );
static const std::string kParamDistSearch( "kParamDistSearch" );
static const std::string kThrGradientMagInVote("kThrGradientMagInVote");
static const std::string kParamAngleVoting( "kParamAngleVoting" );
static const std::string kParamRatioVoting( "kParamRatioVoting" );
static const std::string kParamAverageVoteMin( "kParamAverageVoteMin" );
static const std::string kParamThrMedianDistanceEllipse( "kParamThrMedianDistanceEllipse" );
static const std::string kParamMaximumNbSeeds( "kParamMaximumNbSeeds" );
static const std::string kParamMaximumNbCandidatesLoopTwo( "kParamMaximumNbCandidatesLoopTwo" );
static const std::string kParamNumCrowns( "kParamNumCrowns" );
static const std::string kParamMinPointsSegmentCandidate( "kParamMinPointsSegmentCandidate" );
static const std::string kParamMinVotesToSelectCandidate( "kParamMinVotesToSelectCandidate" );
static const std::string kParamThreshRobustEstimationOfOuterEllipse( "kParamThreshRobustEstimationOfOuterEllipse" );
static const std::string kParamEllipseGrowingEllipticHullWidth( "kParamEllipseGrowingEllipticHullWidth" );
static const std::string kParamWindowSizeOnInnerEllipticSegment( "kParamWindowSizeOnInnerEllipticSegment" );
static const std::string kParamNumberOfMultiresLayers( "kParamNumberOfMultiresLayers" );
static const std::string kParamNumberOfProcessedMultiresLayers( "kParamNumberOfProcessedMultiresLayers" );
static const std::string kParamNumCutsInIdentStep( "kParamNumCutsInIdentStep" );
static const std::string kParamNumSamplesOuterEdgePointsRefinement( "kParamNumSamplesOuterEdgePointsRefinement" );
static const std::string kParamCutsSelectionTrials( "kParamCutsSelectionTrials" );
static const std::string kParamSampleCutLength( "kParamSampleCutLength" );
static const std::string kParamMinIdentProba( "kParamMinIdentProba" );
static const std::string kParamUseLMDif( "kParamUseLMDif" );
static const std::string kParamSearchForAnotherSegment( "kParamSearchForAnotherSegment" );
static const std::string kParamWriteOutput( "kParamWriteOutput" );
static const std::string kParamDoIdentification( "kParamDoIdentification" );

static const std::size_t kWeight = INV_GRAD_WEIGHT; // todo@L

struct Parameters
{
  friend class boost::serialization::access;
  Parameters()
    : _cctagBankFilename( kDefaultCCTagBankFilename )
    , _cannyThrLow( kDefaultCannyThrLow )
    , _cannyThrHigh( kDefaultCannyThrHigh )
    , _distSearch( kDefaultDistSearch )
    , _thrGradientMagInVote( kDefaultThrGradientMagInVote )
    , _angleVoting( kDefaultAngleVoting )
    , _ratioVoting( kDefaultRatioVoting )
    , _averageVoteMin( kDefaultAverageVoteMin )
    , _thrMedianDistanceEllipse( kDefaultThrMedianDistanceEllipse )
    , _maximumNbSeeds( kDefaultMaximumNbSeeds )
    , _maximumNbCandidatesLoopTwo( kDefaultMaximumNbCandidatesLoopTwo )
    , _numCrowns( kDefaultNumCrowns )
    , _minPointsSegmentCandidate( kDefaultMinPointsSegmentCandidate )
    , _minVotesToSelectCandidate( kDefaultMinVotesToSelectCandidate )
    , _threshRobustEstimationOfOuterEllipse( kDefaultThreshRobustEstimationOfOuterEllipse )
    , _ellipseGrowingEllipticHullWidth( kDefaultEllipseGrowingEllipticHullWidth )
    , _windowSizeOnInnerEllipticSegment( kDefaultWindowSizeOnInnerEllipticSegment )
    , _numberOfMultiresLayers( kDefaultNumberOfMultiresLayers )
    , _numberOfProcessedMultiresLayers( kDefaultNumberOfProcessedMultiresLayers )
    , _numCutsInIdentStep( kDefaultNumCutsInIdentStep )
    , _numSamplesOuterEdgePointsRefinement( kDefaultNumSamplesOuterEdgePointsRefinement )
    , _cutsSelectionTrials( kDefaultCutsSelectionTrials )
    , _sampleCutLength( kDefaultSampleCutLength )
    , _minIdentProba( kDefaultMinIdentProba )
    , _useLMDif( kDefaultUseLMDif )
    , _searchForAnotherSegment( kDefaultSearchForAnotherSegment )
    , _writeOutput( kDefaultWriteOutput )
    , _doIdentification( kDefaultDoIdentification )
  {
    _nCircles = 2*_numCrowns;
  }

  std::string _cctagBankFilename; // path of the cctag bank containing the radius radio
  // of the CCTag library
  float _cannyThrLow; // canny low threshold
  float _cannyThrHigh; // canny high threshold
  std::size_t _distSearch; // maximum distance (in pixels) of research from one edge points
  // to another one. maximum length of a arc segment composing the polygonal line.
  int _thrGradientMagInVote; // during the voting procedure, the gradient direction
  // is followed as long as the gradient magnitude is
  // greater than this threshold
  float _angleVoting; // maximum angle between of gradient directions of two consecutive
  // edge points.
  float _ratioVoting; // maximum distance ratio between of gradient directions of two consecutive
  // edge points.
  float _averageVoteMin;
  double _thrMedianDistanceEllipse;
  std::size_t _maximumNbSeeds; // number of seeds to process as potential candidates
  std::size_t _maximumNbCandidatesLoopTwo;
  std::size_t _numCrowns; // number of crowns
  std::size_t _nCircles; // number of circles
  std::size_t _minPointsSegmentCandidate; // minimal number of points on the outer ellipse to select an inner segment candidate
  std::size_t _minVotesToSelectCandidate; // minimum number of received votes to select an edge
  // point as a new seed.
  double _threshRobustEstimationOfOuterEllipse; // LMeDs threshold on robust estimation of the outer ellipse
  double _ellipseGrowingEllipticHullWidth; // width of elliptic hull in ellipse growing
  std::size_t _windowSizeOnInnerEllipticSegment; // window size on the inner elliptic segment
  std::size_t _numberOfMultiresLayers; // number of multi-resolution layers
  std::size_t _numberOfProcessedMultiresLayers; // number of processed layers in multi-resolution
  std::size_t _numCutsInIdentStep; // number of cuts in the identification step
  std::size_t _numSamplesOuterEdgePointsRefinement; // number of sample for the outer edge points refinement in identification
  std::size_t _cutsSelectionTrials; // number of trials in cuts selection
  std::size_t _sampleCutLength; // sample cut length
  double _minIdentProba; // minimal probability of delivered by the identification algorithm
  // to consider a candidate as a CCTag
  bool _useLMDif;
  bool _searchForAnotherSegment; // is CCTag can be made of many flow components (2).
  // This implies an assembling step which may be time
  // time consuming.
  bool _writeOutput;
  bool _doIdentification; // perform the identification step

  template<class Archive>
  void serialize(Archive & ar, const unsigned int version)
  {
    ar & BOOST_SERIALIZATION_NVP( _cctagBankFilename );
    ar & BOOST_SERIALIZATION_NVP( _cannyThrLow );
    ar & BOOST_SERIALIZATION_NVP( _cannyThrHigh );
    ar & BOOST_SERIALIZATION_NVP( _distSearch );
    ar & BOOST_SERIALIZATION_NVP( _thrGradientMagInVote );
    ar & BOOST_SERIALIZATION_NVP( _angleVoting );
    ar & BOOST_SERIALIZATION_NVP( _ratioVoting );
    ar & BOOST_SERIALIZATION_NVP( _averageVoteMin);
    ar & BOOST_SERIALIZATION_NVP( _thrMedianDistanceEllipse);
    ar & BOOST_SERIALIZATION_NVP( _maximumNbSeeds);
    ar & BOOST_SERIALIZATION_NVP( _maximumNbCandidatesLoopTwo);
    ar & BOOST_SERIALIZATION_NVP( _numCrowns );
    ar & BOOST_SERIALIZATION_NVP( _minPointsSegmentCandidate );
    ar & BOOST_SERIALIZATION_NVP( _minVotesToSelectCandidate );
    ar & BOOST_SERIALIZATION_NVP( _threshRobustEstimationOfOuterEllipse );
    ar & BOOST_SERIALIZATION_NVP( _ellipseGrowingEllipticHullWidth );
    ar & BOOST_SERIALIZATION_NVP( _windowSizeOnInnerEllipticSegment );
    ar & BOOST_SERIALIZATION_NVP( _numberOfMultiresLayers );
    ar & BOOST_SERIALIZATION_NVP( _numberOfProcessedMultiresLayers );
    ar & BOOST_SERIALIZATION_NVP( _numCutsInIdentStep );
    ar & BOOST_SERIALIZATION_NVP( _numSamplesOuterEdgePointsRefinement );
    ar & BOOST_SERIALIZATION_NVP( _cutsSelectionTrials );
    ar & BOOST_SERIALIZATION_NVP( _sampleCutLength );
    ar & BOOST_SERIALIZATION_NVP( _minIdentProba );
    ar & BOOST_SERIALIZATION_NVP( _useLMDif );
    ar & BOOST_SERIALIZATION_NVP( _searchForAnotherSegment );
    ar & BOOST_SERIALIZATION_NVP( _writeOutput );
    ar & BOOST_SERIALIZATION_NVP( _doIdentification );
    _nCircles = 2*_numCrowns;
  }
};

}
}
}

#endif
