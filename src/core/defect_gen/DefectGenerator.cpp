#include "DefectGenerator.h"

#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>

#include <random>
#include <QDir>

// ─── Helpers ──────────────────────────────────────────────────────────────────

static cv::Rect clampRect(cv::Rect r, int cols, int rows)
{
    return r & cv::Rect(0, 0, cols, rows);
}

// Expand a single-channel CV_32F factor map to match an image's channel count,
// then multiply in-place.  cv::multiply requires identical channel counts.
static void applyDarkFactor(cv::Mat& imgF, const cv::Mat& factor1ch)
{
    if (imgF.channels() == 1) {
        cv::multiply(imgF, factor1ch, imgF);
    } else {
        std::vector<cv::Mat> ch(imgF.channels(), factor1ch);
        cv::Mat factorN;
        cv::merge(ch, factorN);
        cv::multiply(imgF, factorN, imgF);
    }
}

// Darken image pixels where mask8U > 0 by a fixed fraction.
void DefectGenerator::darkenWithMask(cv::Mat& image, const cv::Mat& mask8U, float amount)
{
    cv::Mat maskF;
    mask8U.convertTo(maskF, CV_32F, 1.0f / 255.0f);

    // darkFactor = 1.0 where mask==0, (1 - amount) where mask==255
    cv::Mat factor = cv::Mat::ones(image.size(), CV_32F) - maskF * amount;
    cv::max(factor, 0.0f, factor);

    cv::Mat imgF;
    image.convertTo(imgF, CV_32F);
    applyDarkFactor(imgF, factor);
    imgF.convertTo(image, CV_8U);
}

// Darken image with a Gaussian profile centred at a point (smooth dent / pit).
void DefectGenerator::darkenGaussian(cv::Mat& image, cv::Point center, int radius,
                                      float peakAmount, float sigma)
{
    // Build a 1-channel dark factor map, then expand to match image channels.
    cv::Mat factor = cv::Mat::ones(image.size(), CV_32F);
    int r = radius;
    for (int y = std::max(0, center.y - r); y < std::min(image.rows, center.y + r); ++y) {
        for (int x = std::max(0, center.x - r); x < std::min(image.cols, center.x + r); ++x) {
            float dx = static_cast<float>(x - center.x) / r;
            float dy = static_cast<float>(y - center.y) / r;
            float d2 = dx * dx + dy * dy;
            if (d2 <= 1.0f)
                factor.at<float>(y, x) = 1.0f - peakAmount * std::exp(-sigma * d2);
        }
    }

    cv::Mat imgF;
    image.convertTo(imgF, CV_32F);
    applyDarkFactor(imgF, factor);
    imgF.convertTo(image, CV_8U);
}

// ─── Public ───────────────────────────────────────────────────────────────────

bool DefectGenerator::generate(const cv::Mat& depthMap,
                                const cv::Mat& referenceImage,
                                const Params& params,
                                ProgressCallback onProgress)
{
    if (depthMap.empty()) return false;

    m_outputImages.clear();
    m_outputLabels.clear();
    m_outputBounds.clear();

    // Build product mask from the depth map (determines WHERE defects can go)
    m_productMask = buildProductMask(depthMap);

    // Prepare the base image that defects will be rendered onto.
    // Prefer the all-in-focus reference; fall back to a jet-colourised depth map.
    if (!referenceImage.empty()) {
        if (referenceImage.channels() == 1)
            cv::cvtColor(referenceImage, m_referenceImage, cv::COLOR_GRAY2BGR);
        else
            m_referenceImage = referenceImage.clone();
    } else {
        cv::Mat norm;
        cv::normalize(depthMap, norm, 0, 255, cv::NORM_MINMAX, CV_8U);
        cv::applyColorMap(norm, m_referenceImage, cv::COLORMAP_JET);
    }

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
        auto [defected, bounds] = applyDefect(m_referenceImage, type,
                                              params.severity, params.scaleFactor);

        // Light bilateral pass: removes residual noise without blurring the
        // defect edges — bilateral preserves the sharp boundary between the
        // darkened defect region and the surrounding surface.
        cv::Mat cleaned;
        cv::bilateralFilter(defected, cleaned, 5, 35.0, 5.0);

        m_outputImages.push_back(std::move(cleaned));
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

        // Images are already CV_8U BGR — write directly without normalisation.
        cv::imwrite(path.toStdString(), m_outputImages[i]);

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
    cv::normalize(depthMap, depth8, 0, 255, cv::NORM_MINMAX, CV_8U);

    cv::Mat mask;
    cv::threshold(depth8, mask, 0, 255, cv::THRESH_BINARY | cv::THRESH_OTSU);

    int    total   = mask.rows * mask.cols;
    int    nonZero = cv::countNonZero(mask);
    double ratio   = static_cast<double>(nonZero) / total;

    if (ratio < 0.05) {
        cv::bitwise_not(mask, mask);
        ratio = 1.0 - ratio;
    }

    // If the mask mainly occupies the image border it is the background, not the
    // product — invert so the central object is selected.
    {
        const int bx = mask.cols / 10;
        const int by = mask.rows / 10;

        cv::Mat borderStrip = cv::Mat::zeros(mask.size(), CV_8U);
        cv::rectangle(borderStrip, {0, 0},
                      {mask.cols - 1, by - 1}, cv::Scalar(255), cv::FILLED);
        cv::rectangle(borderStrip, {0, mask.rows - by},
                      {mask.cols - 1, mask.rows - 1}, cv::Scalar(255), cv::FILLED);
        cv::rectangle(borderStrip, {0, 0},
                      {bx - 1, mask.rows - 1}, cv::Scalar(255), cv::FILLED);
        cv::rectangle(borderStrip, {mask.cols - bx, 0},
                      {mask.cols - 1, mask.rows - 1}, cv::Scalar(255), cv::FILLED);

        cv::Mat overlap;
        cv::bitwise_and(mask, borderStrip, overlap);
        double borderRatio = static_cast<double>(cv::countNonZero(overlap))
                           / static_cast<double>(cv::countNonZero(borderStrip) + 1);

        if (borderRatio > 0.65) {
            cv::bitwise_not(mask, mask);
            ratio = 1.0 - ratio;
        }
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

std::pair<cv::Mat, cv::Rect> DefectGenerator::applyDefect(const cv::Mat& refImage,
                                                           DefectType type,
                                                           float severity,
                                                           float scale) const
{
    switch (type) {
        case DefectType::Scratch:    return applyScratch(refImage, severity, scale);
        case DefectType::ShallowDent:return applyDent   (refImage, severity, scale);
        case DefectType::Crack:      return applyCrack  (refImage, severity, scale);
        case DefectType::SurfacePit: return applyPit    (refImage, severity, scale);
    }
    return { refImage.clone(), {} };
}

// ─── Individual defect types ──────────────────────────────────────────────────
// Each function renders a visually realistic defect on the reference image by
// darkening the affected area.  The product mask limits placement to the object.

std::pair<cv::Mat, cv::Rect> DefectGenerator::applyScratch(const cv::Mat& src,
                                                            float severity,
                                                            float scale) const
{
    cv::Mat result = src.clone();
    std::mt19937 rng(std::random_device{}());

    cv::Point p1 = samplePointInMask(rng);
    cv::Point p2 = samplePointInMask(rng);
    int thickness = std::max(1, static_cast<int>(scale));

    // Draw the scratch as a dark line (mask) then darken those pixels
    cv::Mat mask = cv::Mat::zeros(src.size(), CV_8U);
    cv::line(mask, p1, p2, cv::Scalar(255), thickness);
    cv::bitwise_and(mask, m_productMask, mask);

    // Slight blur softens the scratch edges for realism
    cv::GaussianBlur(mask, mask, cv::Size(3, 3), 0.8);

    darkenWithMask(result, mask, severity * 0.70f);

    int pad = thickness + 4;
    cv::Rect bounds(std::min(p1.x, p2.x) - pad, std::min(p1.y, p2.y) - pad,
                    std::abs(p2.x - p1.x) + 2 * pad, std::abs(p2.y - p1.y) + 2 * pad);
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

    if (!m_productMask.empty() && m_productMask.at<uchar>(center.y, center.x) == 0)
        return { result, {} };

    // Gaussian darkening — deepest at centre, fading toward edges
    darkenGaussian(result, center, r, severity * 0.55f, 2.5f);

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

    // Accumulate the full crack mask before darkening (so blur smooths the whole crack)
    cv::Mat crackMask = cv::Mat::zeros(src.size(), CV_8U);

    for (int i = 0; i < segments; ++i) {
        int dx = std::uniform_int_distribution<>(-30, 30)(rng);
        int dy = std::uniform_int_distribution<>(-30, 30)(rng);
        cv::Point next(std::clamp(cur.x + dx, 0, src.cols - 1),
                       std::clamp(cur.y + dy, 0, src.rows - 1));

        if (m_productMask.empty() || m_productMask.at<uchar>(next.y, next.x) > 0) {
            cv::line(crackMask, cur, next, cv::Scalar(255), 1);
            cur = next;
        }

        minX = std::min(minX, cur.x); minY = std::min(minY, cur.y);
        maxX = std::max(maxX, cur.x); maxY = std::max(maxY, cur.y);
    }

    cv::bitwise_and(crackMask, m_productMask, crackMask);
    cv::GaussianBlur(crackMask, crackMask, cv::Size(3, 3), 0.8);
    darkenWithMask(result, crackMask, severity * 0.75f);

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

    if (!m_productMask.empty() && m_productMask.at<uchar>(center.y, center.x) == 0)
        return { result, {} };

    // Tight Gaussian darkening — very dark centre, fast falloff
    darkenGaussian(result, center, r, severity * 0.80f, 4.0f);

    cv::Rect bounds(center.x - r - 4, center.y - r - 4, 2*(r+4), 2*(r+4));
    return { result, clampRect(bounds, src.cols, src.rows) };
}
