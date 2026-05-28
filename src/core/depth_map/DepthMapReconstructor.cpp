#include "DepthMapReconstructor.h"

#include <opencv2/imgproc.hpp>

cv::Mat DepthMapReconstructor::computeSharpnessMap(const cv::Mat& frame, int kernelSize) const
{
    cv::Mat gray;
    if (frame.channels() > 1)
        cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);
    else
        gray = frame;

    cv::Mat laplacian;
    cv::Laplacian(gray, laplacian, CV_32F, kernelSize);

    // Square then blur locally so blurry/background pixels stay consistently
    // low — without the blur, noise peaks cause random frame wins per pixel.
    cv::Mat sharpness;
    cv::multiply(laplacian, laplacian, sharpness);
    int blurKernel = kernelSize * 2 + 1;
    cv::GaussianBlur(sharpness, sharpness, cv::Size(blurKernel, blurKernel), 0);
    return sharpness;
}

bool DepthMapReconstructor::reconstruct(const std::vector<cv::Mat>& stack,
                                         const Params& params,
                                         ProgressCallback onProgress)
{
    if (stack.empty()) return false;

    const int rows = stack[0].rows;
    const int cols = stack[0].cols;
    const int N    = static_cast<int>(stack.size());

    cv::Mat bestSharp  = cv::Mat::zeros(rows, cols, CV_32F);
    cv::Mat bestIdx    = cv::Mat::zeros(rows, cols, CV_32F);
    // Sharpness of the frame just before and just after each pixel's best frame,
    // needed for parabolic sub-frame interpolation.
    cv::Mat prevAtBest = cv::Mat::zeros(rows, cols, CV_32F);
    cv::Mat nextAtBest = cv::Mat::zeros(rows, cols, CV_32F);
    cv::Mat prevSharp;

    for (int i = 0; i < N; ++i) {
        cv::Mat sharpness = computeSharpnessMap(stack[i], params.kernelSize);

        // For pixels whose current best is frame i-1, this frame is their "next".
        // Must be done before updating the winner so bestIdx still holds i-1.
        if (i > 0) {
            cv::Mat isNext;
            cv::compare(bestIdx, float(i - 1), isNext, cv::CMP_EQ);
            sharpness.copyTo(nextAtBest, isNext);
        }

        // Pixels where this frame beats the current best
        cv::Mat isBetter;
        cv::compare(sharpness, bestSharp, isBetter, cv::CMP_GT);

        // Record the previous frame's sharpness for newly-won pixels
        if (!prevSharp.empty())
            prevSharp.copyTo(prevAtBest, isBetter);
        else
            prevAtBest.setTo(0.0f, isBetter);

        bestIdx.setTo(cv::Scalar(float(i)), isBetter);
        cv::max(bestSharp, sharpness, bestSharp);
        // Reset next for newly-won pixels — will be filled when frame i+1 arrives
        nextAtBest.setTo(0.0f, isBetter);

        prevSharp = sharpness.clone();

        if (onProgress)
            onProgress((i + 1) * 100 / N,
                       QString("Processing frame %1 / %2").arg(i + 1).arg(N));
    }

    // ── Parabolic sub-frame interpolation ────────────────────────────────────
    // Winner-takes-all assigns integer depth values 0…N-1.  Fitting a parabola
    // through (prev, best, next) sharpness gives a fractional peak position,
    // turning hard integer steps into a smooth continuous depth surface.
    //
    // offset = (prev - next) / (2 * (prev - 2*best + next))
    // depth  = bestIdx + offset   (clamped to ±1 frame for safety)
    m_depthMap = bestIdx.clone();

    if (N >= 3) {
        cv::Mat denom = 2.0f * (prevAtBest - 2.0f * bestSharp + nextAtBest);
        cv::Mat numer = prevAtBest - nextAtBest;

        cv::Mat offset;
        cv::divide(numer, denom, offset);  // Inf where denom≈0
        cv::patchNaNs(offset, 0.0);        // NaN → 0

        // Clamp Inf and large values to ±1 (one frame either side of the peak)
        cv::min(offset, 1.0, offset);
        cv::max(offset, -1.0, offset);

        // Only apply to interior frames — first and last have no prev/next
        cv::Mat gt0, ltNm1, isInterior, isBorder;
        cv::compare(bestIdx, 0.0f,        gt0,   cv::CMP_GT);
        cv::compare(bestIdx, float(N - 1), ltNm1, cv::CMP_LT);
        cv::bitwise_and(gt0, ltNm1, isInterior);
        cv::bitwise_not(isInterior, isBorder);
        offset.setTo(0.0f, isBorder);

        m_depthMap = bestIdx + offset;
    }

    // ── Edge-preserving bilateral filter ─────────────────────────────────────
    // Gaussian blur smooths depth edges; bilateral preserves them while still
    // removing speckle noise in flat/low-texture regions.
    cv::Mat depthNorm;
    cv::normalize(m_depthMap, depthNorm, 0.0f, 255.0f, cv::NORM_MINMAX, CV_32F);
    // sigmaColor=15: treats depth values within 15/255 of the range as the same
    // surface — preserves boundaries while averaging within flat regions.
    cv::bilateralFilter(depthNorm, m_depthMap, 9, 15.0, 9.0);

    cv::normalize(m_depthMap, m_depthMap, 0, 255, cv::NORM_MINMAX);
    return true;
}
