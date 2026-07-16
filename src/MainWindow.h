#pragma once

#include <QCameraDevice>
#include <QMainWindow>
#include <QMediaCaptureSession>
#include <QMediaDevices>

#include <memory>

class QCamera;
class QComboBox;
class QLabel;
class QPushButton;
class QSlider;
class QVideoWidget;

class MainWindow final : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    void refreshDevices();
    void selectCamera(int index);
    void selectFormat(int index);
    void toggleCamera();
    void updateCameraState();
    void showCameraError();

private:
    void buildUi();
    void requestCameraPermission();
    void openCamera(const QCameraDevice &device);
    void populateFormats(const QCameraDevice &device);
    void syncControls();
    QString formatLabel(const QCameraFormat &format) const;

    QMediaDevices m_mediaDevices;
    QMediaCaptureSession m_captureSession;
    std::unique_ptr<QCamera> m_camera;

    QVideoWidget *m_videoWidget = nullptr;
    QComboBox *m_deviceCombo = nullptr;
    QComboBox *m_formatCombo = nullptr;
    QPushButton *m_startButton = nullptr;
    QLabel *m_statusLabel = nullptr;
    QSlider *m_exposureSlider = nullptr;
    QLabel *m_exposureValue = nullptr;
    QSlider *m_zoomSlider = nullptr;
    QLabel *m_zoomValue = nullptr;
    bool m_receivedFrame = false;
};
