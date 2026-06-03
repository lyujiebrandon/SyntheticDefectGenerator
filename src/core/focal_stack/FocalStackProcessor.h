#pragma once

#include <functional>
#include <vector>
#include <QString>
#include <opencv2/core.hpp>

// Loads a focal stack from a folder of images and stores it for the
// ImageRegistrator and DepthMapReconstructor pipeline stages.
//
// Preferred input format (CSV method): a folder of sequentially-named images
// (img_00.png … img_45.png) alongside a metadata.csv file that maps each
// filename to its focal power in diopters, e.g. "img_00.png, -4.2".
// Fallback: any supported images loaded in filename order with no focal data.
class FocalStackProcessor
{
public:
    using ProgressCallback = std::function<void(int percent, const QString& message)>;

    bool loadFromFolder(const QString& folderPath, ProgressCallback onProgress = {});

    bool hasStack() const { return !m_stack.empty(); }
    const std::vector<cv::Mat>& getStack() const { return m_stack; }
    std::vector<cv::Mat>&       getStack()        { return m_stack; }
    void clearStack() { m_stack.clear(); }

    bool hasFocalPowers() const { return !m_focalPowers.empty(); }
    const std::vector<float>& getFocalPowers() const { return m_focalPowers; }

private:
    std::vector<cv::Mat>  m_stack;
    std::vector<float>    m_focalPowers;   // diopter value per frame, in stack order
};
