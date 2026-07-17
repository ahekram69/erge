#include "MainWindow.h"

#include <QApplication>
#include <QCoreApplication>
#include <QStyleFactory>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QCoreApplication::setApplicationName("USB Camera Control");
    QCoreApplication::setOrganizationName("CameraTools");
    QCoreApplication::setApplicationVersion(QStringLiteral(APP_VERSION));
    app.setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
    app.setStyleSheet(QStringLiteral(R"(
        * {
            font-family: "Segoe UI", "Microsoft YaHei UI", "PingFang SC";
            font-size: 13px;
            color: #e8edf5;
        }
        QMainWindow, QWidget#appRoot { background: #0b0f15; }
        QFrame#headerBar {
            background: #121923;
            border: 1px solid #202a38;
            border-radius: 12px;
        }
        QLabel#appTitle { font-size: 20px; font-weight: 700; color: #ffffff; }
        QLabel#appSubtitle { color: #8e9aad; }
        QLabel#connectionBadge {
            background: #253044;
            color: #b7c3d6;
            border-radius: 14px;
            padding: 7px 12px;
            font-weight: 600;
        }
        QLabel#connectionBadge[state="ready"] { background: #153b2d; color: #69e6ae; }
        QLabel#connectionBadge[state="error"] { background: #472229; color: #ff8c98; }
        QScrollArea, QScrollArea > QWidget > QWidget { background: transparent; }
        QGroupBox {
            background: #121923;
            border: 1px solid #202a38;
            border-radius: 10px;
            margin-top: 14px;
            padding: 16px 12px 12px 12px;
            font-weight: 700;
            color: #f4f7fb;
        }
        QGroupBox::title { subcontrol-origin: margin; left: 12px; padding: 0 5px; }
        QComboBox, QPushButton {
            min-height: 34px;
            background: #1a2330;
            border: 1px solid #2b3748;
            border-radius: 7px;
            padding: 0 10px;
        }
        QComboBox:hover, QPushButton:hover { border-color: #4f81ff; background: #202c3c; }
        QPushButton:pressed { background: #17202c; }
        QPushButton#primaryButton { background: #376cff; border-color: #4f81ff; font-weight: 700; }
        QPushButton#primaryButton:hover { background: #4779ff; }
        QPushButton#recordButton[recording="true"] { background: #d83b4b; border-color: #f15b68; }
        QComboBox::drop-down { border: none; width: 24px; }
        QCheckBox { spacing: 8px; min-height: 25px; }
        QCheckBox::indicator { width: 17px; height: 17px; }
        QSlider::groove:horizontal { height: 5px; background: #273244; border-radius: 2px; }
        QSlider::sub-page:horizontal { background: #4f81ff; border-radius: 2px; }
        QSlider::handle:horizontal {
            width: 15px; margin: -5px 0; border-radius: 7px;
            background: #f4f7fb; border: 2px solid #4f81ff;
        }
        QLabel#statusCard {
            background: #111822;
            border: 1px solid #253044;
            border-radius: 8px;
            padding: 10px;
            color: #aeb9ca;
        }
        QStatusBar { background: #0b0f15; color: #748197; }
        QToolTip { background: #1c2634; color: white; border: 1px solid #34445b; }
    )"));

    MainWindow window;
    window.show();
    return app.exec();
}
