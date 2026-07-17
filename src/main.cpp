#include "MainWindow.h"

#include <QApplication>
#include <QCoreApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QCoreApplication::setApplicationName("USB Camera Control");
    QCoreApplication::setOrganizationName("CameraTools");
    QCoreApplication::setApplicationVersion(QStringLiteral(APP_VERSION));

    MainWindow window;
    window.show();
    return app.exec();
}
