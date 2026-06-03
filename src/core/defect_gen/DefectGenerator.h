#pragma once

#include "DefectTypes.h"

#include <functional>
#include <random>
#include <vector>
#include <QString>
#include <opencv2/core.hpp>

// Applies procedural geometric deformations to a depth map to determine defect
// geometry, then renders those defects as visual darkening onto the all-in-focus
// reference image.  The exported images look like real photos of defective parts.
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

    // referenceImage: all-in-focus grayscale composite from DepthMapReconstructor.
    // Defects are rendered as realistic darkening on this image.
    // If referenceImage is empty, falls back to a colourised depth map.
    bool generate(const cv::Mat& depthMap,
                  const cv::Mat& referenceImage,
                  const Params& params,
                  ProgressCallback onProgress = {});

    bool hasOutput() const { return !m_outputImages.empty(); }
    const std::vector<cv::Mat>&    getOutputImages() const { return m_outputImages; }
    const std::vector<DefectType>& getOutputLabels() const { return m_outputLabels; }
    const std::vector<cv::Rect>&   getOutputBounds() const { return m_outputBounds; }

    bool exportDataset(const QString& outputDir,
                       ProgressCallback onProgress = {}) const;

private:
    std::pair<cv::Mat, cv::Rect> applyDefect(const cv::Mat& refImage,
                                              DefectType type,
                                              float severity, float scale) const;

    std::pair<cv::Mat, cv::Rect> applyScratch(const cv::Mat& src, float severity, float scale) const;
    std::pair<cv::Mat, cv::Rect> applyDent   (const cv::Mat& src, float severity, float scale) const;
    std::pair<cv::Mat, cv::Rect> applyCrack  (const cv::Mat& src, float severity, float scale) const;
    std::pair<cv::Mat, cv::Rect> applyPit    (const cv::Mat& src, float severity, float scale) const;

    cv::Mat buildProductMask(const cv::Mat& depthMap) const;
    cv::Point samplePointInMask(std::mt19937& rng) const;

    // Helpers for visual defect rendering
    static void darkenWithMask(cv::Mat& image, const cv::Mat& mask8U, float amount);
    static void darkenGaussian(cv::Mat& image, cv::Point center, int radius,
                                float peakAmount, float sigma);

    std::vector<cv::Mat>     m_outputImages;
    std::vector<DefectType>  m_outputLabels;
    std::vector<cv::Rect>    m_outputBounds;
    cv::Mat                  m_productMask;
    cv::Mat                  m_referenceImage;  // all-in-focus base image (CV_8U)
};
