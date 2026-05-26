#pragma once

#include <functional>
#include <vector>
#include <QString>
#include <opencv2/core.hpp>

class ICameraModule;
class ILiquidLensController;

// Drives the liquid lens through a dioptre sweep, triggering one camera
// frame per step. Stores the raw focal stack for the ImageRegistrator.
class FocalStackProcessor
{
public:
    struct SweepParams {
        double startDioptre = -2.0;
        double endDioptre   =  2.0;
        int    imageCount   = 20;
    };

    using ProgressCallback = std::function<void(int percent, const QString& message)>;

    bool captureStack(ICameraModule& camera,
                      ILiquidLensController& lens,
                      const SweepParams& params,
                      ProgressCallback onProgress = {});

    bool loadFromFolder(const QString& folderPath, ProgressCallback onProgress = {});

    bool hasStack() const { return !m_stack.empty(); }
    const std::vector<cv::Mat>& getStack() const { return m_stack; }
    std::vector<cv::Mat>&       getStack()        { return m_stack; }
    void clearStack() { m_stack.clear(); }

private:
    std::vector<cv::Mat> m_stack;
};
