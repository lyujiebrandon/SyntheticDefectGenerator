#pragma once

#include "ILiquidLensController.h"
#include <QSerialPort>

// Optotune EL-16-40 liquid lens controller via USB Lens Driver (LD4 / LD5).
// The LD enumerates as a virtual COM port on Windows.
//
// Protocol: 'A' command, signed int16 big-endian, 200 counts = 1.0 dioptre, XOR CRC.
// Verify scaling against your LD firmware version using Optotune ECSW before use.
class RealLiquidLensController : public ILiquidLensController
{
public:
    explicit RealLiquidLensController(const QString& portName);

    bool    connect()     override;
    void    disconnect()  override;
    bool    isConnected() const override { return m_serial.isOpen(); }
    QString deviceName()  const override;

    bool    setDioptre(double dioptre) override;
    double  minDioptre() const override { return -1.5; }
    double  maxDioptre() const override { return  3.5; }

private:
    bool sendPacket(const QByteArray& packet);

    QSerialPort m_serial;
    QString     m_portName;
};
