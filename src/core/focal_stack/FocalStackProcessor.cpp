#include "FocalStackProcessor.h"
#include "camera/ICameraModule.h"

bool FocalStackProcessor::captureStack(ICameraModule& camera,
                                        const SweepParams& params,
                                        ProgressCallback onProgress)
{
    m_stack.clear();

    int totalSteps = params.imageCount;
    double focusRange = params.endFocus - params.startFocus;
    double stepSize   = (totalSteps > 1) ? focusRange / (totalSteps - 1) : 0.0;

    for (int i = 0; i < totalSteps; ++i) {
        double focusPos = params.startFocus + i * stepSize;

        if (!camera.setFocusPosition(focusPos))
            return false;

        cv::Mat frame = camera.captureFrame();
        if (frame.empty())
            return false;

        m_stack.push_back(std::move(frame));

        if (onProgress) {
            int pct = static_cast<int>((i + 1) * 100 / totalSteps);
            onProgress(pct, QString("Captured frame %1 / %2 (focus = %3)")
                .arg(i + 1).arg(totalSteps).arg(focusPos, 0, 'f', 1));
        }
    }

    return !m_stack.empty();
}
