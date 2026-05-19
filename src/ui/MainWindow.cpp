#include "MainWindow.h"
#include "ui_MainWindow.h"

#include <QApplication>
#include <QDir>
#include <QFileDialog>
#include <QMessageBox>
#include <QDateTime>
#include <QPixmap>
#include <QImage>

#include <opencv2/imgproc.hpp>

#include "camera/MockCameraModule.h"
#include "camera/MockLiquidLensController.h"
#include "camera/FileCameraModule.h"
#include "camera/RealCameraModule.h"
#ifdef SDG_HAVE_SERIAL_PORT
#  include "camera/RealLiquidLensController.h"
#  include <QSerialPortInfo>
#endif
#include "core/focal_stack/FocalStackProcessor.h"
#include "core/image_registration/ImageRegistrator.h"
#include "core/depth_map/DepthMapReconstructor.h"
#include "core/defect_gen/DefectGenerator.h"
#include "core/defect_gen/DefectTypes.h"

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
    connect(ui->btnBrowseImageFolder,&QPushButton::clicked,  this, &MainWindow::onBrowseImageFolder);
    connect(ui->btnRefreshPorts,     &QPushButton::clicked,  this, &MainWindow::onRefreshComPorts);
    connect(ui->radioMockSynthetic,  &QRadioButton::toggled, this, &MainWindow::onCameraSourceChanged);
    connect(ui->radioMockImages,     &QRadioButton::toggled, this, &MainWindow::onCameraSourceChanged);
    connect(ui->radioRealCamera,     &QRadioButton::toggled, this, &MainWindow::onCameraSourceChanged);

    onRefreshComPorts(); // populate COM port list on startup
    connect(ui->btnRegisterStack,    &QPushButton::clicked, this, &MainWindow::onRegisterStack);
    connect(ui->btnReconstructDepth, &QPushButton::clicked, this, &MainWindow::onReconstructDepthMap);
    connect(ui->btnGenerateDefects,  &QPushButton::clicked, this, &MainWindow::onGenerateDefects);
    connect(ui->chkShowDefectBounds, &QCheckBox::toggled,   this, [this]{ renderDefectPreview(); });
    connect(ui->btnBrowseOutput,     &QPushButton::clicked, this, &MainWindow::onBrowseOutputDir);
    connect(ui->btnExportDataset,    &QPushButton::clicked, this, &MainWindow::onExportDataset);
}

// ─── Tab 1: Focal Capture ─────────────────────────────────────────────────────

void MainWindow::onConnectCamera()
{
    // Disconnect existing session if any
    if (m_camera && m_camera->isConnected()) m_camera->disconnect();
    if (m_lens   && m_lens->isConnected())   m_lens->disconnect();

    // Instantiate the selected camera backend
    if (ui->radioMockSynthetic->isChecked()) {
        m_camera = std::make_unique<MockCameraModule>();
        m_lens   = std::make_unique<MockLiquidLensController>();

    } else if (ui->radioMockImages->isChecked()) {
        QString folder = ui->lineImageFolder->text().trimmed();
        if (folder.isEmpty()) {
            QMessageBox::warning(this, "No Folder Selected",
                "Browse to a folder containing your focal stack images first.");
            return;
        }
        m_camera = std::make_unique<FileCameraModule>(folder);
        m_lens   = std::make_unique<MockLiquidLensController>();

    } else {
        // Real Camera — SVS-VISTEK GigE + Optotune EL-16-40 via Lens Driver
        m_camera = std::make_unique<RealCameraModule>();
#ifdef SDG_HAVE_SERIAL_PORT
        QString port = ui->comboComPort->currentData().toString();
        if (port.isEmpty()) {
            QMessageBox::warning(this, "No COM Port Selected",
                "Select the Optotune Lens Driver COM port and click Refresh if none appear.");
            return;
        }
        m_lens = std::make_unique<RealLiquidLensController>(port);
#else
        m_lens = std::make_unique<MockLiquidLensController>();
        logMessage("WARNING: Qt SerialPort not installed — lens sweep uses mock control only.");
#endif
    }

    bool camOk  = m_camera->connect();
    bool lensOk = m_lens->connect();

    if (camOk && lensOk) {
        // Sync image count to folder size when using file camera
        if (ui->radioMockImages->isChecked()) {
            auto* fileCam = dynamic_cast<FileCameraModule*>(m_camera.get());
            if (fileCam && fileCam->imageCount() > 0)
                ui->spinImageCount->setValue(fileCam->imageCount());
        }
        updateCameraStatus(true);
        logMessage(QString("Connected — Camera: %1 | Lens: %2")
            .arg(m_camera->deviceName(), m_lens->deviceName()));
    } else {
        updateCameraStatus(false);
        logMessage("ERROR: Failed to connect camera or lens.");
        QMessageBox::critical(this, "Connection Error",
            "Could not connect to camera or liquid lens controller.\n"
            "Check that the device is powered on and the folder path is valid.");
    }
}

void MainWindow::onBrowseImageFolder()
{
    QString dir = QFileDialog::getExistingDirectory(
        this, "Select Focal Stack Image Folder",
        QDir::homePath(), QFileDialog::ShowDirsOnly);
    if (!dir.isEmpty())
        ui->lineImageFolder->setText(dir);
}

void MainWindow::onCameraSourceChanged()
{
    bool isFileMode = ui->radioMockImages->isChecked();
    bool isRealMode = ui->radioRealCamera->isChecked();

    ui->lineImageFolder->setEnabled(isFileMode);
    ui->btnBrowseImageFolder->setEnabled(isFileMode);

    ui->lblComPort->setEnabled(isRealMode);
    ui->comboComPort->setEnabled(isRealMode);
    ui->btnRefreshPorts->setEnabled(isRealMode);
}

void MainWindow::onRefreshComPorts()
{
    ui->comboComPort->clear();
#ifdef SDG_HAVE_SERIAL_PORT
    const auto ports = QSerialPortInfo::availablePorts();
    for (const QSerialPortInfo& info : ports) {
        QString label = info.portName();
        if (!info.description().isEmpty())
            label += "  —  " + info.description();
        ui->comboComPort->addItem(label, info.portName());
    }
    if (ports.isEmpty())
        ui->comboComPort->addItem("No COM ports found");
#else
    ui->comboComPort->addItem("Install Qt SerialPort module to enable");
#endif
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

    logMessage(QString("Starting focal sweep: %1 frames, %2 D → %3 D")
        .arg(params.imageCount)
        .arg(params.startDioptre, 0, 'f', 2)
        .arg(params.endDioptre,   0, 'f', 2));

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
        // Store first result for toggle re-render
        m_previewDefectImage  = m_defectGenerator->getOutputImages().front();
        m_previewDefectBounds = m_defectGenerator->getOutputBounds().front();
        const DefectType t    = m_defectGenerator->getOutputLabels().front();
        switch (t) {
            case DefectType::Scratch:    m_previewDefectType = "Scratch";     break;
            case DefectType::ShallowDent:m_previewDefectType = "Shallow Dent";break;
            case DefectType::Crack:      m_previewDefectType = "Crack";       break;
            case DefectType::SurfacePit: m_previewDefectType = "Surface Pit"; break;
        }
        ui->lblDefectTypeTag->setText(m_previewDefectType);
        renderDefectPreview();
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

void MainWindow::renderDefectPreview()
{
    if (m_previewDefectImage.empty()) return;

    // Convert depth map to 8-bit JET colourmap (same as showMatInLabel)
    cv::Mat display;
    cv::Mat norm;
    cv::normalize(m_previewDefectImage, norm, 0, 255, cv::NORM_MINMAX);
    norm.convertTo(display, CV_8U);
    cv::applyColorMap(display, display, cv::COLORMAP_JET);

    // Overlay the bounding box when the toggle is on
    if (ui->chkShowDefectBounds->isChecked() && m_previewDefectBounds.area() > 0) {
        // Bright yellow box (BGR)
        cv::rectangle(display, m_previewDefectBounds, cv::Scalar(0, 230, 255), 2);

        // Small label tag above the box
        cv::Point textPos(m_previewDefectBounds.x,
                          std::max(0, m_previewDefectBounds.y - 6));
        cv::putText(display, m_previewDefectType.toStdString(),
                    textPos, cv::FONT_HERSHEY_SIMPLEX, 0.45,
                    cv::Scalar(0, 0, 0), 3, cv::LINE_AA);          // black outline
        cv::putText(display, m_previewDefectType.toStdString(),
                    textPos, cv::FONT_HERSHEY_SIMPLEX, 0.45,
                    cv::Scalar(0, 230, 255), 1, cv::LINE_AA);      // yellow text
    }

    QImage img(display.data, display.cols, display.rows,
               static_cast<int>(display.step), QImage::Format_RGB888);
    ui->lblDefectPreview->setPixmap(
        QPixmap::fromImage(img).scaled(ui->lblDefectPreview->size(),
                                       Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

void MainWindow::logMessage(const QString& message)
{
    QString ts = QDateTime::currentDateTime().toString("hh:mm:ss");
    ui->textLog->appendPlainText(QString("[%1] %2").arg(ts, message));
}
