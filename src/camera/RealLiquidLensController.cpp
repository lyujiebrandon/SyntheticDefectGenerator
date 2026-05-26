#include "RealLiquidLensController.h"

#include <QDebug>
#include <algorithm>
#include <cstdint>

RealLiquidLensController::RealLiquidLensController(const QString& portName)
    : m_portName(portName)
{
    m_serial.setPortName(portName);
    m_serial.setBaudRate(QSerialPort::Baud115200);
    m_serial.setDataBits(QSerialPort::Data8);
    m_serial.setParity(QSerialPort::NoParity);
    m_serial.setStopBits(QSerialPort::OneStop);
    m_serial.setFlowControl(QSerialPort::NoFlowControl);
}

bool RealLiquidLensController::connect()
{
    if (m_serial.isOpen()) return true;

    if (!m_serial.open(QIODevice::ReadWrite)) {
        m_lastError = m_serial.errorString();
        qWarning() << "RealLiquidLensController: Cannot open" << m_portName
                   << "—" << m_lastError;
        return false;
    }

    // Return lens to 0 D on connect so it starts from a known state
    setDioptre(0.0);
    qDebug() << "RealLiquidLensController: Opened" << m_portName;
    return true;
}

void RealLiquidLensController::disconnect()
{
    if (!m_serial.isOpen()) return;
    setDioptre(0.0);   // return to neutral before closing
    m_serial.close();
}

QString RealLiquidLensController::deviceName() const
{
    return QString("Optotune EL-16-40 [%1]").arg(m_portName);
}

bool RealLiquidLensController::setDioptre(double dioptre)
{
    if (!m_serial.isOpen()) return false;

    // ── Optotune Lens Driver protocol (LD4 / LD5) ──────────────────────────────
    // Packet layout (4 bytes):
    //   [0x41]  Command 'A' = set focal power
    //   [H][L]  Signed int16, big-endian.  200 counts = 1.0 dioptre.
    //   [CRC]   XOR of the three preceding bytes.
    //
    // Example: 1.5 D  →  raw = 300  →  0x01 0x2C  →  CRC = 0x41^0x01^0x2C = 0x6C
    // ───────────────────────────────────────────────────────────────────────────
    double clamped = std::clamp(dioptre, minDioptre(), maxDioptre());
    auto   raw     = static_cast<int16_t>(clamped * 200.0);

    auto h = static_cast<uint8_t>((raw >> 8) & 0xFF);
    auto l = static_cast<uint8_t>( raw        & 0xFF);

    QByteArray pkt(4, '\0');
    pkt[0] = 0x41;
    pkt[1] = static_cast<char>(h);
    pkt[2] = static_cast<char>(l);
    pkt[3] = static_cast<char>(0x41 ^ h ^ l);

    return sendPacket(pkt);
}

bool RealLiquidLensController::sendPacket(const QByteArray& packet)
{
    if (m_serial.write(packet) != packet.size()) {
        qWarning() << "RealLiquidLensController: Write error —" << m_serial.errorString();
        return false;
    }
    // LD echoes the command byte as an ACK — read and discard it
    m_serial.waitForReadyRead(100);
    m_serial.readAll();
    return true;
}
