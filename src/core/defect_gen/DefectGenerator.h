#pragma once

#include "DefectTypes.h"

#include <functional>
#include <vector>
#include <QString>
#include <opencv2/core.hpp>

// Applies procedural geometric deformations to a depth map to simulate
// physically realistic surface anomalies (Phase 2 core algorithm).
class DefectGenerator
{
public:
    struct Params {
        int   defectCount   = 100;
        float severity      = 0.5f;   // 0.0 – 1.0
        float scaleFactor   = 1.0f;

        bool enableScratch  = true;
        bool enableDent     = true;
        bool enableCrack    = false;
        bool enablePit      = false;
    };

    using ProgressCallback = std::function<void(int percent, const QString& message)>;

    bool generate(const cv::Mat& depthMap,
                  const Params& params,
                  ProgressCallback onProgress = {});

    bool hasOutput() const { return !m_outputImages.empty(); }

    bool exportDataset(const QString& outputDir,
                       ProgressCallback onProgress = {}) const;

private:
    cv::Mat applyDefect(const cv::Mat& depthMap, DefectType type,
                        float severity, float scale) const;

    cv::Mat applyScratch(const cv::Mat& src, float severity, float scale) const;
    cv::Mat applyDent(const cv::Mat& src, float severity, float scale) const;
    cv::Mat applyCrack(const cv::Mat& src, float severity, float scale) const;
    cv::Mat applyPit(const cv::Mat& src, float severity, float scale) const;

    std::vector<cv::Mat> m_outputImages;
    std::vector<DefectType> m_outputLabels;
};
