#include "DefectGenerator.h"

#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>

#include <random>
#include <QDir>

// ─── Helpers ──────────────────────────────────────────────────────────────────

// Clamp a Rect inside the image bounds
static cv::Rect clampRect(cv::Rect r, int cols, int rows)
{
    return r & cv::Rect(0, 0, cols, rows);
}

// ─── Public ───────────────────────────────────────────────────────────────────

bool DefectGenerator::generate(const cv::Mat& depthMap,
                                const Params& params,
                                ProgressCallback onProgress)
{
    if (depthMap.empty()) return false;

    m_outputImages.clear();
    m_outputLabels.clear();
    m_outputBounds.clear();

    m_productMask = buildProductMask(depthMap);

    std::vector<DefectType> enabledTypes;
    if (params.enableScratch) enabledTypes.push_back(DefectType::Scratch);
    if (params.enableDent)    enabledTypes.push_back(DefectType::ShallowDent);
    if (params.enableCrack)   enabledTypes.push_back(DefectType::Crack);
    if (params.enablePit)     enabledTypes.push_back(DefectType::SurfacePit);

    if (enabledTypes.empty()) return false;

    std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<int> typeDist(0, static_cast<int>(enabledTypes.size()) - 1);

    for (int i = 0; i < params.defectCount; ++i) {
        DefectType type = enabledTypes[typeDist(rng)];
        auto [defected, bounds] = applyDefect(depthMap, type, params.severity, params.scaleFactor);

        m_outputImages.push_back(std::move(defected));
        m_outputLabels.push_back(type);
        m_outputBounds.push_back(bounds);

        if (onProgress) {
            int pct = (i + 1) * 100 / params.defectCount;
            onProgress(pct, QString("Generated image %1 / %2").arg(i + 1).arg(params.defectCount));
        }
    }

    return !m_outputImages.empty();
}

bool DefectGenerator::exportDataset(const QString& outputDir, ProgressCallback onProgress) const
{
    if (m_outputImages.empty()) return false;

    auto labelToFolder = [](DefectType t) -> QString {
        switch (t) {
            case DefectType::Scratch:    return "scratch";
            case DefectType::ShallowDent:return "dent";
            case DefectType::Crack:      return "crack";
            case DefectType::SurfacePit: return "pit";
        }
        return "unknown";
    };

    for (const auto& label : m_outputLabels)
        QDir().mkpath(outputDir + "/" + labelToFolder(label));

    int total = static_cast<int>(m_outputImages.size());
    for (int i = 0; i < total; ++i) {
        QString folder = labelToFolder(m_outputLabels[i]);
        QString path   = QString("%1/%2/img_%3.png").arg(outputDir, folder).arg(i, 5, 10, QChar('0'));

        cv::Mat save8;
        cv::normalize(m_outputImages[i], save8, 0, 255, cv::NORM_MINMAX);
        save8.convertTo(save8, CV_8U);
        cv::imwrite(path.toStdString(), save8);

        if (onProgress) {
            int pct = (i + 1) * 100 / total;
            onProgress(pct, QString("Saved %1 / %2").arg(i + 1).arg(total));
        }
    }
    return true;
}

// ─── Product Mask ─────────────────────────────────────────────────────────────

cv::Mat DefectGenerator::buildProductMask(const cv::Mat& depthMap) const
{
    cv::Mat depth8;
    depthMap.convertTo(depth8, CV_8U);

    cv::Mat mask;
    cv::threshold(depth8, mask, 0, 255, cv::THRESH_BINARY | cv::THRESH_OTSU);

    int    total   = mask.rows * mask.cols;
    int    nonZero = cv::countNonZero(mask);
    double ratio   = static_cast<double>(nonZero) / total;

    if (ratio < 0.05) {
        cv::bitwise_not(mask, mask);
        ratio = 1.0 - ratio;
    }

    if (ratio > 0.90) {
        int mx = depthMap.cols / 10;
        int my = depthMap.rows / 10;
        mask = cv::Mat::zeros(depthMap.size(), CV_8U);
        cv::rectangle(mask, cv::Point(mx, my),
                      cv::Point(depthMap.cols - mx - 1, depthMap.rows - my - 1),
                      cv::Scalar(255), -1);
        return mask;
    }

    cv::Mat closeK = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(13, 13));
    cv::Mat erodeK = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(9,  9));
    cv::morphologyEx(mask, mask, cv::MORPH_CLOSE, closeK);
    cv::erode(mask, mask, erodeK);
    return mask;
}

cv::Point DefectGenerator::samplePointInMask(std::mt19937& rng) const
{
    if (m_productMask.empty()) return cv::Point(0, 0);

    std::uniform_int_distribution<> xDist(0, m_productMask.cols - 1);
    std::uniform_int_distribution<> yDist(0, m_productMask.rows - 1);

    for (int attempt = 0; attempt < 1000; ++attempt) {
        cv::Point p(xDist(rng), yDist(rng));
        if (m_productMask.at<uchar>(p.y, p.x) > 0)
            return p;
    }
    return cv::Point(m_productMask.cols / 2, m_productMask.rows / 2);
}

// ─── Defect dispatch ──────────────────────────────────────────────────────────

std::pair<cv::Mat, cv::Rect> DefectGenerator::applyDefect(const cv::Mat& depthMap,
                                                           DefectType type,
                                                           float severity,
                                                           float scale) const
{
    switch (type) {
        case DefectType::Scratch:    return applyScratch(depthMap, severity, scale);
        case DefectType::ShallowDent:return applyDent   (depthMap, severity, scale);
        case DefectType::Crack:      return applyCrack  (depthMap, severity, scale);
        case DefectType::SurfacePit: return applyPit    (depthMap, severity, scale);
    }
    return { depthMap.clone(), {} };
}

// ─── Individual defect types ──────────────────────────────────────────────────

std::pair<cv::Mat, cv::Rect> DefectGenerator::applyScratch(const cv::Mat& src,
                                                            float severity,
                                                            float scale) const
{
    cv::Mat result = src.clone();
    std::mt19937 rng(std::random_device{}());

    cv::Point p1 = samplePointInMask(rng);
    cv::Point p2 = samplePointInMask(rng);
    int thickness = std::max(1, static_cast<int>(scale));

    cv::Mat mask = cv::Mat::zeros(src.size(), CV_8U);
    cv::line(mask, p1, p2, cv::Scalar(255), thickness);
    cv::bitwise_and(mask, m_productMask, mask);
    result.setTo(cv::Scalar(-severity * 50.0f), mask);

    int pad = thickness + 4;
    cv::Rect bounds(std::min(p1.x, p2.x) - pad,
                    std::min(p1.y, p2.y) - pad,
                    std::abs(p2.x - p1.x) + 2 * pad,
                    std::abs(p2.y - p1.y) + 2 * pad);

    return { result, clampRect(bounds, src.cols, src.rows) };
}

std::pair<cv::Mat, cv::Rect> DefectGenerator::applyDent(const cv::Mat& src,
                                                         float severity,
                                                         float scale) const
{
    cv::Mat result = src.clone();
    std::mt19937 rng(std::random_device{}());

    cv::Point center = samplePointInMask(rng);
    int r = std::max(5, static_cast<int>(20 * scale));

    for (int y = std::max(0, center.y - r); y < std::min(src.rows, center.y + r); ++y) {
        for (int x = std::max(0, center.x - r); x < std::min(src.cols, center.x + r); ++x) {
            if (!m_productMask.empty() && m_productMask.at<uchar>(y, x) == 0) continue;
            float dx = static_cast<float>(x - center.x) / r;
            float dy = static_cast<float>(y - center.y) / r;
            float d2 = dx * dx + dy * dy;
            if (d2 <= 1.0f)
                result.at<float>(y, x) += -severity * 40.0f * std::exp(-3.0f * d2);
        }
    }

    cv::Rect bounds(center.x - r - 4, center.y - r - 4, 2*(r+4), 2*(r+4));
    return { result, clampRect(bounds, src.cols, src.rows) };
}

std::pair<cv::Mat, cv::Rect> DefectGenerator::applyCrack(const cv::Mat& src,
                                                          float severity,
                                                          float scale) const
{
    cv::Mat result = src.clone();
    std::mt19937 rng(std::random_device{}());

    cv::Point cur = samplePointInMask(rng);
    int minX = cur.x, minY = cur.y, maxX = cur.x, maxY = cur.y;

    int segments = 6 + static_cast<int>(severity * 10);
    for (int i = 0; i < segments; ++i) {
        int dx = std::uniform_int_distribution<>(-30, 30)(rng);
        int dy = std::uniform_int_distribution<>(-30, 30)(rng);
        cv::Point next(std::clamp(cur.x + dx, 0, src.cols - 1),
                       std::clamp(cur.y + dy, 0, src.rows - 1));

        if (m_productMask.empty() || m_productMask.at<uchar>(next.y, next.x) > 0) {
            cv::Mat mask = cv::Mat::zeros(src.size(), CV_8U);
            cv::line(mask, cur, next, cv::Scalar(255), 1);
            cv::bitwise_and(mask, m_productMask, mask);
            result.setTo(cv::Scalar(-severity * 60.0f), mask);
            cur = next;
        }

        minX = std::min(minX, cur.x); minY = std::min(minY, cur.y);
        maxX = std::max(maxX, cur.x); maxY = std::max(maxY, cur.y);
    }

    cv::Rect bounds(minX - 6, minY - 6, maxX - minX + 12, maxY - minY + 12);
    return { result, clampRect(bounds, src.cols, src.rows) };
}

std::pair<cv::Mat, cv::Rect> DefectGenerator::applyPit(const cv::Mat& src,
                                                        float severity,
                                                        float scale) const
{
    cv::Mat result = src.clone();
    std::mt19937 rng(std::random_device{}());

    cv::Point center = samplePointInMask(rng);
    int r = std::max(2, static_cast<int>(6 * scale));

    cv::Mat pitMask = cv::Mat::zeros(src.size(), CV_8U);
    cv::circle(pitMask, center, r, cv::Scalar(255), -1);
    if (!m_productMask.empty())
        cv::bitwise_and(pitMask, m_productMask, pitMask);
    result.setTo(cv::Scalar(-severity * 80.0f), pitMask);

    cv::Rect bounds(center.x - r - 4, center.y - r - 4, 2*(r+4), 2*(r+4));
    return { result, clampRect(bounds, src.cols, src.rows) };
}
