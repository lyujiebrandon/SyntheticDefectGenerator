#pragma once

#include <functional>
#include <vector>
#include <QString>
#include <opencv2/core.hpp>

class ICameraModule;

// Drives the camera through a focal sweep and assembles the image stack.
// The stack is stored in memory for the DepthMapReconstructor to consume.
class FocalStackProcessor
{
public:
    struct SweepParams {
        double startFocus  = 0.0;
        double endFocus    = 100.0;
        double stepSize    = 5.0;
        int    imageCount  = 20;
    };

    using ProgressCallback = std::function<void(int percent, const QString& message)>;

    bool captureStack(ICameraModule& camera,
                      const SweepParams& params,
                      ProgressCallback onProgress = {});

    bool hasStack() const { return !m_stack.empty(); }
    const std::vector<cv::Mat>& getStack() const { return m_stack; }
    void clearStack() { m_stack.clear(); }

private:
    std::vector<cv::Mat> m_stack;
};
