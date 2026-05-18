#include "MockCameraModule.h"

#include <opencv2/imgproc.hpp>
#include <cmath>

MockCameraModule::MockCameraModule() = default;

bool MockCameraModule::connect()
{
    // Generate a synthetic 640x480 test pattern (gradient + circle) as the source.
    m_sourceImage = cv::Mat(480, 640, CV_8UC1);
    for (int y = 0; y < m_sourceImage.rows; ++y)
        for (int x = 0; x < m_sourceImage.cols; ++x)
            m_sourceImage.at<uchar>(y, x) = static_cast<uchar>((x + y) % 256);

    cv::circle(m_sourceImage, {320, 240}, 100, cv::Scalar(200), -1);
    cv::circle(m_sourceImage, {320, 240}, 60,  cv::Scalar(120), -1);
    cv::circle(m_sourceImage, {160, 120}, 40,  cv::Scalar(180), -1);

    m_connected = true;
    return true;
}

void MockCameraModule::disconnect()
{
    m_connected = false;
    m_sourceImage.release();
}

bool MockCameraModule::setFocusPosition(double position)
{
    m_focusPosition = position;
    return true;
}

cv::Mat MockCameraModule::captureFrame()
{
    if (!m_connected || m_sourceImage.empty())
        return {};

    // Sharpest at position 50. Blur increases away from that plane.
    constexpr double sharpestPlane = 50.0;
    double distance = std::abs(m_focusPosition - sharpestPlane);
    int kernelSize  = static_cast<int>(1 + distance * 0.4);
    if (kernelSize % 2 == 0) ++kernelSize; // must be odd

    cv::Mat frame;
    cv::GaussianBlur(m_sourceImage, frame, {kernelSize, kernelSize}, 0);
    return frame;
}
