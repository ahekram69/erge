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
            color: #1d1d1f;
        }
        QMainWindow, QWidget#appRoot { background: #f5f5f7; }
        QFrame#headerBar {
            background: #ffffff;
            border: 1px solid #dedee3;
            border-radius: 14px;
        }
        QLabel#appTitle { font-size: 20px; font-weight: 700; color: #1d1d1f; }
        QLabel#appSubtitle { color: #6e6e73; }
        QLabel#secondaryText { color: #6e6e73; }
        QLabel#connectionBadge {
            background: #eeeeF2;
            color: #6e6e73;
            border-radius: 14px;
            padding: 7px 12px;
            font-weight: 600;
        }
        QLabel#connectionBadge[state="ready"] { background: #e5f5eb; color: #18794e; }
        QLabel#connectionBadge[state="error"] { background: #fdebec; color: #c93442; }
        QScrollArea, QScrollArea > QWidget > QWidget { background: transparent; }
        QFrame#sidePanel { background: transparent; }
        QGroupBox {
            background: #ffffff;
            border: 1px solid #dedee3;
            border-radius: 12px;
            margin-top: 14px;
            padding: 16px 12px 12px 12px;
            font-weight: 700;
            color: #1d1d1f;
        }
        QGroupBox::title { subcontrol-origin: margin; left: 12px; padding: 0 5px; }
        QTabWidget#controlTabs::pane { border: none; background: transparent; }
        QTabBar::tab {
            min-width: 112px;
            padding: 9px 14px;
            margin: 0 2px;
            background: #e8e8ed;
            color: #6e6e73;
            border: none;
            border-radius: 9px;
            font-weight: 600;
        }
        QTabBar::tab:selected { background: #ffffff; color: #0071e3; }
        QTabBar::tab:hover:!selected { background: #dedee3; color: #1d1d1f; }
        QComboBox, QPushButton, QSpinBox {
            min-height: 34px;
            background: #ffffff;
            border: 1px solid #d2d2d7;
            border-radius: 8px;
            padding: 0 10px;
        }
        QComboBox:hover, QPushButton:hover, QSpinBox:hover { border-color: #0071e3; background: #f7f9fc; }
        QPushButton:pressed { background: #ececf1; }
        QPushButton#primaryButton { background: #0071e3; border-color: #0071e3; color: white; font-weight: 700; }
        QPushButton#primaryButton:hover { background: #0077ed; }
        QPushButton#recordButton[recording="true"] { background: #e43d4c; border-color: #e43d4c; color: white; }
        QComboBox::drop-down { border: none; width: 24px; }
        QCheckBox { spacing: 8px; min-height: 25px; }
        QCheckBox::indicator {
            width: 17px; height: 17px;
            background: #ffffff;
            border: 1px solid #c7c7cc;
            border-radius: 4px;
        }
        QCheckBox::indicator:checked { background: #0071e3; border-color: #0071e3; }
        QSlider::groove:horizontal { height: 5px; background: #d9d9df; border-radius: 2px; }
        QSlider::sub-page:horizontal { background: #0071e3; border-radius: 2px; }
        QSlider::handle:horizontal {
            width: 15px; margin: -5px 0; border-radius: 7px;
            background: #ffffff; border: 2px solid #0071e3;
        }
        QLabel#statusCard {
            background: #ffffff;
            border: 1px solid #dedee3;
            border-radius: 10px;
            padding: 10px;
            color: #6e6e73;
        }
        QStatusBar { background: #f5f5f7; color: #86868b; }
        QToolTip { background: #ffffff; color: #1d1d1f; border: 1px solid #d2d2d7; }
    )"));

    MainWindow window;
    window.show();
    return app.exec();
}
