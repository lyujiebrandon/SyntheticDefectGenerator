#include <QApplication>
#include <QStyleFactory>
#include "ui/MainWindow.h"

static const char* kStyleSheet = R"(
QMainWindow, QDialog { background: #0d0d18; }

QTabWidget::pane {
    border: 1px solid #252538;
    background: #121220;
    border-radius: 0 4px 4px 4px;
}
QTabBar { background: transparent; }
QTabBar::tab {
    background: #181826; color: #4a4e70;
    padding: 9px 20px; margin-right: 2px;
    border: 1px solid #1e1e30; border-bottom: none;
    border-radius: 4px 4px 0 0;
    font-weight: bold; font-size: 9pt;
}
QTabBar::tab:selected {
    background: #121220; color: #c4cdf5;
    border-color: #303055; border-bottom: 1px solid #121220;
}
QTabBar::tab:hover:!selected { background: #1a1a2c; color: #8890c0; }
QTabBar::tab:disabled { color: #252535; }

QGroupBox {
    background: #191928; border: 1px solid #222235;
    border-radius: 6px; margin-top: 20px; padding: 8px 6px 6px 6px;
}
QGroupBox::title {
    subcontrol-origin: margin; left: 10px; top: -1px;
    padding: 0 6px; color: #404468;
    font-size: 8pt; font-weight: bold;
}

QPushButton {
    background: #1c1c2e; color: #8890c8;
    border: 1px solid #282840; border-radius: 5px;
    padding: 6px 14px; font-weight: 600; min-height: 26px;
}
QPushButton:hover { background: #212135; border-color: #383860; color: #b0bcf0; }
QPushButton:pressed { background: #161628; border-color: #4a72d9; }
QPushButton:disabled { background: #131320; color: #2c2c44; border-color: #181828; }

QPushButton#btnLoadStack, QPushButton#btnRegisterStack,
QPushButton#btnReconstructDepth, QPushButton#btnGenerateDefects,
QPushButton#btnExportDataset {
    background: #172050; color: #7898e8;
    border-color: #253070;
}
QPushButton#btnLoadStack:hover, QPushButton#btnRegisterStack:hover,
QPushButton#btnReconstructDepth:hover, QPushButton#btnGenerateDefects:hover,
QPushButton#btnExportDataset:hover {
    background: #1c2860; color: #a0bcff; border-color: #384898;
}
QPushButton#btnLoadStack:disabled, QPushButton#btnRegisterStack:disabled,
QPushButton#btnReconstructDepth:disabled, QPushButton#btnGenerateDefects:disabled,
QPushButton#btnExportDataset:disabled {
    background: #0f1428; color: #222c48; border-color: #141c38;
}

QLineEdit, QSpinBox, QDoubleSpinBox, QComboBox {
    background: #0e0e1c; border: 1px solid #242438;
    border-radius: 4px; color: #b0b8e0;
    padding: 4px 7px; min-height: 22px;
    selection-background-color: #28408a;
}
QLineEdit:focus, QSpinBox:focus, QDoubleSpinBox:focus, QComboBox:focus {
    border-color: #4a72d9;
}
QSpinBox::up-button, QDoubleSpinBox::up-button {
    background: #1c1c2e; border: none; width: 16px; border-radius: 0 3px 0 0;
}
QSpinBox::down-button, QDoubleSpinBox::down-button {
    background: #1c1c2e; border: none; width: 16px; border-radius: 0 0 3px 0;
}
QComboBox::drop-down { border: none; width: 22px; }
QComboBox QAbstractItemView {
    background: #15152a; border: 1px solid #2e2e4e;
    selection-background-color: #223070; color: #b0b8e0; outline: none;
}

QSlider::groove:horizontal {
    background: #1a1a2e; height: 4px; border-radius: 2px;
}
QSlider::handle:horizontal {
    background: #4a72d9; width: 13px; height: 13px;
    margin: -5px 0; border-radius: 7px;
}
QSlider::sub-page:horizontal { background: #2a4490; border-radius: 2px; }

QCheckBox { color: #8890c0; spacing: 7px; }
QCheckBox::indicator {
    width: 14px; height: 14px;
    border: 1px solid #2c2c48; border-radius: 3px; background: #0e0e1c;
}
QCheckBox::indicator:checked { background: #2a4490; border-color: #4a72d9; }
QCheckBox::indicator:hover   { border-color: #3c56b0; }

QProgressBar {
    background: #0e0e1c; border: 1px solid #242438;
    border-radius: 3px; color: #60688a; font-size: 8pt;
    text-align: center; max-height: 10px;
}
QProgressBar::chunk {
    background: qlineargradient(x1:0,y1:0,x2:1,y2:0,
                stop:0 #1c2e80, stop:1 #4a72d9);
    border-radius: 3px;
}

QPlainTextEdit {
    background: #080812; border: 1px solid #191928;
    border-radius: 4px; color: #585c80;
    font-family: Consolas, 'Courier New', monospace; font-size: 9pt;
}

QScrollBar:vertical   { background: #0d0d18; width: 7px;  border: none; }
QScrollBar:horizontal { background: #0d0d18; height: 7px; border: none; }
QScrollBar::handle:vertical, QScrollBar::handle:horizontal {
    background: #232338; border-radius: 3px; min-height: 20px; min-width: 20px;
}
QScrollBar::handle:vertical:hover, QScrollBar::handle:horizontal:hover {
    background: #303055;
}
QScrollBar::add-line, QScrollBar::sub-line { width: 0; height: 0; }

QMenuBar {
    background: #090912; color: #60688a;
    border-bottom: 1px solid #181828; padding: 1px 4px;
}
QMenuBar::item:selected { background: #181830; color: #c4cdf5; }
QMenu {
    background: #121225; border: 1px solid #242438; color: #a8b0d8; padding: 4px;
}
QMenu::item { padding: 5px 20px; border-radius: 3px; }
QMenu::item:selected { background: #1e2e68; }

QStatusBar {
    background: #080810; border-top: 1px solid #181828;
    color: #40445e; font-size: 8pt;
}
QStatusBar::item { border: none; }

QLabel { color: #70789a; background: transparent; }

QFrame[frameShape="4"], QFrame[frameShape="5"] {
    color: #1c1c2e; max-height: 1px;
}

QToolTip {
    background: #181830; border: 1px solid #4a72d9;
    color: #c4cdf5; padding: 4px 8px; border-radius: 4px;
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
