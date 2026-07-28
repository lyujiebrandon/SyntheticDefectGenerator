#include <gtest/gtest.h>
#include "core/defect_gen/DefectGenerator.h"
#include "core/defect_gen/DefectTypes.h"
#include <opencv2/core.hpp>

namespace {

// Depth map with a bright central region so buildProductMask finds a product.
cv::Mat makeDepthMap(int width = 128, int height = 128)
{
    cv::Mat depth(height, width, CV_32F, cv::Scalar(0.0f));
    cv::Mat center(depth, cv::Rect(width / 4, height / 4, width / 2, height / 2));
    center.setTo(cv::Scalar(0.8f));
    return depth;
}

cv::Mat makeReferenceImage(int width = 128, int height = 128)
{
    return cv::Mat(height, width, CV_8U, cv::Scalar(180));
}

} // namespace

TEST(DefectGenerator, EmptyDepthMapReturnsFalse)
{
    DefectGenerator gen;
    EXPECT_FALSE(gen.generate(cv::Mat{}, cv::Mat{}, {}));
    EXPECT_FALSE(gen.hasOutput());
}

TEST(DefectGenerator, NoEnabledTypesReturnsFalse)
{
    DefectGenerator gen;
    DefectGenerator::Params params;
    params.enableScratch = false;
    params.enableDent    = false;
    params.enableCrack   = false;
    params.enablePit     = false;
    EXPECT_FALSE(gen.generate(makeDepthMap(), makeReferenceImage(), params));
}

TEST(DefectGenerator, OutputCountMatchesDefectCount)
{
    DefectGenerator gen;
    DefectGenerator::Params params;
    params.defectCount = 5;
    ASSERT_TRUE(gen.generate(makeDepthMap(), makeReferenceImage(), params));
    EXPECT_EQ(static_cast<int>(gen.getOutputImages().size()), 5);
}

TEST(DefectGenerator, LabelsAndBoundsMatchImageCount)
{
    DefectGenerator gen;
    DefectGenerator::Params params;
    params.defectCount = 5;
    gen.generate(makeDepthMap(), makeReferenceImage(), params);
    EXPECT_EQ(gen.getOutputImages().size(), gen.getOutputLabels().size());
    EXPECT_EQ(gen.getOutputImages().size(), gen.getOutputBounds().size());
}

TEST(DefectGenerator, OutputImagesMatchInputSize)
{
    DefectGenerator gen;
    DefectGenerator::Params params;
    params.defectCount = 3;
    gen.generate(makeDepthMap(128, 64), makeReferenceImage(128, 64), params);
    for (const auto& img : gen.getOutputImages()) {
        EXPECT_EQ(img.rows, 64);
        EXPECT_EQ(img.cols, 128);
    }
}

TEST(DefectGenerator, FallsBackToDepthColormapWhenReferenceIsEmpty)
{
    DefectGenerator gen;
    DefectGenerator::Params params;
    params.defectCount = 2;
    EXPECT_TRUE(gen.generate(makeDepthMap(), cv::Mat{}, params));
    EXPECT_TRUE(gen.hasOutput());
}

TEST(DefectGenerator, ScratchOnlyProducesOnlyScratchLabels)
{
    DefectGenerator gen;
    DefectGenerator::Params params;
    params.defectCount   = 10;
    params.enableScratch = true;
    params.enableDent    = false;
    params.enableCrack   = false;
    params.enablePit     = false;
    gen.generate(makeDepthMap(), makeReferenceImage(), params);
    for (auto label : gen.getOutputLabels())
        EXPECT_EQ(label, DefectType::Scratch);
}

TEST(DefectGenerator, DentOnlyProducesOnlyDentLabels)
{
    DefectGenerator gen;
    DefectGenerator::Params params;
    params.defectCount   = 10;
    params.enableScratch = false;
    params.enableDent    = true;
    params.enableCrack   = false;
    params.enablePit     = false;
    gen.generate(makeDepthMap(), makeReferenceImage(), params);
    for (auto label : gen.getOutputLabels())
        EXPECT_EQ(label, DefectType::ShallowDent);
}

TEST(DefectGenerator, OutputImagesAreBGR)
{
    DefectGenerator gen;
    DefectGenerator::Params params;
    params.defectCount = 2;
    gen.generate(makeDepthMap(), makeReferenceImage(), params);
    for (const auto& img : gen.getOutputImages())
        EXPECT_EQ(img.channels(), 3);
}

TEST(DefectGenerator, HasOutputReturnsFalseBeforeGenerate)
{
    DefectGenerator gen;
    EXPECT_FALSE(gen.hasOutput());
}
