#pragma once

#include "ILiquidLensController.h"

class MockLiquidLensController : public ILiquidLensController
{
public:
    bool    connect()    override { m_connected = true;  return true; }
    void    disconnect() override { m_connected = false; }
    bool    isConnected() const override { return m_connected; }
    QString deviceName()  const override { return "Mock Liquid Lens (Simulator)"; }

    bool    setDioptre(double dioptre) override { m_dioptre = dioptre; return true; }
    double  minDioptre() const override { return -3.0; }
    double  maxDioptre() const override { return  3.0; }
    double  currentDioptre() const      { return m_dioptre; }

private:
    bool   m_connected = false;
    double m_dioptre   = 0.0;
};
