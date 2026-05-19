#pragma once

#include "ICameraModule.h"

// Simulates a camera + liquid lens system for home/offline development.
// Focus breathing: as dioptre deviates from the sharpest plane, the image
// is both blurred AND slightly scaled (simulating real liquid lens behaviour).
class MockCameraModule : public ICameraModule
{
public:
    MockCameraModule();

    bool    connect()    override;
    void    disconnect() override;
    bool    isConnected() const override { return m_connected; }
    QString deviceName() const  override { return "Mock Camera (Simulator)"; }

    cv::Mat captureFrame() override;

    // Called by FocalStackProcessor to simulate the lens being at a given dioptre.
    void setSimulatedDioptre(double dioptre) { m_dioptre = dioptre; }

private:
    bool    m_connected = false;
    double  m_dioptre   = 0.0;
    cv::Mat m_sourceImage;

    static constexpr double kSharpestDioptre  = 0.0;   // sharpest focal plane
    static constexpr double kBreathingCoeff   = 0.012; // scale shift per dioptre unit
    static constexpr double kBlurCoeff        = 0.5;   // blur sigma per dioptre unit
};
