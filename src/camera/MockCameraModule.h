#pragma once

#include "ICameraModule.h"

// Generates synthetic blurred frames for home development without hardware.
// Simulates focus behavior by applying Gaussian blur inversely proportional
// to how close the focus position is to the sharpest focal plane.
class MockCameraModule : public ICameraModule
{
public:
    MockCameraModule();

    bool    connect()    override;
    void    disconnect() override;
    bool    isConnected() const override { return m_connected; }
    QString deviceName() const  override { return "Mock Camera (Simulator)"; }

    bool    setFocusPosition(double position) override;
    cv::Mat captureFrame()                    override;

private:
    bool   m_connected     = false;
    double m_focusPosition = 0.0;

    // The "golden" source image for simulation (loaded on connect)
    cv::Mat m_sourceImage;
};
