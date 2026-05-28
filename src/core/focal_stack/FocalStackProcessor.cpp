#include "FocalStackProcessor.h"

#include <QDir>
#include <opencv2/imgcodecs.hpp>

bool FocalStackProcessor::loadFromFolder(const QString& folderPath, ProgressCallback onProgress)
{
    m_stack.clear();

    QDir dir(folderPath);
    if (!dir.exists()) return false;

    const QStringList filters = {"*.png", "*.jpg", "*.jpeg", "*.bmp", "*.tiff", "*.tif"};
    const QStringList files   = dir.entryList(filters, QDir::Files, QDir::Name);
    if (files.isEmpty()) return false;

    int total = files.size();
    for (int i = 0; i < total; ++i) {
        cv::Mat img = cv::imread(dir.absoluteFilePath(files[i]).toStdString(),
                                 cv::IMREAD_GRAYSCALE);
        if (img.empty()) { m_stack.clear(); return false; }
        m_stack.push_back(std::move(img));

        if (onProgress) {
            onProgress((i + 1) * 100 / total,
                       QString("Loading %1 / %2: %3").arg(i + 1).arg(total).arg(files[i]));
        }
    }

    return true;
}
