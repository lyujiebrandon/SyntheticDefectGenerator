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

    // Use the middle frame as the reference (statistically the sharpest region)
    int refIdx = static_cast<int>(stack.size()) / 2;

    cv::Mat reference;
    cv::GaussianBlur(stack[refIdx], reference,
                     {params.gaussianBlurKernel, params.gaussianBlurKernel}, 0);
    reference.convertTo(reference, CV_32F);

    int total = static_cast<int>(stack.size());
    for (int i = 0; i < total; ++i) {
        if (i == refIdx) {
            if (onProgress)
                onProgress((i + 1) * 100 / total,
                           QString("Frame %1 / %2 — reference (skipped)").arg(i + 1).arg(total));
            continue;
        }

        cv::Mat aligned = alignFrame(stack[refIdx], stack[i], params);
        if (!aligned.empty())
            stack[i] = aligned;

        if (onProgress) {
            int pct = (i + 1) * 100 / total;
            onProgress(pct, QString("Registered frame %1 / %2").arg(i + 1).arg(total));
        }
    }

    m_registered = true;
    return true;
}

cv::Mat ImageRegistrator::alignFrame(const cv::Mat& reference,
                                      const cv::Mat& frame,
                                      const Params& params)
{
    // Pre-blur to improve ECC convergence on noisy industrial images
    cv::Mat refBlur, frameBlur;
    cv::GaussianBlur(reference, refBlur,
                     {params.gaussianBlurKernel, params.gaussianBlurKernel}, 0);
    cv::GaussianBlur(frame, frameBlur,
                     {params.gaussianBlurKernel, params.gaussianBlurKernel}, 0);

    cv::Mat refF, frameF;
    refBlur.convertTo(refF,   CV_32F);
    frameBlur.convertTo(frameF, CV_32F);

    int motionType = (params.motionModel == MotionModel::Affine)
                     ? cv::MOTION_AFFINE
                     : cv::MOTION_EUCLIDEAN;

    // Identity initialisation for warp matrix
    cv::Mat warpMatrix;
    if (motionType == cv::MOTION_AFFINE)
        warpMatrix = cv::Mat::eye(2, 3, CV_32F);
    else
        warpMatrix = cv::Mat::eye(2, 3, CV_32F);

    cv::TermCriteria criteria(cv::TermCriteria::COUNT | cv::TermCriteria::EPS,
                              params.eccIterations, params.eccEpsilon);

    try {
        cv::findTransformECC(refF, frameF, warpMatrix, motionType, criteria);
    } catch (const cv::Exception& e) {
        qWarning() << "ImageRegistrator: ECC failed —" << e.what();
        return frame.clone(); // return unaligned rather than empty
    }

    cv::Mat aligned;
    cv::warpAffine(frame, aligned, warpMatrix, frame.size(),
                   cv::INTER_LINEAR | cv::WARP_INVERSE_MAP);
    return aligned;
}
