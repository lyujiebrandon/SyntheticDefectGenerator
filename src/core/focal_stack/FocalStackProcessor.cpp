#include "FocalStackProcessor.h"

#include "camera/ICameraModule.h"
#include "camera/ILiquidLensController.h"
#include "camera/MockCameraModule.h"
#include <QDir>
#include <opencv2/imgcodecs.hpp>

bool FocalStackProcessor::captureStack(ICameraModule& camera,
                                        ILiquidLensController& lens,
                                        const SweepParams& params,
                                        ProgressCallback onProgress)
{
    m_stack.clear();

    int N = params.imageCount;
    double dioptreRange = params.endDioptre - params.startDioptre;
    double step = (N > 1) ? dioptreRange / (N - 1) : 0.0;

    // Keep a pointer to MockCameraModule if we're in simulation mode
    // so we can synchronise its internal dioptre state.
    auto* mockCam = dynamic_cast<MockCameraModule*>(&camera);

    for (int i = 0; i < N; ++i) {
        double dioptre = params.startDioptre + i * step;

        lens.setDioptre(dioptre);
        if (mockCam) mockCam->setSimulatedDioptre(dioptre);

        cv::Mat frame = camera.captureFrame();
        if (frame.empty()) return false;

        m_stack.push_back(std::move(frame));

        if (onProgress) {
            int pct = (i + 1) * 100 / N;
            onProgress(pct, QString("Captured frame %1 / %2  (%3 D)")
                .arg(i + 1).arg(N).arg(dioptre, 0, 'f', 2));
        }
    }

    return !m_stack.empty();
}

bool FocalStackProcessor::loadFromFolder(const QString& folderPath, ProgressCallback onProgress)
{
    m_stack.clear();

    QDir dir(folderPath);
    if (!dir.exists()) return false;

    const QStringList filters = {"*.png", "*.jpg", "*.jpeg", "*.bmp", "*.tiff", "*.tif"};
    const QStringList files   = dir.entryList(filters, QDir::Files, QDir::Name);
    if (files.isEmpty()) return false;

    int total = files.size();
    for (int i = 0; i < total; ++i) {
        cv::Mat img = cv::imread(dir.absoluteFilePath(files[i]).toStdString(),
                                 cv::IMREAD_GRAYSCALE);
        if (img.empty()) { m_stack.clear(); return false; }
        m_stack.push_back(std::move(img));

        if (onProgress) {
            onProgress((i + 1) * 100 / total,
                       QString("Loading %1 / %2: %3").arg(i + 1).arg(total).arg(files[i]));
        }
    }

    return true;
}
