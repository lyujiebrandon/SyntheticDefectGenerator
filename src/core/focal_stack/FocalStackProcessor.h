#pragma once

#include <functional>
#include <vector>
#include <QString>
#include <opencv2/core.hpp>

// Loads a focal stack from a folder of images and stores it for the
// ImageRegistrator and DepthMapReconstructor pipeline stages.
class FocalStackProcessor
{
public:
    using ProgressCallback = std::function<void(int percent, const QString& message)>;

    // Reads all supported images from folderPath in filename order (ascending).
    // Images should be named so they sort by focal distance, e.g. frame_01.png…frame_20.png.
    bool loadFromFolder(const QString& folderPath, ProgressCallback onProgress = {});

    bool hasStack() const { return !m_stack.empty(); }
    const std::vector<cv::Mat>& getStack() const { return m_stack; }
    std::vector<cv::Mat>&       getStack()        { return m_stack; }
    void clearStack() { m_stack.clear(); }

private:
    std::vector<cv::Mat> m_stack;
};
