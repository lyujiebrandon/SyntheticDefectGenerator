#include "DepthMapReconstructor.h"

#include <algorithm>
#include <opencv2/imgproc.hpp>

cv::Mat DepthMapReconstructor::computeSharpnessMap(const cv::Mat& frame, const Params& params) const
{
    cv::Mat gray;
    if (frame.channels() > 1)
        cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);
    else
        gray = frame;

    // Pre-blur suppresses sensor noise before Laplacian — without this, noise
    // spikes on dark low-texture surfaces (matte plastic) produce false sharpness
    // wins that are indistinguishable from real focus edges.
    cv::Mat denoised;
    cv::GaussianBlur(gray, denoised, cv::Size(3, 3), 0.8);

    cv::Mat laplacian;
    cv::Laplacian(denoised, laplacian, CV_32F, params.kernelSize);

    // Square then blur locally so blurry/background pixels stay consistently
    // low — without the blur, noise peaks cause random frame wins per pixel.
    cv::Mat sharpness;
    cv::multiply(laplacian, laplacian, sharpness);
    int blurKernel = params.kernelSize * 2 + 1;
    cv::GaussianBlur(sharpness, sharpness, cv::Size(blurKernel, blurKernel), 0);
    return sharpness;
}

bool DepthMapReconstructor::reconstruct(const std::vector<cv::Mat>& stack,
                                         const Params& params,
                                         const std::vector<float>& focalPowers,
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
        cv::Mat sharpness = computeSharpnessMap(stack[i], params);

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

    // ── Confidence mask — suppress background and low-texture regions ─────────
    // ── Build inner-region mask ───────────────────────────────────────────────
    // ECC registration warps frames and leaves hard black edges at the image
    // border. Those edges produce extremely high Laplacian² values that inflate
    // maxSharpness and blow the adaptive threshold sky-high, masking every real
    // object pixel. Excluding a thin border (~5 %) from the peak search prevents
    // this, and force-masking it keeps warp artifacts out of the depth map.
    const int bx = std::max(2, cols / 20);
    const int by = std::max(2, rows / 20);
    cv::Mat innerMask = cv::Mat::zeros(rows, cols, CV_8U);
    cv::rectangle(innerMask, cv::Point(bx, by),
                  cv::Point(cols - bx - 1, rows - by - 1),
                  cv::Scalar(255), cv::FILLED);

    double maxSharpness;
    cv::minMaxLoc(bestSharp, nullptr, &maxSharpness, nullptr, nullptr, innerMask);
    float adaptiveMin = std::max(params.sharpnessMinimum,
                                 static_cast<float>(maxSharpness) * params.sharpnessRatio);
    cv::Mat lowConfidence;
    cv::compare(bestSharp, cv::Scalar(adaptiveMin), lowConfidence, cv::CMP_LT);

    // Force-mask the border region regardless of sharpness values
    cv::Mat borderMask;
    cv::bitwise_not(innerMask, borderMask);
    lowConfidence.setTo(255, borderMask);

    m_depthMap.setTo(0.0f, lowConfidence);

    // ── Focal depth normalization ─────────────────────────────────────────────
    // Convert fractional frame indices [0, N-1] to normalized depth [0.0, 1.0].
    // When focal powers are provided (from metadata.csv), each pixel is mapped to
    // the interpolated diopter value at its peak-sharpness frame, then normalized
    // over the hardware range [focalMin, focalMax].
    const bool useFocalPowers = !focalPowers.empty() &&
                                 static_cast<int>(focalPowers.size()) == N;
    if (useFocalPowers) {
        // Use the actual range of the loaded data rather than the Params defaults
        // so any subset of the diopter range normalises to [0.0, 1.0] correctly.
        float fp0 = *std::min_element(focalPowers.begin(), focalPowers.end());
        float fp1 = *std::max_element(focalPowers.begin(), focalPowers.end());
        if (fp0 >= fp1) { fp0 = params.focalMin; fp1 = params.focalMax; }
        float focalRange = fp1 - fp0;
        for (int y = 0; y < rows; ++y) {
            float* row = m_depthMap.ptr<float>(y);
            for (int x = 0; x < cols; ++x) {
                if (row[x] == 0.0f) continue;
                int   i0 = std::clamp(static_cast<int>(row[x]), 0, N - 1);
                int   i1 = std::min(i0 + 1, N - 1);
                float t  = row[x] - static_cast<float>(i0);
                float fp = focalPowers[i0] + t * (focalPowers[i1] - focalPowers[i0]);
                row[x]   = std::clamp((fp - fp0) / focalRange, 0.0f, 1.0f);
            }
        }
    } else {
        // No focal metadata: uniformly map frame indices → [0.0, 1.0]
        cv::normalize(m_depthMap, m_depthMap, 0.0f, 1.0f, cv::NORM_MINMAX);
    }

    // ── Pre-bilateral median — remove speckle before bilateral spreads it ─────
    // Scale to [0, 255] for 8-bit median (no NORM_MINMAX so calibration is kept),
    // then scale back to [0.0, 1.0].
    cv::Mat depthScaled;
    m_depthMap.convertTo(depthScaled, CV_8U, 255.0f);
    cv::medianBlur(depthScaled, depthScaled, 9);
    depthScaled.convertTo(m_depthMap, CV_32F, 1.0f / 255.0f);
    // Re-apply mask: median can pull zeros into the object boundary.
    m_depthMap.setTo(0.0f, lowConfidence);

    // ── Edge-preserving bilateral filter ─────────────────────────────────────
    // sigmaColor=60 is tuned for 0–255 range, so scale up before filtering.
    cv::Mat depthForBilat;
    m_depthMap.convertTo(depthForBilat, CV_32F, 255.0f);
    cv::Mat filtered;
    cv::bilateralFilter(depthForBilat, filtered, 15, 60.0, 15.0);
    filtered.convertTo(m_depthMap, CV_32F, 1.0f / 255.0f);

    // ── Post-bilateral median — remove residual speckle ──────────────────────
    cv::Mat temp;
    m_depthMap.convertTo(temp, CV_8U, 255.0f);
    cv::medianBlur(temp, temp, 7);
    temp.convertTo(m_depthMap, CV_32F, 1.0f / 255.0f);

    // m_depthMap is now CV_32F in [0.0, 1.0] — ready for DefectGenerator.
    return true;
}
