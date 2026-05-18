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

    cv::Mat sharpness;
    cv::multiply(laplacian, laplacian, sharpness);
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

    // Accumulate: best sharpness per pixel and the frame index that won
    cv::Mat bestSharpness = cv::Mat::zeros(rows, cols, CV_32F);
    m_depthMap            = cv::Mat::zeros(rows, cols, CV_32F);

    for (int i = 0; i < N; ++i) {
        cv::Mat sharpness = computeSharpnessMap(stack[i], params.kernelSize);

        // For each pixel, if this frame is sharper, record its index as depth
        for (int y = 0; y < rows; ++y) {
            for (int x = 0; x < cols; ++x) {
                float s = sharpness.at<float>(y, x);
                if (s > bestSharpness.at<float>(y, x)) {
                    bestSharpness.at<float>(y, x) = s;
                    m_depthMap.at<float>(y, x)    = static_cast<float>(i);
                }
            }
        }

        if (onProgress) {
            int pct = (i + 1) * 100 / N;
            onProgress(pct, QString("Processing frame %1 / %2").arg(i + 1).arg(N));
        }
    }

    // Normalize depth map to [0, 255] for display
    cv::normalize(m_depthMap, m_depthMap, 0, 255, cv::NORM_MINMAX);
    return true;
}
