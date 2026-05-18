#pragma once

#include <QMainWindow>
#include <QLabel>
#include <QProgressBar>
#include <memory>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class FocalStackProcessor;
class DepthMapReconstructor;
class DefectGenerator;
class ICameraModule;

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

    // Tab 2 — Depth Reconstruction
    void onReconstructDepthMap();

    // Tab 3 — Defect Generation
    void onGenerateDefects();

    // Tab 4 — Dataset Export
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
    void logMessage(const QString& message);

    Ui::MainWindow* ui;

    std::unique_ptr<ICameraModule>       m_camera;
    std::unique_ptr<FocalStackProcessor> m_focalProcessor;
    std::unique_ptr<DepthMapReconstructor> m_depthReconstructor;
    std::unique_ptr<DefectGenerator>     m_defectGenerator;

    QLabel*       m_statusLabel;
    QProgressBar* m_progressBar;
};
