#pragma once

#include <functional>
#include <vector>
#include <QString>
#include <opencv2/core.hpp>

// Corrects liquid-lens focus breathing by aligning all frames in a focal
// stack to a common reference frame using ECC (Enhanced Correlation
// Coefficient) maximisation — cv::findTransformECC with MOTION_EUCLIDEAN.
//
// Focus breathing produces a scale + translation shift. MOTION_EUCLIDEAN
// handles translation + rotation (fast). For stronger breathing, switch to
// MOTION_AFFINE which also handles scale.
class ImageRegistrator
{
public:
    enum class MotionModel { Euclidean, Affine };

    struct Params {
        MotionModel motionModel  = MotionModel::Euclidean;
        int         eccIterations = 50;
        double      eccEpsilon    = 1e-3;
        int         gaussianBlurKernel = 5; // pre-blur to improve ECC convergence
    };

    using ProgressCallback = std::function<void(int percent, const QString& message)>;

    // Aligns all frames to the middle frame of the stack.
    // Replaces the stack in-place with the registered frames.
    bool registerStack(std::vector<cv::Mat>& stack,
                       const Params& params,
                       ProgressCallback onProgress = {});

    bool hasRegisteredStack() const { return m_registered; }

private:
    // Takes pre-processed reference images (float, blurred) at full and ¼ scale
    // to avoid recomputing them for every frame.
    cv::Mat alignFrame(const cv::Mat& refFull,
                       const cv::Mat& refSmall,
                       const cv::Mat& frame,
                       const Params& params);

    bool m_registered = false;
};
