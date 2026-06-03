#include "ImageRegistrator.h"

#include <opencv2/imgproc.hpp>
#include <opencv2/video/tracking.hpp>
#include <QDebug>
#include <QThread>

#include <future>
#include <vector>
#include <algorithm>

bool ImageRegistrator::registerStack(std::vector<cv::Mat>& stack,
                                      const Params& params,
                                      ProgressCallback onProgress)
{
    m_registered = false;
    if (stack.size() < 2) return false;

    const int refIdx    = static_cast<int>(stack.size()) / 2;
    const int total     = static_cast<int>(stack.size());
    // Limit concurrency to physical core count so the system stays responsive.
    const int batchSize = std::max(1, QThread::idealThreadCount());

    // Pre-process the reference frame once — blurred float at full and ¼ scale.
    cv::Mat refBlur;
    cv::GaussianBlur(stack[refIdx], refBlur,
                     {params.gaussianBlurKernel, params.gaussianBlurKernel}, 0);
    cv::Mat refFull;
    refBlur.convertTo(refFull, CV_32F);
    cv::Mat refSmall;
    cv::resize(refFull, refSmall, cv::Size(), 0.25, 0.25, cv::INTER_AREA);

    // ── Batched parallel registration ─────────────────────────────────────────
    // Process frames in batches of batchSize.  Within each batch all frames run
    // concurrently (one thread each); the next batch starts only after the
    // current batch is fully collected.  This caps live threads at the core
    // count, preventing the thread-storm that makes the system lag.
    for (int batchStart = 0; batchStart < total; batchStart += batchSize) {
        const int batchEnd = std::min(batchStart + batchSize, total);
        const int n        = batchEnd - batchStart;

        // Launch this batch
        std::vector<std::future<cv::Mat>> futures(n);
        for (int j = 0; j < n; ++j) {
            int i = batchStart + j;
            if (i == refIdx) continue;
            futures[j] = std::async(std::launch::async,
                [this, frame = stack[i], &refFull, &refSmall, &params]() mutable {
                    return alignFrame(refFull, refSmall, frame, params);
                });
        }

        // Collect this batch and report progress
        // (progress callback calls QApplication::processEvents() → UI stays live)
        for (int j = 0; j < n; ++j) {
            int i = batchStart + j;
            if (i == refIdx) {
                if (onProgress)
                    onProgress((i + 1) * 100 / total,
                               QString("Frame %1 / %2 — reference (skipped)").arg(i + 1).arg(total));
                continue;
            }
            cv::Mat aligned = futures[j].get();
            if (!aligned.empty())
                stack[i] = aligned;
            if (onProgress)
                onProgress((i + 1) * 100 / total,
                           QString("Registered frame %1 / %2").arg(i + 1).arg(total));
        }
    }

    m_registered = true;
    return true;
}

cv::Mat ImageRegistrator::alignFrame(const cv::Mat& refFull,
                                      const cv::Mat& refSmall,
                                      const cv::Mat& frame,
                                      const Params& params)
{
    // Blur and convert the moving frame
    cv::Mat frameBlur;
    cv::GaussianBlur(frame, frameBlur,
                     {params.gaussianBlurKernel, params.gaussianBlurKernel}, 0);
    cv::Mat frameFull;
    frameBlur.convertTo(frameFull, CV_32F);
    cv::Mat frameSmall;
    cv::resize(frameFull, frameSmall, cv::Size(), 0.25, 0.25, cv::INTER_AREA);

    int motionType = (params.motionModel == MotionModel::Affine)
                     ? cv::MOTION_AFFINE
                     : cv::MOTION_EUCLIDEAN;

    cv::Mat warpMatrix = cv::Mat::eye(2, 3, CV_32F);

    // ── Level 1: phase correlation — O(N log N) translation estimate ──────────
    // Replaces the old 50-iteration coarse ECC pass.  Gives an accurate
    // translation starting point so the full-res ECC needs very few iterations.
    try {
        cv::Point2d shift = cv::phaseCorrelate(refSmall, frameSmall);
        warpMatrix.at<float>(0, 2) = static_cast<float>(shift.x / 0.25);
        warpMatrix.at<float>(1, 2) = static_cast<float>(shift.y / 0.25);
    } catch (...) {
        // Leave warpMatrix as identity if phaseCorrelate fails
    }

    // ── Level 2: ECC fine refinement at full resolution ────────────────────────
    int fineIter = std::max(5, params.eccIterations / 3);
    cv::TermCriteria fine(cv::TermCriteria::COUNT | cv::TermCriteria::EPS,
                          fineIter, params.eccEpsilon);
    try {
        cv::findTransformECC(refFull, frameFull, warpMatrix, motionType, fine);
    } catch (const cv::Exception& e) {
        qWarning() << "ImageRegistrator: ECC fine pass failed —" << e.what();
    }

    cv::Mat aligned;
    cv::warpAffine(frame, aligned, warpMatrix, frame.size(),
                   cv::INTER_LINEAR | cv::WARP_INVERSE_MAP);
    return aligned;
}
