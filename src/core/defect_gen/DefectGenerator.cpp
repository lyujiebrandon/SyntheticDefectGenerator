#include "DefectGenerator.h"

#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>

#include <random>
#include <QDir>

// ─── Public ───────────────────────────────────────────────────────────────────

bool DefectGenerator::generate(const cv::Mat& depthMap,
                                const Params& params,
                                ProgressCallback onProgress)
{
    if (depthMap.empty())
        return false;

    m_outputImages.clear();
    m_outputLabels.clear();

    // Build the list of enabled defect types
    std::vector<DefectType> enabledTypes;
    if (params.enableScratch) enabledTypes.push_back(DefectType::Scratch);
    if (params.enableDent)    enabledTypes.push_back(DefectType::ShallowDent);
    if (params.enableCrack)   enabledTypes.push_back(DefectType::Crack);
    if (params.enablePit)     enabledTypes.push_back(DefectType::SurfacePit);

    if (enabledTypes.empty())
        return false;

    std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<int> typeDist(0, static_cast<int>(enabledTypes.size()) - 1);

    for (int i = 0; i < params.defectCount; ++i) {
        DefectType type = enabledTypes[typeDist(rng)];
        cv::Mat defected = applyDefect(depthMap, type, params.severity, params.scaleFactor);

        m_outputImages.push_back(std::move(defected));
        m_outputLabels.push_back(type);

        if (onProgress) {
            int pct = (i + 1) * 100 / params.defectCount;
            onProgress(pct, QString("Generated image %1 / %2").arg(i + 1).arg(params.defectCount));
        }
    }

    return !m_outputImages.empty();
}

bool DefectGenerator::exportDataset(const QString& outputDir, ProgressCallback onProgress) const
{
    if (m_outputImages.empty())
        return false;

    auto labelToFolder = [](DefectType t) -> QString {
        switch (t) {
            case DefectType::Scratch:    return "scratch";
            case DefectType::ShallowDent:return "dent";
            case DefectType::Crack:      return "crack";
            case DefectType::SurfacePit: return "pit";
        }
        return "unknown";
    };

    // Create per-class subdirectories
    for (const auto& label : m_outputLabels) {
        QDir().mkpath(outputDir + "/" + labelToFolder(label));
    }

    int total = static_cast<int>(m_outputImages.size());
    for (int i = 0; i < total; ++i) {
        QString folder = labelToFolder(m_outputLabels[i]);
        QString path   = QString("%1/%2/img_%3.png").arg(outputDir, folder).arg(i, 5, 10, QChar('0'));

        cv::imwrite(path.toStdString(), m_outputImages[i]);

        if (onProgress) {
            int pct = (i + 1) * 100 / total;
            onProgress(pct, QString("Saved %1 / %2").arg(i + 1).arg(total));
        }
    }

    return true;
}

// ─── Defect Application ────────────────────────────────────────────────────────

cv::Mat DefectGenerator::applyDefect(const cv::Mat& depthMap, DefectType type,
                                      float severity, float scale) const
{
    switch (type) {
        case DefectType::Scratch:    return applyScratch(depthMap, severity, scale);
        case DefectType::ShallowDent:return applyDent(depthMap, severity, scale);
        case DefectType::Crack:      return applyCrack(depthMap, severity, scale);
        case DefectType::SurfacePit: return applyPit(depthMap, severity, scale);
    }
    return depthMap.clone();
}

cv::Mat DefectGenerator::applyScratch(const cv::Mat& src, float severity, float scale) const
{
    cv::Mat result = src.clone();
    std::mt19937 rng(std::random_device{}());

    int x1 = std::uniform_int_distribution<>(0, src.cols)(rng);
    int y1 = std::uniform_int_distribution<>(0, src.rows)(rng);
    int x2 = std::uniform_int_distribution<>(0, src.cols)(rng);
    int y2 = std::uniform_int_distribution<>(0, src.rows)(rng);

    int thickness = std::max(1, static_cast<int>(scale));
    float depth   = -severity * 50.0f;  // depress the surface along the scratch

    cv::Mat mask = cv::Mat::zeros(src.size(), CV_8U);
    cv::line(mask, {x1, y1}, {x2, y2}, cv::Scalar(255), thickness);

    result.setTo(cv::Scalar(depth), mask);
    return result;
}

cv::Mat DefectGenerator::applyDent(const cv::Mat& src, float severity, float scale) const
{
    cv::Mat result = src.clone();
    std::mt19937 rng(std::random_device{}());

    int cx = std::uniform_int_distribution<>(0, src.cols)(rng);
    int cy = std::uniform_int_distribution<>(0, src.rows)(rng);
    int r  = std::max(5, static_cast<int>(20 * scale));

    // Gaussian bowl depression
    for (int y = std::max(0, cy - r); y < std::min(src.rows, cy + r); ++y) {
        for (int x = std::max(0, cx - r); x < std::min(src.cols, cx + r); ++x) {
            float dx = static_cast<float>(x - cx) / r;
            float dy = static_cast<float>(y - cy) / r;
            float d2 = dx * dx + dy * dy;
            if (d2 <= 1.0f) {
                float depress = -severity * 40.0f * std::exp(-3.0f * d2);
                result.at<float>(y, x) += depress;
            }
        }
    }
    return result;
}

cv::Mat DefectGenerator::applyCrack(const cv::Mat& src, float severity, float scale) const
{
    // Phase 2 placeholder — crack uses a more complex fractal path algorithm.
    // For now, approximate with multiple connected scratch segments.
    cv::Mat result = src.clone();
    std::mt19937 rng(std::random_device{}());

    int x = std::uniform_int_distribution<>(0, src.cols)(rng);
    int y = std::uniform_int_distribution<>(0, src.rows)(rng);

    int segments = 6 + static_cast<int>(severity * 10);
    for (int i = 0; i < segments; ++i) {
        int dx = std::uniform_int_distribution<>(-30, 30)(rng);
        int dy = std::uniform_int_distribution<>(-30, 30)(rng);
        int nx = std::clamp(x + dx, 0, src.cols - 1);
        int ny = std::clamp(y + dy, 0, src.rows - 1);

        cv::Mat mask = cv::Mat::zeros(src.size(), CV_8U);
        cv::line(mask, {x, y}, {nx, ny}, cv::Scalar(255), 1);
        result.setTo(cv::Scalar(-severity * 60.0f), mask);

        x = nx; y = ny;
    }
    return result;
}

cv::Mat DefectGenerator::applyPit(const cv::Mat& src, float severity, float scale) const
{
    cv::Mat result = src.clone();
    std::mt19937 rng(std::random_device{}());

    int cx = std::uniform_int_distribution<>(0, src.cols)(rng);
    int cy = std::uniform_int_distribution<>(0, src.rows)(rng);
    int r  = std::max(2, static_cast<int>(6 * scale));

    cv::circle(result, {cx, cy}, r, cv::Scalar(-severity * 80.0f), -1);
    return result;
}
