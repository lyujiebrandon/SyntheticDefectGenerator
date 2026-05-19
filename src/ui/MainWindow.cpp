#include "MainWindow.h"
#include "ui_MainWindow.h"

#include <QFileDialog>
#include <QMessageBox>
#include <QDateTime>
#include <QPixmap>
#include <QImage>

#include <opencv2/imgproc.hpp>

#include "camera/MockCameraModule.h"
#include "camera/MockLiquidLensController.h"
#include "core/focal_stack/FocalStackProcessor.h"
#include "core/image_registration/ImageRegistrator.h"
#include "core/depth_map/DepthMapReconstructor.h"
#include "core/defect_gen/DefectGenerator.h"

// Uncomment to use real hardware instead of mock:
// #include "camera/RealCameraModule.h"

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_camera(std::make_unique<MockCameraModule>())
    , m_lens(std::make_unique<MockLiquidLensController>())
    , m_focalProcessor(std::make_unique<FocalStackProcessor>())
    , m_registrator(std::make_unique<ImageRegistrator>())
    , m_depthReconstructor(std::make_unique<DepthMapReconstructor>())
    , m_defectGenerator(std::make_unique<DefectGenerator>())
{
    ui->setupUi(this);

    m_statusLabel = new QLabel("Ready", this);
    m_progressBar = new QProgressBar(this);
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_progressBar->setFixedWidth(200);
    m_progressBar->setVisible(false);

    ui->statusbar->addPermanentWidget(m_statusLabel);
    ui->statusbar->addPermanentWidget(m_progressBar);

    setupConnections();
    updateCameraStatus(false);
    logMessage("Application started. Camera: MockCameraModule | Lens: MockLiquidLensController");
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::setupConnections()
{
    connect(ui->btnConnectCamera,    &QPushButton::clicked, this, &MainWindow::onConnectCamera);
    connect(ui->btnStartSweep,       &QPushButton::clicked, this, &MainWindow::onStartFocalSweep);
    connect(ui->btnRegisterStack,    &QPushButton::clicked, this, &MainWindow::onRegisterStack);
    connect(ui->btnReconstructDepth, &QPushButton::clicked, this, &MainWindow::onReconstructDepthMap);
    connect(ui->btnGenerateDefects,  &QPushButton::clicked, this, &MainWindow::onGenerateDefects);
    connect(ui->btnBrowseOutput,     &QPushButton::clicked, this, &MainWindow::onBrowseOutputDir);
    connect(ui->btnExportDataset,    &QPushButton::clicked, this, &MainWindow::onExportDataset);
}

// ─── Tab 1: Focal Capture ─────────────────────────────────────────────────────

void MainWindow::onConnectCamera()
{
    bool camOk  = m_camera->connect();
    bool lensOk = m_lens->connect();

    if (camOk && lensOk) {
        updateCameraStatus(true);
        logMessage(QString("Connected — Camera: %1 | Lens: %2")
            .arg(m_camera->deviceName(), m_lens->deviceName()));
    } else {
        logMessage("ERROR: Failed to connect camera or lens.");
        QMessageBox::critical(this, "Connection Error",
            "Could not connect to camera or liquid lens controller.");
    }
}

void MainWindow::onStartFocalSweep()
{
    if (!m_camera->isConnected()) {
        QMessageBox::warning(this, "Not Connected", "Connect camera and lens first.");
        return;
    }

    FocalStackProcessor::SweepParams params;
    params.startDioptre = ui->spinDioptreStart->value();
    params.endDioptre   = ui->spinDioptreEnd->value();
    params.imageCount   = ui->spinImageCount->value();

    logMessage(QString("Starting focal sweep: %1 frames, %.2f D → %.2f D")
        .arg(params.imageCount).arg(params.startDioptre).arg(params.endDioptre));

    setControlsEnabled(false);
    m_progressBar->setVisible(true);

    bool ok = m_focalProcessor->captureStack(*m_camera, *m_lens, params,
        [this](int pct, const QString& msg){ onOperationProgress(pct, msg); });

    if (ok) {
        // Show the middle frame as preview
        const auto& stack = m_focalProcessor->getStack();
        showMatInLabel(ui->lblCapturePreview, stack[stack.size() / 2]);
        onOperationComplete(QString("Focal stack captured: %1 frames.").arg(params.imageCount));
        ui->tabWidget->setTabEnabled(1, true);
    } else {
        onOperationError("Focal sweep failed.");
    }
}

// ─── Tab 2: Image Registration ───────────────────────────────────────────────

void MainWindow::onRegisterStack()
{
    if (!m_focalProcessor->hasStack()) {
        QMessageBox::warning(this, "No Stack", "Capture a focal stack first.");
        return;
    }

    ImageRegistrator::Params params;
    params.motionModel    = ui->comboMotionModel->currentIndex() == 0
                            ? ImageRegistrator::MotionModel::Euclidean
                            : ImageRegistrator::MotionModel::Affine;
    params.eccIterations  = ui->spinEccIterations->value();
    params.eccEpsilon     = ui->spinEccEpsilon->value();

    logMessage(QString("Registering %1 frames using ECC (%2)...")
        .arg(m_focalProcessor->getStack().size())
        .arg(params.motionModel == ImageRegistrator::MotionModel::Affine ? "Affine" : "Euclidean"));

    setControlsEnabled(false);
    m_progressBar->setVisible(true);

    bool ok = m_registrator->registerStack(m_focalProcessor->getStack(), params,
        [this](int pct, const QString& msg){ onOperationProgress(pct, msg); });

    if (ok) {
        const auto& stack = m_focalProcessor->getStack();
        showMatInLabel(ui->lblRegistrationPreview, stack[stack.size() / 2]);
        onOperationComplete("Image registration complete — focus breathing corrected.");
        ui->tabWidget->setTabEnabled(2, true);
    } else {
        onOperationError("Image registration failed.");
    }
}

// ─── Tab 3: Depth Reconstruction ─────────────────────────────────────────────

void MainWindow::onReconstructDepthMap()
{
    if (!m_focalProcessor->hasStack()) {
        QMessageBox::warning(this, "No Stack", "Complete registration first.");
        return;
    }

    DepthMapReconstructor::Params params;
    params.kernelSize = ui->spinLaplacianKernel->value();

    logMessage("Reconstructing depth map...");
    setControlsEnabled(false);
    m_progressBar->setVisible(true);

    bool ok = m_depthReconstructor->reconstruct(m_focalProcessor->getStack(), params,
        [this](int pct, const QString& msg){ onOperationProgress(pct, msg); });

    if (ok) {
        showMatInLabel(ui->lblDepthPreview, m_depthReconstructor->getDepthMap());
        onOperationComplete("Depth map reconstruction complete.");
        ui->tabWidget->setTabEnabled(3, true);
    } else {
        onOperationError("Depth reconstruction failed.");
    }
}

// ─── Tab 4: Defect Generation ─────────────────────────────────────────────────

void MainWindow::onGenerateDefects()
{
    if (!m_depthReconstructor->hasDepthMap()) {
        QMessageBox::warning(this, "No Depth Map", "Reconstruct a depth map first.");
        return;
    }

    DefectGenerator::Params params;
    params.defectCount   = ui->spinDefectCount->value();
    params.severity      = ui->sliderSeverity->value() / 100.0f;
    params.scaleFactor   = ui->spinDefectScale->value();
    params.enableScratch = ui->chkScratch->isChecked();
    params.enableDent    = ui->chkDent->isChecked();
    params.enableCrack   = ui->chkCrack->isChecked();
    params.enablePit     = ui->chkPit->isChecked();

    logMessage(QString("Generating %1 synthetic defects...").arg(params.defectCount));
    setControlsEnabled(false);
    m_progressBar->setVisible(true);

    bool ok = m_defectGenerator->generate(m_depthReconstructor->getDepthMap(), params,
        [this](int pct, const QString& msg){ onOperationProgress(pct, msg); });

    if (ok) {
        onOperationComplete(QString("Generated %1 defect images.").arg(params.defectCount));
        ui->tabWidget->setTabEnabled(4, true);
    } else {
        onOperationError("Defect generation failed.");
    }
}

// ─── Tab 5: Dataset Export ────────────────────────────────────────────────────

void MainWindow::onBrowseOutputDir()
{
    QString dir = QFileDialog::getExistingDirectory(this, "Select Output Directory",
        QDir::homePath(), QFileDialog::ShowDirsOnly);
    if (!dir.isEmpty())
        ui->lineOutputDir->setText(dir);
}

void MainWindow::onExportDataset()
{
    QString outDir = ui->lineOutputDir->text();
    if (outDir.isEmpty()) {
        QMessageBox::warning(this, "No Output Dir", "Select an output directory first.");
        return;
    }
    if (!m_defectGenerator->hasOutput()) {
        QMessageBox::warning(this, "No Data", "Generate defects first.");
        return;
    }

    logMessage(QString("Exporting dataset to: %1").arg(outDir));
    setControlsEnabled(false);
    m_progressBar->setVisible(true);

    bool ok = m_defectGenerator->exportDataset(outDir,
        [this](int pct, const QString& msg){ onOperationProgress(pct, msg); });

    if (ok) onOperationComplete("Dataset export complete.");
    else    onOperationError("Export failed.");
}

// ─── Shared helpers ────────────────────────────────────────────────────────────

void MainWindow::onOperationProgress(int percent, const QString& message)
{
    m_progressBar->setValue(percent);
    m_statusLabel->setText(message);
    QApplication::processEvents();
}

void MainWindow::onOperationComplete(const QString& message)
{
    m_progressBar->setVisible(false);
    m_progressBar->setValue(0);
    m_statusLabel->setText("Ready");
    setControlsEnabled(true);
    logMessage(message);
}

void MainWindow::onOperationError(const QString& error)
{
    m_progressBar->setVisible(false);
    m_progressBar->setValue(0);
    m_statusLabel->setText("Error");
    setControlsEnabled(true);
    logMessage("ERROR: " + error);
    QMessageBox::critical(this, "Operation Failed", error);
}

void MainWindow::updateCameraStatus(bool connected)
{
    ui->lblCameraStatus->setText(connected
        ? QString("Connected: %1").arg(m_camera->deviceName())
        : "Disconnected");
    ui->lblCameraStatus->setStyleSheet(connected
        ? "color: green; font-weight: bold;"
        : "color: red;");
    ui->btnStartSweep->setEnabled(connected);
}

void MainWindow::setControlsEnabled(bool enabled)
{
    ui->btnConnectCamera->setEnabled(enabled);
    ui->btnStartSweep->setEnabled(enabled && m_camera->isConnected());
    ui->btnRegisterStack->setEnabled(enabled);
    ui->btnReconstructDepth->setEnabled(enabled);
    ui->btnGenerateDefects->setEnabled(enabled);
    ui->btnExportDataset->setEnabled(enabled);
}

void MainWindow::showMatInLabel(QLabel* label, const cv::Mat& mat)
{
    if (mat.empty()) return;

    cv::Mat display;
    if (mat.type() == CV_32F) {
        cv::Mat norm;
        cv::normalize(mat, norm, 0, 255, cv::NORM_MINMAX);
        norm.convertTo(display, CV_8U);
        cv::applyColorMap(display, display, cv::COLORMAP_JET);
    } else {
        mat.copyTo(display);
    }

    if (display.channels() == 1)
        cv::cvtColor(display, display, cv::COLOR_GRAY2RGB);

    QImage img(display.data, display.cols, display.rows,
               static_cast<int>(display.step),
               QImage::Format_RGB888);

    label->setPixmap(QPixmap::fromImage(img).scaled(
        label->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

void MainWindow::logMessage(const QString& message)
{
    QString ts = QDateTime::currentDateTime().toString("hh:mm:ss");
    ui->textLog->appendPlainText(QString("[%1] %2").arg(ts, message));
}
