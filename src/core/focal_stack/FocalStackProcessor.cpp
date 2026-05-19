#include "FocalStackProcessor.h"

#include "camera/ICameraModule.h"
#include "camera/ILiquidLensController.h"
#include "camera/MockCameraModule.h"

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
