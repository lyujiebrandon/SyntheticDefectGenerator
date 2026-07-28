#include <gtest/gtest.h>
#include "core/focal_stack/FocalStackProcessor.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <opencv2/imgcodecs.hpp>

namespace {

bool writePng(const QString& path, int width = 32, int height = 32)
{
    cv::Mat img(height, width, CV_8U, cv::Scalar(128));
    return cv::imwrite(path.toStdString(), img);
}

} // namespace

TEST(FocalStackProcessor, NonexistentFolderReturnsFalse)
{
    FocalStackProcessor proc;
    EXPECT_FALSE(proc.loadFromFolder("/nonexistent/path/sdg_xyz_test_abc"));
    EXPECT_FALSE(proc.hasStack());
}

TEST(FocalStackProcessor, EmptyFolderReturnsFalse)
{
    QTemporaryDir tmp;
    ASSERT_TRUE(tmp.isValid());
    FocalStackProcessor proc;
    EXPECT_FALSE(proc.loadFromFolder(tmp.path()));
}

TEST(FocalStackProcessor, LoadsImagesAlphabetically)
{
    QTemporaryDir tmp;
    ASSERT_TRUE(tmp.isValid());
    writePng(tmp.filePath("img_00.png"));
    writePng(tmp.filePath("img_01.png"));
    writePng(tmp.filePath("img_02.png"));

    FocalStackProcessor proc;
    ASSERT_TRUE(proc.loadFromFolder(tmp.path()));
    EXPECT_EQ(static_cast<int>(proc.getStack().size()), 3);
    EXPECT_TRUE(proc.hasStack());
}

TEST(FocalStackProcessor, LoadedFramesAreGrayscale)
{
    QTemporaryDir tmp;
    ASSERT_TRUE(tmp.isValid());
    writePng(tmp.filePath("img_00.png"));
    writePng(tmp.filePath("img_01.png"));

    FocalStackProcessor proc;
    proc.loadFromFolder(tmp.path());
    for (const auto& frame : proc.getStack())
        EXPECT_EQ(frame.channels(), 1);
}

TEST(FocalStackProcessor, ParsesFocalPowersFromMetadataCSV)
{
    QTemporaryDir tmp;
    ASSERT_TRUE(tmp.isValid());
    writePng(tmp.filePath("a.png"));
    writePng(tmp.filePath("b.png"));

    QFile csv(tmp.filePath("metadata.csv"));
    ASSERT_TRUE(csv.open(QIODevice::WriteOnly));
    csv.write("a.png, -2.5\nb.png, 1.0\n");
    csv.close();

    FocalStackProcessor proc;
    ASSERT_TRUE(proc.loadFromFolder(tmp.path()));
    EXPECT_TRUE(proc.hasFocalPowers());
    ASSERT_EQ(static_cast<int>(proc.getFocalPowers().size()), 2);
    EXPECT_FLOAT_EQ(proc.getFocalPowers()[0], -2.5f);
    EXPECT_FLOAT_EQ(proc.getFocalPowers()[1],  1.0f);
}

TEST(FocalStackProcessor, NoCSVMeansNoFocalPowers)
{
    QTemporaryDir tmp;
    ASSERT_TRUE(tmp.isValid());
    writePng(tmp.filePath("img_00.png"));
    writePng(tmp.filePath("img_01.png"));

    FocalStackProcessor proc;
    ASSERT_TRUE(proc.loadFromFolder(tmp.path()));
    EXPECT_FALSE(proc.hasFocalPowers());
}

TEST(FocalStackProcessor, ClearResetsStack)
{
    QTemporaryDir tmp;
    ASSERT_TRUE(tmp.isValid());
    writePng(tmp.filePath("img_00.png"));

    FocalStackProcessor proc;
    proc.loadFromFolder(tmp.path());
    ASSERT_TRUE(proc.hasStack());
    proc.clearStack();
    EXPECT_FALSE(proc.hasStack());
}

TEST(FocalStackProcessor, ParsesFocalPowersFromFilenames)
{
    // Files named after their diopter value (-2.3.png, 0.0.png, 1.5.png)
    QTemporaryDir tmp;
    ASSERT_TRUE(tmp.isValid());
    writePng(tmp.filePath("-2.3.png"));
    writePng(tmp.filePath("0.0.png"));
    writePng(tmp.filePath("1.5.png"));

    FocalStackProcessor proc;
    ASSERT_TRUE(proc.loadFromFolder(tmp.path()));
    EXPECT_TRUE(proc.hasFocalPowers());
    ASSERT_EQ(static_cast<int>(proc.getFocalPowers().size()), 3);
    // Must be sorted ascending by diopter value
    EXPECT_FLOAT_EQ(proc.getFocalPowers()[0], -2.3f);
    EXPECT_FLOAT_EQ(proc.getFocalPowers()[1],  0.0f);
    EXPECT_FLOAT_EQ(proc.getFocalPowers()[2],  1.5f);
}
