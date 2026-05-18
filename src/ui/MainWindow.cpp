#include "MainWindow.h"
#include "ui_MainWindow.h"

#include <QFileDialog>
#include <QMessageBox>
#include <QDateTime>

#include "camera/MockCameraModule.h"
#include "core/focal_stack/FocalStackProcessor.h"
#include "core/depth_map/DepthMapReconstructor.h"
#include "core/defect_gen/DefectGenerator.h"

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_camera(std::make_unique<MockCameraModule>())
    , m_focalProcessor(std::make_unique<FocalStackProcessor>())
    , m_depthReconstructor(std::make_unique<DepthMapReconstructor>())
    , m_defectGenerator(std::make_unique<DefectGenerator>())
{
    ui->setupUi(this);

    m_statusLabel  = new QLabel("Ready", this);
    m_progressBar  = new QProgressBar(this);
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_progressBar->setFixedWidth(200);
    m_progressBar->setVisible(false);

    ui->statusbar->addPermanentWidget(m_statusLabel);
    ui->statusbar->addPermanentWidget(m_progressBar);

    setupConnections();
    updateCameraStatus(false);
    logMessage("Application started. Camera: MockCameraModule (simulation mode).");
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::setupConnections()
{
    connect(ui->btnConnectCamera,   &QPushButton::clicked, this, &MainWindow::onConnectCamera);
    connect(ui->btnStartSweep,      &QPushButton::clicked, this, &MainWindow::onStartFocalSweep);
    connect(ui->btnReconstructDepth,&QPushButton::clicked, this, &MainWindow::onReconstructDepthMap);
    connect(ui->btnGenerateDefects, &QPushButton::clicked, this, &MainWindow::onGenerateDefects);
    connect(ui->btnBrowseOutput,    &QPushButton::clicked, this, &MainWindow::onBrowseOutputDir);
    connect(ui->btnExportDataset,   &QPushButton::clicked, this, &MainWindow::onExportDataset);
}

// ─── Tab 1 ────────────────────────────────────────────────────────────────────

void MainWindow::onConnectCamera()
{
    if (m_camera->connect()) {
        updateCameraStatus(true);
        logMessage(QString("Camera connected: %1").arg(m_camera->deviceName()));
    } else {
        logMessage("ERROR: Failed to connect to camera.");
        QMessageBox::critical(this, "Camera Error", "Could not connect to camera.");
    }
}

void MainWindow::onStartFocalSweep()
{
    if (!m_camera->isConnected()) {
        QMessageBox::warning(this, "No Camera", "Connect a camera first.");
        return;
    }

    FocalStackProcessor::SweepParams params;
    params.startFocus  = ui->spinFocusStart->value();
    params.endFocus    = ui->spinFocusEnd->value();
    params.stepSize    = ui->spinFocusStep->value();
    params.imageCount  = ui->spinImageCount->value();

    logMessage(QString("Starting focal sweep: %1 images, focus %2→%3")
        .arg(params.imageCount)
        .arg(params.startFocus)
        .arg(params.endFocus));

    setControlsEnabled(false);
    m_progressBar->setVisible(true);

    bool ok = m_focalProcessor->captureStack(*m_camera, params,
        [this](int pct, const QString& msg){ onOperationProgress(pct, msg); });

    if (ok) {
        onOperationComplete(QString("Focal stack captured: %1 frames.").arg(params.imageCount));
        ui->tabWidget->setTabEnabled(1, true);
    } else {
        onOperationError("Focal sweep failed.");
    }
}

// ─── Tab 2 ────────────────────────────────────────────────────────────────────

void MainWindow::onReconstructDepthMap()
{
    if (!m_focalProcessor->hasStack()) {
        QMessageBox::warning(this, "No Stack", "Capture a focal stack first.");
        return;
    }

    logMessage("Reconstructing depth map from focal stack...");
    setControlsEnabled(false);
    m_progressBar->setVisible(true);

    DepthMapReconstructor::Params params;
    params.kernelSize = ui->spinLaplacianKernel->value();

    bool ok = m_depthReconstructor->reconstruct(m_focalProcessor->getStack(), params,
        [this](int pct, const QString& msg){ onOperationProgress(pct, msg); });

    if (ok) {
        onOperationComplete("Depth map reconstruction complete.");
        ui->tabWidget->setTabEnabled(2, true);
    } else {
        onOperationError("Depth reconstruction failed.");
    }
}

// ─── Tab 3 ────────────────────────────────────────────────────────────────────

void MainWindow::onGenerateDefects()
{
    if (!m_depthReconstructor->hasDepthMap()) {
        QMessageBox::warning(this, "No Depth Map", "Reconstruct a depth map first.");
        return;
    }

    DefectGenerator::Params params;
    params.defectCount  = ui->spinDefectCount->value();
    params.severity     = ui->sliderSeverity->value() / 100.0f;
    params.scaleFactor  = ui->spinDefectScale->value();
    params.enableScratch = ui->chkScratch->isChecked();
    params.enableDent    = ui->chkDent->isChecked();
    params.enableCrack   = ui->chkCrack->isChecked();
    params.enablePit     = ui->chkPit->isChecked();

    logMessage(QString("Generating %1 synthetic defects (severity: %2)...")
        .arg(params.defectCount)
        .arg(params.severity, 0, 'f', 2));

    setControlsEnabled(false);
    m_progressBar->setVisible(true);

    bool ok = m_defectGenerator->generate(m_depthReconstructor->getDepthMap(), params,
        [this](int pct, const QString& msg){ onOperationProgress(pct, msg); });

    if (ok) {
        onOperationComplete(QString("Generated %1 defect variants.").arg(params.defectCount));
        ui->tabWidget->setTabEnabled(3, true);
    } else {
        onOperationError("Defect generation failed.");
    }
}

// ─── Tab 4 ────────────────────────────────────────────────────────────────────

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

    if (ok)
        onOperationComplete("Dataset export complete.");
    else
        onOperationError("Export failed.");
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
    ui->btnReconstructDepth->setEnabled(enabled);
    ui->btnGenerateDefects->setEnabled(enabled);
    ui->btnExportDataset->setEnabled(enabled);
}

void MainWindow::logMessage(const QString& message)
{
    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss");
    ui->textLog->appendPlainText(QString("[%1] %2").arg(timestamp, message));
}
