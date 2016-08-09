#ifndef VISION_CCTAG_MULTIRESOLUTION_HPP_
#define VISION_CCTAG_MULTIRESOLUTION_HPP_

#include <cctag/CCTag.hpp>
#include <cctag/Params.hpp>
#include <cctag/geometry/Ellipse.hpp>
#include <cctag/geometry/Circle.hpp>
#include <cctag/boostCv/cvImage.hpp>
#include <cctag/ImagePyramid.hpp>
#ifdef WITH_CUDA
#include "cuda/tag.h"
#endif
#include "cctag/utils/LogTime.hpp"

#include <cstddef>
#include <cmath>
#include <vector>

namespace cctag {

struct CCTagParams
{
};

/**
 * @brief Detect all CCTag in the image using multiresolution detection.
 * 
 * @param[out] markers detected cctags
 * @param[in] srcImg
 * @param[in] frame
 * 
 */

void cctagMultiresDetection(
        CCTag::List& markers,
        const cv::Mat& imgGraySrc,
        const ImagePyramid& imagePyramid,
        const std::size_t   frame,
        popart::TagPipe*    cuda_pipe,
        const Parameters&   params,
        cctag::logtime::Mgmt* durations );

void update(CCTag::List& markers, const CCTag& markerToAdd);

} // namespace cctag


#endif
