#include <QApplication>
#include <QStyleFactory>
#include "ui/MainWindow.h"

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    app.setApplicationName("Synthetic Defect Generator");
    app.setApplicationVersion("0.1.0");
    app.setOrganizationName("JM Vistec System");
    app.setStyle(QStyleFactory::create("Fusion"));

    MainWindow window;
    window.show();

    return app.exec();
}
