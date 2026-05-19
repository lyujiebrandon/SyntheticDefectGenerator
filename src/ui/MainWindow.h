#pragma once

#include <QMainWindow>
#include <QLabel>
#include <QProgressBar>
#include <memory>
#include <opencv2/core.hpp>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class FocalStackProcessor;
class ImageRegistrator;
class DepthMapReconstructor;
class DefectGenerator;
class ICameraModule;
class ILiquidLensController;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

private slots:
    // Tab 1 — Focal Capture
    void onConnectCamera();
    void onStartFocalSweep();
    void onBrowseImageFolder();
    void onCameraSourceChanged();
    void onRefreshComPorts();

    // Tab 2 — Image Registration
    void onRegisterStack();

    // Tab 3 — Depth Reconstruction
    void onReconstructDepthMap();

    // Tab 4 — Defect Generation
    void onGenerateDefects();

    // Tab 5 — Dataset Export
    void onBrowseOutputDir();
    void onExportDataset();

    // Shared
    void onOperationProgress(int percent, const QString& message);
    void onOperationComplete(const QString& message);
    void onOperationError(const QString& error);

private:
    void setupConnections();
    void updateCameraStatus(bool connected);
    void setControlsEnabled(bool enabled);
    void showMatInLabel(QLabel* label, const cv::Mat& mat);
    void logMessage(const QString& message);

    // Renders the defect preview with or without the highlight box
    void renderDefectPreview();

    cv::Mat  m_previewDefectImage;   // stored for toggle re-render
    cv::Rect m_previewDefectBounds;
    QString  m_previewDefectType;

    Ui::MainWindow* ui;

    std::unique_ptr<ICameraModule>        m_camera;
    std::unique_ptr<ILiquidLensController> m_lens;
    std::unique_ptr<FocalStackProcessor>  m_focalProcessor;
    std::unique_ptr<ImageRegistrator>     m_registrator;
    std::unique_ptr<DepthMapReconstructor> m_depthReconstructor;
    std::unique_ptr<DefectGenerator>      m_defectGenerator;

    QLabel*       m_statusLabel;
    QProgressBar* m_progressBar;
};
