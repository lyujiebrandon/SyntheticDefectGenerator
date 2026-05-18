#pragma once

#include <functional>
#include <vector>
#include <QString>
#include <opencv2/core.hpp>

// Reconstructs a per-pixel depth map from a focal stack.
// Uses focus-metric-based winner-takes-all: for each pixel, the frame with
// the highest sharpness score determines that pixel's depth value.
class DepthMapReconstructor
{
public:
    struct Params {
        int kernelSize = 5;  // Laplacian kernel (must be odd)
    };

    using ProgressCallback = std::function<void(int percent, const QString& message)>;

    bool reconstruct(const std::vector<cv::Mat>& stack,
                     const Params& params,
                     ProgressCallback onProgress = {});

    bool hasDepthMap() const { return !m_depthMap.empty(); }
    const cv::Mat& getDepthMap() const { return m_depthMap; }

private:
    // Returns per-pixel Variance of Laplacian sharpness map for one frame.
    cv::Mat computeSharpnessMap(const cv::Mat& frame, int kernelSize) const;

    cv::Mat m_depthMap;   // CV_32F, values = frame index of sharpest focus
};
