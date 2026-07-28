#include <gtest/gtest.h>
#include "core/image_registration/ImageRegistrator.h"
#include <opencv2/core.hpp>

namespace {

std::vector<cv::Mat> makeIdenticalStack(int n, int width = 64, int height = 64)
{
    cv::Mat base(height, width, CV_8U);
    cv::randu(base, cv::Scalar(0), cv::Scalar(255));
    return std::vector<cv::Mat>(n, base.clone());
}

} // namespace

TEST(ImageRegistrator, EmptyStackReturnsFalse)
{
    ImageRegistrator reg;
    std::vector<cv::Mat> empty;
    EXPECT_FALSE(reg.registerStack(empty, {}));
    EXPECT_FALSE(reg.hasRegisteredStack());
}

TEST(ImageRegistrator, SingleFrameReturnsFalse)
{
    ImageRegistrator reg;
    auto stack = makeIdenticalStack(1);
    EXPECT_FALSE(reg.registerStack(stack, {}));
    EXPECT_FALSE(reg.hasRegisteredStack());
}

TEST(ImageRegistrator, TwoFramesReturnsTrue)
{
    ImageRegistrator reg;
    auto stack = makeIdenticalStack(2);
    EXPECT_TRUE(reg.registerStack(stack, {}));
    EXPECT_TRUE(reg.hasRegisteredStack());
}

TEST(ImageRegistrator, FrameCountUnchangedAfterRegistration)
{
    ImageRegistrator reg;
    auto stack = makeIdenticalStack(5);
    reg.registerStack(stack, {});
    EXPECT_EQ(static_cast<int>(stack.size()), 5);
}

TEST(ImageRegistrator, FrameSizeUnchangedAfterRegistration)
{
    ImageRegistrator reg;
    auto stack = makeIdenticalStack(3, 80, 60);
    reg.registerStack(stack, {});
    for (const auto& frame : stack) {
        EXPECT_EQ(frame.rows, 60);
        EXPECT_EQ(frame.cols, 80);
    }
}

TEST(ImageRegistrator, IdenticalFramesAlignWithNearZeroResidual)
{
    // Aligning a stack where every frame is the same should produce near-identical
    // output (the warp for identical images should converge to identity).
    ImageRegistrator reg;
    cv::Mat base(64, 64, CV_8U);
    cv::randu(base, cv::Scalar(30), cv::Scalar(230));
    std::vector<cv::Mat> stack = { base.clone(), base.clone(), base.clone() };
    ASSERT_TRUE(reg.registerStack(stack, {}));

    for (const auto& frame : stack) {
        cv::Mat diff;
        cv::absdiff(frame, base, diff);
        double maxDiff;
        cv::minMaxLoc(diff, nullptr, &maxDiff);
        EXPECT_LT(maxDiff, 10.0); // allow minor bilinear interpolation rounding
    }
}

TEST(ImageRegistrator, AffineModelAlsoSucceeds)
{
    ImageRegistrator reg;
    auto stack = makeIdenticalStack(3);
    ImageRegistrator::Params params;
    params.motionModel = ImageRegistrator::MotionModel::Affine;
    EXPECT_TRUE(reg.registerStack(stack, params));
}
