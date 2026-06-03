#include "MainWindow.h"
#include "ui_MainWindow.h"

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QMessageBox>
#include <QDateTime>
#include <QPixmap>
#include <QImage>
#include <algorithm>

#include <opencv2/imgproc.hpp>

#include "core/focal_stack/FocalStackProcessor.h"
#include "core/image_registration/ImageRegistrator.h"
#include "core/depth_map/DepthMapReconstructor.h"
#include "core/defect_gen/DefectGenerator.h"
#include "core/defect_gen/DefectTypes.h"

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
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
    logMessage("Application started. Select a folder of focal stack images to begin.");
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::setupConnections()
{
    connect(ui->btnBrowseStack, &QPushButton::clicked, this, &MainWindow::onBrowseStack);
    connect(ui->btnLoadStack,   &QPushButton::clicked, this, &MainWindow::onLoadStack);

    connect(ui->btnRegisterStack,    &QPushButton::clicked, this, &MainWindow::onRegisterStack);
    connect(ui->btnReconstructDepth, &QPushButton::clicked, this, &MainWindow::onReconstructDepthMap);
    connect(ui->btnGenerateDefects,  &QPushButton::clicked, this, &MainWindow::onGenerateDefects);
    connect(ui->chkShowDefectBounds, &QCheckBox::toggled,   this, [this]{ renderDefectPreview(); });
    connect(ui->btnBrowseOutput,     &QPushButton::clicked, this, &MainWindow::onBrowseOutputDir);
    connect(ui->btnExportDataset,    &QPushButton::clicked, this, &MainWindow::onExportDataset);
}

// ─── Tab 1: Focal Stack Input ─────────────────────────────────────────────────

void MainWindow::onBrowseStack()
{
    QString dir = QFileDialog::getExistingDirectory(
        this, "Select Focal Stack Image Folder",
        QDir::homePath(), QFileDialog::ShowDirsOnly);
    if (dir.isEmpty()) return;

    ui->lineStackFolder->setText(dir);

    // Count supported images in the folder
    const QStringList filters = {"*.png", "*.jpg", "*.jpeg", "*.bmp", "*.tiff", "*.tif"};
    QStringList files = QDir(dir).entryList(filters, QDir::Files, QDir::Name);
    int count = files.size();

    if (count == 0) {
        ui->lblImageCount->setText("No supported images found in this folder.");
        ui->lblImageCount->setStyleSheet("color: #e05050;");
        ui->lblCsvStatus->setText("—");
        ui->lblCsvStatus->setStyleSheet("color: #888;");
        ui->btnLoadStack->setEnabled(false);
    } else {
        ui->lblImageCount->setText(QString("%1 image(s) found — will load in filename order.").arg(count));
        ui->lblImageCount->setStyleSheet("color: #50c050;");
        ui->btnLoadStack->setEnabled(true);

        // Detect metadata.csv in the selected folder
        if (QFile::exists(dir + "/metadata.csv")) {
            ui->lblCsvStatus->setText("metadata.csv detected — focal power data will be loaded.");
            ui->lblCsvStatus->setStyleSheet("color: #50c050;");
        } else {
            ui->lblCsvStatus->setText("No metadata.csv — depth will use uniform focal spacing.");
            ui->lblCsvStatus->setStyleSheet("color: #e0a050;");
        }
    }
}

void MainWindow::onLoadStack()
{
    QString folder = ui->lineStackFolder->text().trimmed();
    if (folder.isEmpty()) return;

    logMessage(QString("Loading focal stack from: %1").arg(folder));
    setControlsEnabled(false);
    m_progressBar->setVisible(true);

    bool ok = m_focalProcessor->loadFromFolder(folder,
        [this](int pct, const QString& msg){ onOperationProgress(pct, msg); });

    if (ok) {
        const auto& stack = m_focalProcessor->getStack();
        showMatInLabel(ui->lblCapturePreview, stack[stack.size() / 2]);

        QString status = QString("Stack loaded: %1 frames.").arg(stack.size());
        ui->lblStackStatus->setText(status);
        ui->lblStackStatus->setStyleSheet("color: #50c050; font-weight: bold;");

        // Update CSV status label with the actual loaded focal range
        if (m_focalProcessor->hasFocalPowers()) {
            const auto& fp = m_focalProcessor->getFocalPowers();
            float minFp = *std::min_element(fp.begin(), fp.end());
            float maxFp = *std::max_element(fp.begin(), fp.end());
            ui->lblCsvStatus->setText(
                QString("Focal range: %1 to %2 diopters (%3 frames)")
                .arg(minFp, 0, 'f', 1).arg(maxFp, 0, 'f', 1)
                .arg(static_cast<int>(fp.size())));
            ui->lblCsvStatus->setStyleSheet("color: #50c050;");
        }

        onOperationComplete(status);
        ui->tabWidget->setTabEnabled(1, true);
    } else {
        ui->lblStackStatus->setText("Failed to load stack.");
        ui->lblStackStatus->setStyleSheet("color: #e05050;");
        onOperationError("Could not load images from the selected folder. "
                         "Check that the folder contains valid image files.");
    }
}

// ─── Tab 2: Image Registration ───────────────────────────────────────────────

void MainWindow::onRegisterStack()
{
    if (!m_focalProcessor->hasStack()) {
        QMessageBox::warning(this, "No Stack", "Load a focal stack first.");
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

    bool ok = m_depthReconstructor->reconstruct(
        m_focalProcessor->getStack(), params,
        m_focalProcessor->getFocalPowers(),
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
        m_previewDefectImage  = m_defectGenerator->getOutputImages().front();
        m_previewDefectBounds = m_defectGenerator->getOutputBounds().front();
        const DefectType t    = m_defectGenerator->getOutputLabels().front();
        switch (t) {
            case DefectType::Scratch:    m_previewDefectType = "Scratch";      break;
            case DefectType::ShallowDent:m_previewDefectType = "Shallow Dent"; break;
            case DefectType::Crack:      m_previewDefectType = "Crack";        break;
            case DefectType::SurfacePit: m_previewDefectType = "Surface Pit";  break;
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

void MainWindow::setControlsEnabled(bool enabled)
{
    ui->btnBrowseStack->setEnabled(enabled);
    ui->btnLoadStack->setEnabled(enabled && !ui->lineStackFolder->text().isEmpty());
    ui->btnRegisterStack->setEnabled(enabled);
    ui->btnReconstructDepth->setEnabled(enabled);
    ui->btnGenerateDefects->setEnabled(enabled);
    ui->btnExportDataset->setEnabled(enabled);
}

void MainWindow::showMatInLabel(ZoomableImageLabel* label, const cv::Mat& mat)
{
    if (mat.empty()) return;

    cv::Mat display;
    if (mat.type() == CV_32F) {
        // Build a mask of non-zero (object) pixels — zeros are masked background.
        cv::Mat objectMask;
        cv::threshold(mat, objectMask, 0.001f, 255.0f, cv::THRESH_BINARY);
        objectMask.convertTo(objectMask, CV_8U);

        // Normalize over the object's actual depth range, not the full [0, max]
        // range. This prevents the background zeros from compressing all object
        // variation into the red end of the Jet colormap.
        double minVal = 0.0, maxVal = 1.0;
        cv::minMaxLoc(mat, &minVal, &maxVal, nullptr, nullptr, objectMask);
        double range = maxVal - minVal;

        cv::Mat norm;
        if (range > 1e-6)
            mat.convertTo(norm, CV_32F, 255.0 / range, -minVal * 255.0 / range);
        else
            cv::normalize(mat, norm, 0, 255, cv::NORM_MINMAX);

        norm.convertTo(display, CV_8U);
        cv::applyColorMap(display, display, cv::COLORMAP_JET);

        // Set background pixels to black rather than Jet's dark-blue zero-colour.
        cv::Mat bgMask;
        cv::bitwise_not(objectMask, bgMask);
        display.setTo(cv::Scalar(0, 0, 0), bgMask);
    } else {
        mat.copyTo(display);
    }

    if (display.channels() == 1)
        cv::cvtColor(display, display, cv::COLOR_GRAY2RGB);

    QImage img(display.data, display.cols, display.rows,
               static_cast<int>(display.step),
               QImage::Format_RGB888);

    label->setPixmap(QPixmap::fromImage(img.copy()));
}

void MainWindow::renderDefectPreview()
{
    if (m_previewDefectImage.empty()) return;

    cv::Mat display;
    cv::Mat norm;
    cv::normalize(m_previewDefectImage, norm, 0, 255, cv::NORM_MINMAX);
    norm.convertTo(display, CV_8U);
    cv::applyColorMap(display, display, cv::COLORMAP_JET);

    if (ui->chkShowDefectBounds->isChecked() && m_previewDefectBounds.area() > 0) {
        cv::rectangle(display, m_previewDefectBounds, cv::Scalar(0, 230, 255), 2);

        cv::Point textPos(m_previewDefectBounds.x,
                          std::max(0, m_previewDefectBounds.y - 6));
        cv::putText(display, m_previewDefectType.toStdString(),
                    textPos, cv::FONT_HERSHEY_SIMPLEX, 0.45,
                    cv::Scalar(0, 0, 0), 3, cv::LINE_AA);
        cv::putText(display, m_previewDefectType.toStdString(),
                    textPos, cv::FONT_HERSHEY_SIMPLEX, 0.45,
                    cv::Scalar(0, 230, 255), 1, cv::LINE_AA);
    }

    QImage img(display.data, display.cols, display.rows,
               static_cast<int>(display.step), QImage::Format_RGB888);
    ui->lblDefectPreview->setPixmap(QPixmap::fromImage(img));
}

void MainWindow::logMessage(const QString& message)
{
    QString ts = QDateTime::currentDateTime().toString("hh:mm:ss");
    ui->textLog->appendPlainText(QString("[%1] %2").arg(ts, message));
}
