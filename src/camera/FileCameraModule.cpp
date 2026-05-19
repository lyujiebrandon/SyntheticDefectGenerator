#include "FileCameraModule.h"

#include <QDir>
#include <opencv2/imgcodecs.hpp>

FileCameraModule::FileCameraModule(const QString& folderPath)
    : m_folderPath(folderPath)
{}

bool FileCameraModule::connect()
{
    m_images.clear();
    m_frameIndex = 0;

    QDir dir(m_folderPath);
    if (!dir.exists()) return false;

    const QStringList filters = {"*.png", "*.jpg", "*.jpeg", "*.bmp", "*.tiff", "*.tif"};
    const QStringList files   = dir.entryList(filters, QDir::Files, QDir::Name);

    for (const QString& file : files) {
        cv::Mat img = cv::imread(dir.absoluteFilePath(file).toStdString(),
                                 cv::IMREAD_GRAYSCALE);
        if (!img.empty())
            m_images.push_back(std::move(img));
    }

    m_connected = !m_images.empty();
    return m_connected;
}

void FileCameraModule::disconnect()
{
    m_images.clear();
    m_frameIndex = 0;
    m_connected  = false;
}

cv::Mat FileCameraModule::captureFrame()
{
    if (!m_connected || m_images.empty()) return {};
    return m_images[m_frameIndex++ % static_cast<int>(m_images.size())].clone();
}

QString FileCameraModule::deviceName() const
{
    return QString("File Camera [%1]  (%2 images)")
        .arg(QDir(m_folderPath).dirName())
        .arg(static_cast<int>(m_images.size()));
}
