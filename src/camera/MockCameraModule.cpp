#include "MockCameraModule.h"

#include <opencv2/imgproc.hpp>
#include <cmath>

MockCameraModule::MockCameraModule() = default;

bool MockCameraModule::connect()
{
    // Synthetic test target: gradient background + concentric circles
    m_sourceImage = cv::Mat(480, 640, CV_8UC1);
    for (int y = 0; y < m_sourceImage.rows; ++y)
        for (int x = 0; x < m_sourceImage.cols; ++x)
            m_sourceImage.at<uchar>(y, x) = static_cast<uchar>((x + y) % 256);

    cv::circle(m_sourceImage, cv::Point(320, 240), 120, cv::Scalar(220), -1);
    cv::circle(m_sourceImage, cv::Point(320, 240), 80,  cv::Scalar(140), -1);
    cv::circle(m_sourceImage, cv::Point(320, 240), 40,  cv::Scalar(220), -1);
    cv::circle(m_sourceImage, cv::Point(160, 120), 50,  cv::Scalar(180), -1);
    cv::circle(m_sourceImage, cv::Point(480, 360), 40,  cv::Scalar(200), -1);

    m_connected = true;
    return true;
}

void MockCameraModule::disconnect()
{
    m_connected = false;
    m_sourceImage.release();
}

cv::Mat MockCameraModule::captureFrame()
{
    if (!m_connected || m_sourceImage.empty())
        return {};

    double distance = m_dioptre - kSharpestDioptre;

    // ── Focus breathing: scale the image slightly with dioptre ──────────────
    double scaleFactor = 1.0 + distance * kBreathingCoeff;
    cv::Mat scaled;
    cv::resize(m_sourceImage, scaled, {}, scaleFactor, scaleFactor, cv::INTER_LINEAR);

    // Crop or pad back to the original size (simulate fixed sensor FOV)
    cv::Mat frame = cv::Mat::zeros(m_sourceImage.size(), m_sourceImage.type());
    int cx = scaled.cols / 2, cy = scaled.rows / 2;
    int ox = m_sourceImage.cols / 2, oy = m_sourceImage.rows / 2;
    cv::Rect srcRect(cx - ox, cy - oy, m_sourceImage.cols, m_sourceImage.rows);
    srcRect &= cv::Rect(0, 0, scaled.cols, scaled.rows);
    if (srcRect.width > 0 && srcRect.height > 0)
        scaled(srcRect).copyTo(frame(cv::Rect(0, 0, srcRect.width, srcRect.height)));

    // ── Defocus blur: sigma proportional to distance from sharpest plane ────
    double sigma = std::abs(distance) * kBlurCoeff;
    if (sigma > 0.5) {
        int ks = static_cast<int>(sigma * 4) | 1; // odd kernel
        cv::GaussianBlur(frame, frame, {ks, ks}, sigma);
    }

    return frame;
}
