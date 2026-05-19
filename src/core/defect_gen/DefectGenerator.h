#pragma once

#include "DefectTypes.h"

#include <functional>
#include <random>
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
    const std::vector<cv::Mat>&    getOutputImages() const { return m_outputImages; }
    const std::vector<DefectType>& getOutputLabels() const { return m_outputLabels; }
    const std::vector<cv::Rect>&   getOutputBounds() const { return m_outputBounds; }

    bool exportDataset(const QString& outputDir,
                       ProgressCallback onProgress = {}) const;

private:
    // Returns the defected image AND its bounding rect in one call
    std::pair<cv::Mat, cv::Rect> applyDefect(const cv::Mat& depthMap, DefectType type,
                                              float severity, float scale) const;

    std::pair<cv::Mat, cv::Rect> applyScratch(const cv::Mat& src, float severity, float scale) const;
    std::pair<cv::Mat, cv::Rect> applyDent   (const cv::Mat& src, float severity, float scale) const;
    std::pair<cv::Mat, cv::Rect> applyCrack  (const cv::Mat& src, float severity, float scale) const;
    std::pair<cv::Mat, cv::Rect> applyPit    (const cv::Mat& src, float severity, float scale) const;

    cv::Mat   buildProductMask(const cv::Mat& depthMap) const;
    cv::Point samplePointInMask(std::mt19937& rng) const;

    std::vector<cv::Mat>     m_outputImages;
    std::vector<DefectType>  m_outputLabels;
    std::vector<cv::Rect>    m_outputBounds;
    cv::Mat                  m_productMask;
};
