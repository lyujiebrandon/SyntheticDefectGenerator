#pragma once

#include <functional>
#include <vector>
#include <QString>
#include <opencv2/core.hpp>

// Reconstructs a per-pixel depth map from a focal stack.
// Uses focus-metric-based winner-takes-all: for each pixel, the frame with
// the highest sharpness score determines that pixel's depth value.
// Output depth map is CV_32F normalized to [0.0, 1.0]:
//   0.0 = focalMin diopters (lowest Z), 1.0 = focalMax diopters (highest Z).
class DepthMapReconstructor
{
public:
    struct Params {
        int   kernelSize       = 7;     // Laplacian kernel (must be odd)
        float sharpnessMinimum = 30.f;  // Absolute floor: pixels below this are always masked
        float sharpnessRatio   = 0.02f; // Fraction of scene peak sharpness below which pixels are masked
        float focalMin         = -4.5f; // Optotune liquid lens minimum (diopters)
        float focalMax         =  4.7f; // Optotune liquid lens maximum (diopters)
    };

    using ProgressCallback = std::function<void(int percent, const QString& message)>;

    // focalPowers: diopter value per stack frame in load order.
    // If empty, depth is uniformly normalized from 0.0 (first frame) to 1.0 (last frame).
    bool reconstruct(const std::vector<cv::Mat>& stack,
                     const Params& params,
                     const std::vector<float>& focalPowers = {},
                     ProgressCallback onProgress = {});

    bool hasDepthMap() const { return !m_depthMap.empty(); }
    const cv::Mat& getDepthMap() const { return m_depthMap; }

private:
    cv::Mat computeSharpnessMap(const cv::Mat& frame, const Params& params) const;

    cv::Mat m_depthMap;   // CV_32F, [0.0, 1.0] normalized diopter depth
};
