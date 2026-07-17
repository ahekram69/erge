#pragma once

#include <QCameraDevice>
#include <QMainWindow>
#include <QMediaCaptureSession>
#include <QMediaDevices>

#include <memory>

class QCamera;
class QCheckBox;
class QComboBox;
class QLabel;
class QPushButton;
class QSlider;
class QVBoxLayout;
class NativeCameraControls;
class QMediaRecorder;
class VideoPreviewWidget;

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
    void takeSnapshot();
    void toggleRecording();
    void updateRecorderState();
    void showRecorderError();

private:
    void buildUi();
    void requestCameraPermission();
    void openCamera(const QCameraDevice &device);
    void populateFormats(const QCameraDevice &device);
    void rebuildNativeControls();
    void saveNativePreset();
    void loadNativePreset();
    void resetNativeControls();
    void setConnectionBadge(const QString &text, const QString &state);
    QString presetGroup() const;
    void syncControls();
    QString formatLabel(const QCameraFormat &format) const;

    QMediaDevices m_mediaDevices;
    QMediaCaptureSession m_captureSession;
    std::unique_ptr<QCamera> m_camera;
    std::unique_ptr<NativeCameraControls> m_nativeControls;
    std::unique_ptr<QMediaRecorder> m_recorder;

    VideoPreviewWidget *m_videoWidget = nullptr;
    QComboBox *m_deviceCombo = nullptr;
    QComboBox *m_formatCombo = nullptr;
    QPushButton *m_startButton = nullptr;
    QPushButton *m_snapshotButton = nullptr;
    QPushButton *m_recordButton = nullptr;
    QLabel *m_statusLabel = nullptr;
    QLabel *m_connectionBadge = nullptr;
    QCheckBox *m_mirrorCheck = nullptr;
    QCheckBox *m_verticalCheck = nullptr;
    QComboBox *m_rotationCombo = nullptr;
    QSlider *m_exposureSlider = nullptr;
    QLabel *m_exposureValue = nullptr;
    QSlider *m_zoomSlider = nullptr;
    QLabel *m_zoomValue = nullptr;
    QVBoxLayout *m_nativeControlsLayout = nullptr;
    QString m_currentDeviceName;
    bool m_receivedFrame = false;
};
