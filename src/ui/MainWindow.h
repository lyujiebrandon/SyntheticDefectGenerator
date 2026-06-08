#pragma once

#include <QMainWindow>
#include <QLabel>
#include <QProgressBar>
#include "ui/ZoomableImageLabel.h"
#include <memory>
#include <opencv2/core.hpp>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class FocalStackProcessor;
class ImageRegistrator;
class DepthMapReconstructor;
class DefectGenerator;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

private slots:
    // Tab 1 — Focal Stack Input
    void onBrowseStack();
    void onLoadStack();

    // Tab 2 — Image Registration
    void onRegisterStack();

    // Tab 3 — Depth Reconstruction
    void onReconstructDepthMap();

    // Tab 4 — Defect Generation
    void onGenerateDefects();
    void onPrevDefect();
    void onNextDefect();

    // Tab 5 — Dataset Export
    void onBrowseOutputDir();
    void onExportDataset();

    // Shared
    void onOperationProgress(int percent, const QString& message);
    void onOperationComplete(const QString& message);
    void onOperationError(const QString& error);

private:
    void setupConnections();
    void setControlsEnabled(bool enabled);
    void showMatInLabel(ZoomableImageLabel* label, const cv::Mat& mat);
    void logMessage(const QString& message);

    void renderDefectPreview();    // redraws the current defect index
    void updateDefectNavigation(); // refreshes counter label and button states

    int     m_currentDefectIndex = 0;
    QString m_previewDefectType;

    Ui::MainWindow* ui;

    std::unique_ptr<FocalStackProcessor>  m_focalProcessor;
    std::unique_ptr<ImageRegistrator>     m_registrator;
    std::unique_ptr<DepthMapReconstructor> m_depthReconstructor;
    std::unique_ptr<DefectGenerator>      m_defectGenerator;

    QLabel*       m_statusLabel;
    QProgressBar* m_progressBar;
};
