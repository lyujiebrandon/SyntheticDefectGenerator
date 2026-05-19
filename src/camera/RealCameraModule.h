#pragma once

#include "ICameraModule.h"
#include <sv_gen_sdk.h>
#include <vector>

// SVS-VISTEK SVCam Kit implementation of ICameraModule.
// Connects to the first available GigE/USB3 camera discovered by the SDK.
class RealCameraModule : public ICameraModule
{
public:
    RealCameraModule();
    ~RealCameraModule() override;

    bool    connect()    override;
    void    disconnect() override;
    bool    isConnected() const override { return m_connected; }
    QString deviceName() const  override { return m_deviceName; }

    cv::Mat captureFrame() override;

    // Set/get a GenICam float feature on the remote device (e.g. "ExposureTime", "Gain").
    bool setFeatureFloat(const char* featureName, double value);
    bool getFeatureFloat(const char* featureName, double& valueOut);

    // Execute a GenICam command feature (e.g. "AcquisitionStart").
    bool executeCommand(const char* featureName, uint32_t timeoutMs = 5000);

private:
    bool initBuffers(int64_t payloadSize);
    void releaseBuffers();
    cv::Mat bufferToMat(const SV_BUFFER_INFO& info);

    SV_SYSTEM_HANDLE        m_hSystem       = nullptr;
    SV_INTERFACE_HANDLE     m_hInterface    = nullptr;
    SV_DEVICE_HANDLE        m_hDevice       = nullptr;
    SV_REMOTE_DEVICE_HANDLE m_hRemoteDevice = nullptr;
    SV_STREAM_HANDLE        m_hStream       = nullptr;

    static constexpr int      kNumBuffers  = 4;
    static constexpr uint32_t kTimeoutMs   = 3000;

    std::vector<SV_BUFFER_HANDLE>       m_bufferHandles;
    std::vector<std::vector<uint8_t>>   m_bufferMemory;

    bool    m_connected  = false;
    QString m_deviceName = "SVS-VISTEK Camera";
};
