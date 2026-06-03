#include <QApplication>
#include <QStyleFactory>
#include "ui/MainWindow.h"

static const char* kStyleSheet = R"(
QMainWindow, QDialog { background: #f4f5fb; }

QTabWidget::pane {
    border: 1px solid #d0d4ec;
    background: #ffffff;
    border-radius: 0 4px 4px 4px;
}
QTabBar { background: transparent; }
QTabBar::tab {
    background: #eceef8; color: #8890c0;
    padding: 9px 20px; margin-right: 2px;
    border: 1px solid #d0d4ec; border-bottom: none;
    border-radius: 4px 4px 0 0;
    font-weight: bold; font-size: 9pt;
}
QTabBar::tab:selected {
    background: #ffffff; color: #1e2448;
    border-color: #c0c8e8; border-bottom: 1px solid #ffffff;
}
QTabBar::tab:hover:!selected { background: #f0f2fc; color: #3a50a8; }
QTabBar::tab:disabled { color: #c0c4dc; }

QGroupBox {
    background: #f8f9fe; border: 1px solid #d8daf0;
    border-radius: 6px; margin-top: 20px; padding: 8px 6px 6px 6px;
}
QGroupBox::title {
    subcontrol-origin: margin; left: 10px; top: -1px;
    padding: 0 6px; color: #8090c0;
    font-size: 8pt; font-weight: bold;
}

QPushButton {
    background: #eef0fb; color: #4858a8;
    border: 1px solid #c8cce8; border-radius: 5px;
    padding: 6px 14px; font-weight: 600; min-height: 26px;
}
QPushButton:hover { background: #e4e8f8; border-color: #a0a8d8; color: #2838a0; }
QPushButton:pressed { background: #d8ddf5; border-color: #3a5bc0; }
QPushButton:disabled { background: #f4f5fb; color: #c0c4dc; border-color: #e0e2f0; }

QPushButton#btnLoadStack, QPushButton#btnRegisterStack,
QPushButton#btnReconstructDepth, QPushButton#btnGenerateDefects,
QPushButton#btnExportDataset {
    background: #3a5bc0; color: #ffffff; border-color: #2a4ab0;
}
QPushButton#btnLoadStack:hover, QPushButton#btnRegisterStack:hover,
QPushButton#btnReconstructDepth:hover, QPushButton#btnGenerateDefects:hover,
QPushButton#btnExportDataset:hover {
    background: #4a6ed0; color: #ffffff; border-color: #3858c8;
}
QPushButton#btnLoadStack:disabled, QPushButton#btnRegisterStack:disabled,
QPushButton#btnReconstructDepth:disabled, QPushButton#btnGenerateDefects:disabled,
QPushButton#btnExportDataset:disabled {
    background: #c0c8e8; color: #8090b8; border-color: #b0b8d8;
}

QLineEdit, QSpinBox, QDoubleSpinBox, QComboBox {
    background: #ffffff; border: 1px solid #d0d4ec;
    border-radius: 4px; color: #1e2448;
    padding: 4px 7px; min-height: 22px;
    selection-background-color: #b0c0f0;
}
QLineEdit:focus, QSpinBox:focus, QDoubleSpinBox:focus, QComboBox:focus {
    border-color: #3a5bc0;
}
QSpinBox::up-button, QDoubleSpinBox::up-button {
    background: #f0f2fb; border: none; width: 16px; border-radius: 0 3px 0 0;
}
QSpinBox::down-button, QDoubleSpinBox::down-button {
    background: #f0f2fb; border: none; width: 16px; border-radius: 0 0 3px 0;
}
QComboBox::drop-down { border: none; width: 22px; }
QComboBox QAbstractItemView {
    background: #ffffff; border: 1px solid #c8cce8;
    selection-background-color: #d0d8f8; color: #1e2448; outline: none;
}

QSlider::groove:horizontal {
    background: #dde0f0; height: 4px; border-radius: 2px;
}
QSlider::handle:horizontal {
    background: #3a5bc0; width: 13px; height: 13px;
    margin: -5px 0; border-radius: 7px;
}
QSlider::sub-page:horizontal { background: #7090d8; border-radius: 2px; }

QCheckBox { color: #3848a0; spacing: 7px; }
QCheckBox::indicator {
    width: 14px; height: 14px;
    border: 1px solid #c0c8e0; border-radius: 3px; background: #ffffff;
}
QCheckBox::indicator:checked { background: #3a5bc0; border-color: #2a4ab0; }
QCheckBox::indicator:hover   { border-color: #3a5bc0; }

QProgressBar {
    background: #e8eaf8; border: 1px solid #d0d4ec;
    border-radius: 3px; color: #5060a8; font-size: 8pt;
    text-align: center; max-height: 10px;
}
QProgressBar::chunk {
    background: qlineargradient(x1:0,y1:0,x2:1,y2:0,
                stop:0 #3a5bc0, stop:1 #7090d8);
    border-radius: 3px;
}

QPlainTextEdit {
    background: #f8f9fe; border: 1px solid #d8daf0;
    border-radius: 4px; color: #3848a0;
    font-family: Consolas, 'Courier New', monospace; font-size: 9pt;
}

QScrollBar:vertical   { background: #f0f2fb; width: 7px;  border: none; }
QScrollBar:horizontal { background: #f0f2fb; height: 7px; border: none; }
QScrollBar::handle:vertical, QScrollBar::handle:horizontal {
    background: #c0c8e8; border-radius: 3px; min-height: 20px; min-width: 20px;
}
QScrollBar::handle:vertical:hover, QScrollBar::handle:horizontal:hover {
    background: #a0acd8;
}
QScrollBar::add-line, QScrollBar::sub-line { width: 0; height: 0; }

QMenuBar {
    background: #eceef8; color: #5060a8;
    border-bottom: 1px solid #d0d4ec; padding: 1px 4px;
}
QMenuBar::item:selected { background: #dde0f8; color: #1e2448; }
QMenu {
    background: #ffffff; border: 1px solid #d0d4ec; color: #1e2448; padding: 4px;
}
QMenu::item { padding: 5px 20px; border-radius: 3px; }
QMenu::item:selected { background: #d8e0f8; }

QStatusBar {
    background: #eceef8; border-top: 1px solid #d0d4ec;
    color: #7080b8; font-size: 8pt;
}
QStatusBar::item { border: none; }

QLabel { color: #6878b0; background: transparent; }

QFrame[frameShape="4"], QFrame[frameShape="5"] {
    color: #d8daf0; max-height: 1px;
}

QToolTip {
    background: #ffffff; border: 1px solid #3a5bc0;
    color: #1e2448; padding: 4px 8px; border-radius: 4px;
}
)";

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    app.setApplicationName("Synthetic Defect Generator");
    app.setApplicationVersion("0.3.0");
    app.setOrganizationName("JM Vistec System");
    app.setStyle(QStyleFactory::create("Fusion"));
    app.setStyleSheet(kStyleSheet);

    MainWindow window;
    window.show();

    return app.exec();
}
