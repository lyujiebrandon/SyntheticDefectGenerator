#include "ImageRegistrator.h"

#include <opencv2/imgproc.hpp>
#include <opencv2/video/tracking.hpp>
#include <QDebug>

bool ImageRegistrator::registerStack(std::vector<cv::Mat>& stack,
                                      const Params& params,
                                      ProgressCallback onProgress)
{
    m_registered = false;
    if (stack.size() < 2) return false;

    int refIdx = static_cast<int>(stack.size()) / 2;

    // Pre-process the reference frame once — blurred float at full and ¼ scale.
    // The old code computed this here but then passed the raw frame to alignFrame,
    // so alignFrame re-did it for every single frame in the stack.
    cv::Mat refBlur;
    cv::GaussianBlur(stack[refIdx], refBlur,
                     {params.gaussianBlurKernel, params.gaussianBlurKernel}, 0);

    cv::Mat refFull;
    refBlur.convertTo(refFull, CV_32F);

    cv::Mat refSmall;
    cv::resize(refFull, refSmall, cv::Size(), 0.25, 0.25, cv::INTER_AREA);

    int total = static_cast<int>(stack.size());
    for (int i = 0; i < total; ++i) {
        if (i == refIdx) {
            if (onProgress)
                onProgress((i + 1) * 100 / total,
                           QString("Frame %1 / %2 — reference (skipped)").arg(i + 1).arg(total));
            continue;
        }

        cv::Mat aligned = alignFrame(refFull, refSmall, stack[i], params);
        if (!aligned.empty())
            stack[i] = aligned;

        if (onProgress)
            onProgress((i + 1) * 100 / total,
                       QString("Registered frame %1 / %2").arg(i + 1).arg(total));
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

    // ── Level 1: coarse alignment at ¼ resolution ─────────────────────────────
    // Small images are 16× cheaper per ECC iteration. Running the full iteration
    // budget here gives a good initial warp for almost no cost, so the full-res
    // pass only needs a handful of refinement steps to converge.
    cv::TermCriteria coarse(cv::TermCriteria::COUNT | cv::TermCriteria::EPS,
                            params.eccIterations, params.eccEpsilon * 10.0);
    try {
        cv::findTransformECC(refSmall, frameSmall, warpMatrix, motionType, coarse);
    } catch (...) {
        // coarse pass failed — keep identity, full-res pass will still try
    }

    // Scale translation components back to full resolution (linear/rotation
    // components are scale-invariant for Euclidean and Affine models)
    warpMatrix.at<float>(0, 2) /= 0.25f;
    warpMatrix.at<float>(1, 2) /= 0.25f;

    // ── Level 2: fine refinement at full resolution ────────────────────────────
    // With a good initial warp the algorithm converges in very few iterations.
    int fineIter = std::max(5, params.eccIterations / 5);
    cv::TermCriteria fine(cv::TermCriteria::COUNT | cv::TermCriteria::EPS,
                          fineIter, params.eccEpsilon);
    try {
        cv::findTransformECC(refFull, frameFull, warpMatrix, motionType, fine);
    } catch (const cv::Exception& e) {
        qWarning() << "ImageRegistrator: ECC fine pass failed —" << e.what();
        // Return the coarse-corrected warp rather than the original frame
    }

    cv::Mat aligned;
    cv::warpAffine(frame, aligned, warpMatrix, frame.size(),
                   cv::INTER_LINEAR | cv::WARP_INVERSE_MAP);
    return aligned;
}
