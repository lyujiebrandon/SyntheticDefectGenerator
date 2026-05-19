#pragma once

#include "ICameraModule.h"
#include <QString>
#include <vector>

// Replays a pre-captured focal stack from an image folder.
// Images are loaded in alphabetical filename order.
// captureFrame() returns them sequentially, cycling if exhausted.
class FileCameraModule : public ICameraModule
{
public:
    explicit FileCameraModule(const QString& folderPath = {});

    bool    connect()     override;
    void    disconnect()  override;
    bool    isConnected() const override { return m_connected; }
    QString deviceName()  const override;
    cv::Mat captureFrame() override;

    int  imageCount()  const { return static_cast<int>(m_images.size()); }
    void setFolderPath(const QString& path) { m_folderPath = path; }

private:
    QString              m_folderPath;
    std::vector<cv::Mat> m_images;
    int                  m_frameIndex = 0;
    bool                 m_connected  = false;
};
