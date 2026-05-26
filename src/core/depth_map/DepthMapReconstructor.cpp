#include "DepthMapReconstructor.h"

#include <opencv2/imgproc.hpp>

cv::Mat DepthMapReconstructor::computeSharpnessMap(const cv::Mat& frame, int kernelSize) const
{
    cv::Mat gray;
    if (frame.channels() > 1)
        cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);
    else
        gray = frame;

    cv::Mat laplacian;
    cv::Laplacian(gray, laplacian, CV_32F, kernelSize);

    // Square the Laplacian response, then blur the result over a local window.
    // Without the blur, sharpness is per-pixel noise — blurry background regions
    // have tiny random values and a different frame "wins" at every pixel,
    // making the background indistinguishable from the in-focus object.
    // Aggregating over a neighbourhood ensures only truly in-focus regions
    // produce a sustained high response.
    cv::Mat sharpness;
    cv::multiply(laplacian, laplacian, sharpness);

    int blurKernel = kernelSize * 2 + 1;  // always odd; ~2× the laplacian scale
    cv::GaussianBlur(sharpness, sharpness, cv::Size(blurKernel, blurKernel), 0);

    return sharpness;
}

bool DepthMapReconstructor::reconstruct(const std::vector<cv::Mat>& stack,
                                         const Params& params,
                                         ProgressCallback onProgress)
{
    if (stack.empty())
        return false;

    const int rows = stack[0].rows;
    const int cols = stack[0].cols;
    const int N    = static_cast<int>(stack.size());

    cv::Mat bestSharpness = cv::Mat::zeros(rows, cols, CV_32F);
    m_depthMap            = cv::Mat::zeros(rows, cols, CV_32F);

    for (int i = 0; i < N; ++i) {
        cv::Mat sharpness = computeSharpnessMap(stack[i], params.kernelSize);

        // Vectorised winner-takes-all: update depth wherever this frame is sharper
        cv::Mat mask;
        cv::compare(sharpness, bestSharpness, mask, cv::CMP_GT);
        m_depthMap.setTo(static_cast<float>(i), mask);
        cv::max(bestSharpness, sharpness, bestSharpness);

        if (onProgress) {
            onProgress((i + 1) * 100 / N,
                       QString("Processing frame %1 / %2").arg(i + 1).arg(N));
        }
    }

    // Gaussian smooth the raw depth map to fill noisy speckles at region
    // boundaries and in low-texture background areas.
    cv::GaussianBlur(m_depthMap, m_depthMap, cv::Size(7, 7), 0);

    cv::normalize(m_depthMap, m_depthMap, 0, 255, cv::NORM_MINMAX);
    return true;
}
