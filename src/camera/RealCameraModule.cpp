#include "RealCameraModule.h"

#include <opencv2/imgproc.hpp>
#include <QDebug>

RealCameraModule::RealCameraModule() = default;

RealCameraModule::~RealCameraModule()
{
    disconnect();
}

bool RealCameraModule::connect()
{
    if (m_connected) return true;

    // ── 1. Initialise SDK ────────────────────────────────────────────────────
    if (SVLibInit() != SV_ERROR_SUCCESS) {
        qWarning() << "RealCameraModule: SVLibInit failed";
        return false;
    }

    // ── 2. Open first GenTL producer (system) ───────────────────────────────
    uint32_t tlCount = 0;
    SVLibSystemGetCount(&tlCount);
    if (tlCount == 0) {
        qWarning() << "RealCameraModule: No GenTL producers found";
        SVLibClose();
        return false;
    }

    if (SVLibSystemOpen(0, &m_hSystem) != SV_ERROR_SUCCESS) {
        qWarning() << "RealCameraModule: SVLibSystemOpen failed";
        SVLibClose();
        return false;
    }

    // ── 3. Discover interfaces and devices ──────────────────────────────────
    bool8_t changed = false;
    SVSystemUpdateInterfaceList(m_hSystem, &changed, 1000);

    uint32_t ifCount = 0;
    SVSystemGetNumInterfaces(m_hSystem, &ifCount);

    for (uint32_t i = 0; i < ifCount; ++i) {
        char ifId[SV_STRING_SIZE];
        size_t sz = SV_STRING_SIZE;
        SVSystemGetInterfaceId(m_hSystem, i, ifId, &sz);

        SV_INTERFACE_HANDLE hIF = nullptr;
        if (SVSystemInterfaceOpen(m_hSystem, ifId, &hIF) != SV_ERROR_SUCCESS)
            continue;

        bool8_t devChanged = false;
        SVInterfaceUpdateDeviceList(hIF, &devChanged, 200);

        uint32_t devCount = 0;
        SVInterfaceGetNumDevices(hIF, &devCount);
        if (devCount == 0) { SVInterfaceClose(hIF); continue; }

        // Get device ID string for the first device
        char devId[SV_STRING_SIZE];
        size_t devSz = SV_STRING_SIZE;
        SVInterfaceGetDeviceId(hIF, 0, devId, &devSz);

        // Get device info for display name
        SV_DEVICE_INFO devInfo{};
        SVInterfaceDeviceGetInfo(hIF, devId, &devInfo);

        // Open device — returns both local handle and remote (GenICam) handle
        if (SVInterfaceDeviceOpen(hIF, devId, SV_DEVICE_ACCESS_EXCLUSIVE,
                                  &m_hDevice, &m_hRemoteDevice) == SV_ERROR_SUCCESS) {
            m_hInterface = hIF;
            m_deviceName = QString("%1 %2 [%3]")
                .arg(devInfo.vendor).arg(devInfo.model).arg(devInfo.serialNumber);
            break;
        }
        SVInterfaceClose(hIF);
    }

    if (m_hDevice == nullptr) {
        qWarning() << "RealCameraModule: No camera device found";
        SVSystemClose(m_hSystem);
        SVLibClose();
        return false;
    }

    // ── 4. Open data stream ──────────────────────────────────────────────────
    char streamId[SV_STRING_SIZE];
    size_t streamSz = SV_STRING_SIZE;
    SVDeviceGetStreamId(m_hDevice, 0, streamId, &streamSz);

    if (SVDeviceStreamOpen(m_hDevice, streamId, &m_hStream) != SV_ERROR_SUCCESS) {
        qWarning() << "RealCameraModule: SVDeviceStreamOpen failed";
        SVDeviceClose(m_hDevice);
        SVSystemClose(m_hSystem);
        SVLibClose();
        return false;
    }

    // ── 5. Get payload size from GenICam feature ─────────────────────────────
    SV_FEATURE_HANDLE hPayloadFeat = nullptr;
    int64_t payloadSize = 0;
    if (SVFeatureGetByName(m_hRemoteDevice, "PayloadSize", &hPayloadFeat) == SV_ERROR_SUCCESS)
        SVFeatureGetValueInt64(m_hRemoteDevice, hPayloadFeat, &payloadSize);

    if (payloadSize <= 0 || !initBuffers(payloadSize)) {
        qWarning() << "RealCameraModule: Buffer init failed (payload=" << payloadSize << ")";
        SVStreamClose(m_hStream);
        SVDeviceClose(m_hDevice);
        SVSystemClose(m_hSystem);
        SVLibClose();
        return false;
    }

    // ── 6. Start acquisition (host engine + camera command) ──────────────────
    SVStreamAcquisitionStart(m_hStream, SV_ACQ_START_FLAGS_DEFAULT, GENTL_INFINITE);
    executeCommand("AcquisitionStart");

    m_connected = true;
    qDebug() << "RealCameraModule: Connected —" << m_deviceName;
    return true;
}

void RealCameraModule::disconnect()
{
    if (!m_connected) return;

    executeCommand("AcquisitionStop");
    SVStreamAcquisitionStop(m_hStream, SV_ACQ_STOP_FLAGS_DEFAULT);

    releaseBuffers();

    if (m_hStream)    { SVStreamClose(m_hStream);       m_hStream    = nullptr; }
    if (m_hDevice)    { SVDeviceClose(m_hDevice);       m_hDevice    = nullptr; }
    if (m_hInterface) { SVInterfaceClose(m_hInterface); m_hInterface = nullptr; }
    if (m_hSystem)    { SVSystemClose(m_hSystem);       m_hSystem    = nullptr; }
    SVLibClose();

    m_hRemoteDevice = nullptr;
    m_connected = false;
}

cv::Mat RealCameraModule::captureFrame()
{
    if (!m_connected) return {};

    // Re-queue all buffers
    for (auto hBuf : m_bufferHandles)
        SVStreamQueueBuffer(m_hStream, hBuf);

    // Wait for the next completed frame
    void*             pUserData = nullptr;
    SV_BUFFER_HANDLE  hBuf      = nullptr;
    if (SVStreamWaitForNewBuffer(m_hStream, &pUserData, &hBuf, kTimeoutMs) != SV_ERROR_SUCCESS
        || hBuf == nullptr)
        return {};

    SV_BUFFER_INFO info{};
    SVStreamBufferGetInfo(m_hStream, hBuf, &info);

    return bufferToMat(info);
}

bool RealCameraModule::initBuffers(int64_t payloadSize)
{
    m_bufferMemory.resize(kNumBuffers, std::vector<uint8_t>(static_cast<size_t>(payloadSize)));
    m_bufferHandles.resize(kNumBuffers, nullptr);

    for (int i = 0; i < kNumBuffers; ++i) {
        if (SVStreamAnnounceBuffer(m_hStream,
                                   m_bufferMemory[i].data(),
                                   static_cast<uint32_t>(payloadSize),
                                   nullptr,
                                   &m_bufferHandles[i]) != SV_ERROR_SUCCESS)
            return false;
        SVStreamQueueBuffer(m_hStream, m_bufferHandles[i]);
    }
    return true;
}

void RealCameraModule::releaseBuffers()
{
    SVStreamFlushQueue(m_hStream, SV_ACQ_QUEUE_ALL_DISCARD);
    for (auto hBuf : m_bufferHandles)
        if (hBuf) SVStreamRevokeBuffer(m_hStream, hBuf, nullptr, nullptr);
    m_bufferHandles.clear();
    m_bufferMemory.clear();
}

cv::Mat RealCameraModule::bufferToMat(const SV_BUFFER_INFO& info)
{
    if (!info.pImagePtr) return {};

    int rows = static_cast<int>(info.iSizeY);
    int cols = static_cast<int>(info.iSizeX);

    cv::Mat raw(rows, cols, CV_8UC1, info.pImagePtr);
    cv::Mat frame;
    raw.copyTo(frame);
    return frame;
}

bool RealCameraModule::setFeatureFloat(const char* featureName, double value)
{
    if (!m_hRemoteDevice) return false;
    SV_FEATURE_HANDLE hFeat = nullptr;
    if (SVFeatureGetByName(m_hRemoteDevice, featureName, &hFeat) != SV_ERROR_SUCCESS)
        return false;
    return SVFeatureSetValueFloat(m_hRemoteDevice, hFeat, value) == SV_ERROR_SUCCESS;
}

bool RealCameraModule::getFeatureFloat(const char* featureName, double& valueOut)
{
    if (!m_hRemoteDevice) return false;
    SV_FEATURE_HANDLE hFeat = nullptr;
    if (SVFeatureGetByName(m_hRemoteDevice, featureName, &hFeat) != SV_ERROR_SUCCESS)
        return false;
    return SVFeatureGetValueFloat(m_hRemoteDevice, hFeat, &valueOut) == SV_ERROR_SUCCESS;
}

bool RealCameraModule::executeCommand(const char* featureName, uint32_t timeoutMs)
{
    if (!m_hRemoteDevice) return false;
    SV_FEATURE_HANDLE hFeat = nullptr;
    if (SVFeatureGetByName(m_hRemoteDevice, featureName, &hFeat) != SV_ERROR_SUCCESS)
        return false;
    return SVFeatureCommandExecute(m_hRemoteDevice, hFeat, timeoutMs, true) == SV_ERROR_SUCCESS;
}
