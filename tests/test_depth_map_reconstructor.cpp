#include <gtest/gtest.h>
#include "core/depth_map/DepthMapReconstructor.h"
#include <opencv2/core.hpp>

namespace {

std::vector<cv::Mat> makeRandomStack(int frames, int width = 64, int height = 64)
{
    std::vector<cv::Mat> stack;
    for (int i = 0; i < frames; ++i) {
        cv::Mat f(height, width, CV_8U);
        cv::randu(f, cv::Scalar(0), cv::Scalar(255));
        stack.push_back(f);
    }
    return stack;
}

} // namespace

TEST(DepthMapReconstructor, EmptyStackReturnsFalse)
{
    DepthMapReconstructor dmr;
    EXPECT_FALSE(dmr.reconstruct({}, {}));
    EXPECT_FALSE(dmr.hasDepthMap());
}

TEST(DepthMapReconstructor, ValidStackReturnsTrue)
{
    DepthMapReconstructor dmr;
    EXPECT_TRUE(dmr.reconstruct(makeRandomStack(3), {}));
    EXPECT_TRUE(dmr.hasDepthMap());
}

TEST(DepthMapReconstructor, DepthMapTypeIsCV32F)
{
    DepthMapReconstructor dmr;
    dmr.reconstruct(makeRandomStack(3), {});
    EXPECT_EQ(dmr.getDepthMap().type(), CV_32F);
}

TEST(DepthMapReconstructor, DepthMapNormalizedBetweenZeroAndOne)
{
    DepthMapReconstructor dmr;
    dmr.reconstruct(makeRandomStack(5), {});
    double minVal, maxVal;
    cv::minMaxLoc(dmr.getDepthMap(), &minVal, &maxVal);
    EXPECT_GE(minVal, 0.0);
    EXPECT_LE(maxVal, 1.0);
}

TEST(DepthMapReconstructor, DepthMapSizeMatchesInput)
{
    DepthMapReconstructor dmr;
    dmr.reconstruct(makeRandomStack(3, 80, 60), {});
    EXPECT_EQ(dmr.getDepthMap().rows, 60);
    EXPECT_EQ(dmr.getDepthMap().cols, 80);
}

TEST(DepthMapReconstructor, AllInFocusImageTypeIsCV8U)
{
    DepthMapReconstructor dmr;
    dmr.reconstruct(makeRandomStack(3), {});
    EXPECT_EQ(dmr.getAllInFocusImage().type(), CV_8U);
}

TEST(DepthMapReconstructor, AllInFocusImageSizeMatchesInput)
{
    DepthMapReconstructor dmr;
    dmr.reconstruct(makeRandomStack(3, 80, 60), {});
    EXPECT_EQ(dmr.getAllInFocusImage().rows, 60);
    EXPECT_EQ(dmr.getAllInFocusImage().cols, 80);
}

TEST(DepthMapReconstructor, FocalPowersNormalizeDepth)
{
    DepthMapReconstructor dmr;
    std::vector<float> powers = { -2.0f, 0.0f, 2.0f, 4.0f };
    ASSERT_TRUE(dmr.reconstruct(makeRandomStack(4), {}, powers));
    double minVal, maxVal;
    cv::minMaxLoc(dmr.getDepthMap(), &minVal, &maxVal);
    EXPECT_GE(minVal, 0.0);
    EXPECT_LE(maxVal, 1.0);
}

TEST(DepthMapReconstructor, MismatchedFocalPowerCountFallsBackToUniform)
{
    DepthMapReconstructor dmr;
    std::vector<float> powers = { -2.0f, 0.0f }; // wrong count for a 4-frame stack
    EXPECT_TRUE(dmr.reconstruct(makeRandomStack(4), {}, powers));
    double minVal, maxVal;
    cv::minMaxLoc(dmr.getDepthMap(), &minVal, &maxVal);
    EXPECT_GE(minVal, 0.0);
    EXPECT_LE(maxVal, 1.0);
}

TEST(DepthMapReconstructor, SubframeInterpolationTriggersForThreePlusFrames)
{
    // With ≥3 frames the parabolic sub-frame path runs; map must still be valid.
    DepthMapReconstructor dmr;
    EXPECT_TRUE(dmr.reconstruct(makeRandomStack(7), {}));
    EXPECT_EQ(dmr.getDepthMap().type(), CV_32F);
}
