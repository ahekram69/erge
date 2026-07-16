#include "MainWindow.h"

#include <QApplication>
#include <QCoreApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QCoreApplication::setApplicationName("USB Camera Control");
    QCoreApplication::setOrganizationName("CameraTools");

    MainWindow window;
    window.show();
    return app.exec();
}
