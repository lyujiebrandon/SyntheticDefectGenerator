#pragma once

#include <QString>
#include <vector>
#include <opencv2/core.hpp>

// Abstract interface for all camera backends.
// MockCameraModule: used at home (no hardware required).
// RealCameraModule: wraps the JM Vistec SDK — implement at work only.
class ICameraModule
{
public:
    virtual ~ICameraModule() = default;

    virtual bool    connect()    = 0;
    virtual void    disconnect() = 0;
    virtual bool    isConnected() const = 0;
    virtual QString deviceName() const  = 0;

    // Move the lens/focus motor to the given position (0–1000 abstract units).
    virtual bool    setFocusPosition(double position) = 0;

    // Trigger a single-frame acquisition and return it as a grayscale Mat.
    virtual cv::Mat captureFrame() = 0;
};
