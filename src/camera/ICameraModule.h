#pragma once

#include <QString>
#include <opencv2/core.hpp>

// Abstract interface for all camera backends.
// MockCameraModule  : simulation — no hardware required.
// RealCameraModule  : SVS-VISTEK SVCam Kit SDK — requires hardware.
class ICameraModule
{
public:
    virtual ~ICameraModule() = default;

    virtual bool    connect()     = 0;
    virtual void    disconnect()  = 0;
    virtual bool    isConnected() const = 0;
    virtual QString deviceName()  const = 0;

    // Trigger a single-frame acquisition and return it as an 8-bit grayscale Mat.
    virtual cv::Mat captureFrame() = 0;
};
