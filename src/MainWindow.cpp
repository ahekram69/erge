#include "MainWindow.h"
#include "NativeCameraControls.h"

#include <QApplication>
#include <QCamera>
#include <QCameraFormat>
#include <QCheckBox>
#include <QPermissions>
#include <QComboBox>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSlider>
#include <QStatusBar>
#include <QTimer>
#include <QVBoxLayout>
#include <QVideoFrame>
#include <QVideoSink>
#include <QVideoWidget>

#include <algorithm>

namespace {
constexpr int SliderScale = 100;
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    m_nativeControls = createNativeCameraControls();
    buildUi();
    connect(&m_mediaDevices, &QMediaDevices::videoInputsChanged,
            this, &MainWindow::refreshDevices);
    requestCameraPermission();
}

MainWindow::~MainWindow() = default;

void MainWindow::requestCameraPermission()
{
    const QCameraPermission permission;
    const auto status = qApp->checkPermission(permission);

    if (status == Qt::PermissionStatus::Granted) {
        refreshDevices();
        return;
    }

    if (status == Qt::PermissionStatus::Denied) {
        m_statusLabel->setText(
            tr("摄像头权限已被拒绝。请在系统设置的“隐私与安全性 → 摄像头”中允许本软件。"));
        m_deviceCombo->setEnabled(false);
        m_formatCombo->setEnabled(false);
        m_startButton->setEnabled(false);
        return;
    }

    m_statusLabel->setText(tr("正在请求摄像头权限…"));
    qApp->requestPermission(permission, this, [this](const QPermission &result) {
        if (result.status() == Qt::PermissionStatus::Granted) {
            m_statusLabel->setText(tr("摄像头权限已允许，正在查找设备…"));
            refreshDevices();
        } else {
            m_statusLabel->setText(
                tr("未获得摄像头权限，请允许访问后重新启动软件。"));
            m_deviceCombo->setEnabled(false);
            m_formatCombo->setEnabled(false);
            m_startButton->setEnabled(false);
        }
    });
}

void MainWindow::buildUi()
{
    setWindowTitle(tr("USB 摄像头控制"));
    resize(1180, 720);

    auto *central = new QWidget(this);
    auto *root = new QHBoxLayout(central);
    root->setContentsMargins(12, 12, 12, 12);
    root->setSpacing(12);

    m_videoWidget = new QVideoWidget(central);
    m_videoWidget->setMinimumSize(720, 480);
    m_videoWidget->setStyleSheet("background: #111; border-radius: 6px;");
    connect(m_videoWidget->videoSink(), &QVideoSink::videoFrameChanged,
            this, [this](const QVideoFrame &frame) {
                if (!frame.isValid())
                    return;
                if (!m_receivedFrame) {
                    m_receivedFrame = true;
                    m_statusLabel->setText(tr("预览正常：%1 × %2")
                                               .arg(frame.width())
                                               .arg(frame.height()));
                }
            });
    root->addWidget(m_videoWidget, 1);

    auto *panel = new QFrame(central);
    panel->setMinimumWidth(320);
    panel->setMaximumWidth(390);
    auto *panelLayout = new QVBoxLayout(panel);

    auto *deviceGroup = new QGroupBox(tr("摄像头"), panel);
    auto *deviceLayout = new QVBoxLayout(deviceGroup);
    m_deviceCombo = new QComboBox(deviceGroup);
    m_formatCombo = new QComboBox(deviceGroup);
    m_startButton = new QPushButton(tr("启动预览"), deviceGroup);
    auto *refreshButton = new QPushButton(tr("刷新设备"), deviceGroup);
    deviceLayout->addWidget(new QLabel(tr("设备"), deviceGroup));
    deviceLayout->addWidget(m_deviceCombo);
    deviceLayout->addWidget(new QLabel(tr("分辨率与帧率"), deviceGroup));
    deviceLayout->addWidget(m_formatCombo);
    auto *buttonRow = new QHBoxLayout;
    buttonRow->addWidget(refreshButton);
    buttonRow->addWidget(m_startButton);
    deviceLayout->addLayout(buttonRow);
    panelLayout->addWidget(deviceGroup);

    auto *imageGroup = new QGroupBox(tr("画面参数"), panel);
    auto *imageLayout = new QVBoxLayout(imageGroup);

    auto addSlider = [&](const QString &name, QSlider *&slider, QLabel *&value) {
        auto *labelRow = new QHBoxLayout;
        labelRow->addWidget(new QLabel(name, imageGroup));
        value = new QLabel("--", imageGroup);
        labelRow->addStretch();
        labelRow->addWidget(value);
        slider = new QSlider(Qt::Horizontal, imageGroup);
        imageLayout->addLayout(labelRow);
        imageLayout->addWidget(slider);
    };

    addSlider(tr("曝光补偿"), m_exposureSlider, m_exposureValue);
    addSlider(tr("变焦"), m_zoomSlider, m_zoomValue);
    panelLayout->addWidget(imageGroup);

    auto *nativeGroup = new QGroupBox(tr("Windows UVC 参数"), panel);
    m_nativeControlsLayout = new QVBoxLayout(nativeGroup);
    m_nativeControlsLayout->addWidget(new QLabel(tr("选择摄像头后读取硬件参数"), nativeGroup));
    nativeGroup->setVisible(static_cast<bool>(m_nativeControls));
    panelLayout->addWidget(nativeGroup);
    panelLayout->addStretch();

    m_statusLabel = new QLabel(tr("正在查找摄像头…"), panel);
    m_statusLabel->setWordWrap(true);
    panelLayout->addWidget(m_statusLabel);
    root->addWidget(panel);
    setCentralWidget(central);

    connect(refreshButton, &QPushButton::clicked, this, &MainWindow::refreshDevices);
    connect(m_deviceCombo, &QComboBox::currentIndexChanged, this, &MainWindow::selectCamera);
    connect(m_formatCombo, &QComboBox::currentIndexChanged, this, &MainWindow::selectFormat);
    connect(m_startButton, &QPushButton::clicked, this, &MainWindow::toggleCamera);

    connect(m_exposureSlider, &QSlider::valueChanged, this, [this](int value) {
        if (!m_camera)
            return;
        const float exposure = static_cast<float>(value) / SliderScale;
        m_camera->setExposureCompensation(exposure);
        m_exposureValue->setText(QString::number(exposure, 'f', 1));
    });
    connect(m_zoomSlider, &QSlider::valueChanged, this, [this](int value) {
        if (!m_camera)
            return;
        const float zoom = static_cast<float>(value) / SliderScale;
        m_camera->zoomTo(zoom, 0.15f);
        m_zoomValue->setText(QString::number(zoom, 'f', 1) + QStringLiteral("×"));
    });
}

void MainWindow::refreshDevices()
{
    const auto devices = QMediaDevices::videoInputs();
    const QByteArray previousId = m_deviceCombo->currentData().toByteArray();
    m_deviceCombo->blockSignals(true);
    m_deviceCombo->clear();

    int selected = -1;
    for (int i = 0; i < devices.size(); ++i) {
        const auto &device = devices.at(i);
        m_deviceCombo->addItem(device.description(), device.id());
        if (device.id() == previousId || (previousId.isEmpty() && device.isDefault()))
            selected = i;
    }
    m_deviceCombo->blockSignals(false);

    const bool found = !devices.isEmpty();
    m_deviceCombo->setEnabled(found);
    m_startButton->setEnabled(found);
    if (!found) {
        m_camera.reset();
        m_formatCombo->clear();
        m_statusLabel->setText(tr("未检测到摄像头，请连接 USB 摄像头后刷新。"));
        syncControls();
        return;
    }

    m_deviceCombo->setCurrentIndex(selected >= 0 ? selected : 0);
    selectCamera(m_deviceCombo->currentIndex());
}

void MainWindow::selectCamera(int index)
{
    const auto devices = QMediaDevices::videoInputs();
    if (index < 0 || index >= devices.size())
        return;
    openCamera(devices.at(index));
}

void MainWindow::openCamera(const QCameraDevice &device)
{
    if (m_camera)
        m_camera->stop();

    m_camera = std::make_unique<QCamera>(device);
    m_receivedFrame = false;
    m_captureSession.setCamera(m_camera.get());
    m_captureSession.setVideoOutput(m_videoWidget);
    connect(m_camera.get(), &QCamera::activeChanged, this, &MainWindow::updateCameraState);
    connect(m_camera.get(), &QCamera::errorOccurred, this, &MainWindow::showCameraError);

    populateFormats(device);
    if (m_nativeControls) {
        m_nativeControls->open(device.description());
        rebuildNativeControls();
    }
    syncControls();
    m_statusLabel->setText(tr("已选择：%1").arg(device.description()));
    m_camera->start();
    QTimer::singleShot(3000, this, [this] {
        if (m_camera && m_camera->isActive() && !m_receivedFrame)
            m_statusLabel->setText(tr("摄像头已启动，但尚未收到画面。请使用“自动（推荐）”格式。"));
    });
}

void MainWindow::rebuildNativeControls()
{
    if (!m_nativeControlsLayout || !m_nativeControls)
        return;

    while (QLayoutItem *item = m_nativeControlsLayout->takeAt(0)) {
        delete item->widget();
        delete item;
    }

    const auto controls = m_nativeControls->controls();
    if (controls.isEmpty()) {
        auto *message = new QLabel(m_nativeControls->errorString(), this);
        message->setWordWrap(true);
        m_nativeControlsLayout->addWidget(message);
        return;
    }

    for (const auto &control : controls) {
        auto *container = new QWidget(this);
        auto *layout = new QVBoxLayout(container);
        layout->setContentsMargins(0, 2, 0, 2);

        auto *titleRow = new QHBoxLayout;
        auto *title = new QLabel(control.name, container);
        auto *valueLabel = new QLabel(QString::number(control.value), container);
        auto *autoBox = new QCheckBox(tr("自动"), container);
        autoBox->setVisible(control.autoSupported);
        autoBox->setChecked(control.automatic);
        titleRow->addWidget(title);
        titleRow->addStretch();
        titleRow->addWidget(valueLabel);
        titleRow->addWidget(autoBox);

        auto *slider = new QSlider(Qt::Horizontal, container);
        slider->setRange(static_cast<int>(control.minimum),
                         static_cast<int>(control.maximum));
        slider->setSingleStep(static_cast<int>(control.step));
        slider->setPageStep(static_cast<int>(control.step));
        slider->setValue(static_cast<int>(control.value));
        slider->setEnabled(!control.automatic);

        connect(slider, &QSlider::valueChanged, this,
                [this, id = control.id, valueLabel](int value) {
                    if (m_nativeControls->setValue(id, value))
                        valueLabel->setText(QString::number(value));
                });
        connect(autoBox, &QCheckBox::toggled, this,
                [this, id = control.id, slider](bool enabled) {
                    if (m_nativeControls->setAutomatic(id, enabled))
                        slider->setEnabled(!enabled);
                });

        layout->addLayout(titleRow);
        layout->addWidget(slider);
        m_nativeControlsLayout->addWidget(container);
    }
}

void MainWindow::populateFormats(const QCameraDevice &device)
{
    m_formatCombo->blockSignals(true);
    m_formatCombo->clear();
    m_formatCombo->addItem(tr("自动（推荐）"), QVariant::fromValue(QCameraFormat{}));
    auto formats = device.videoFormats();
    std::sort(formats.begin(), formats.end(), [](const QCameraFormat &a, const QCameraFormat &b) {
        const int aPixels = a.resolution().width() * a.resolution().height();
        const int bPixels = b.resolution().width() * b.resolution().height();
        return aPixels == bPixels ? a.maxFrameRate() > b.maxFrameRate() : aPixels > bPixels;
    });
    for (const auto &format : formats)
        m_formatCombo->addItem(formatLabel(format), QVariant::fromValue(format));
    m_formatCombo->blockSignals(false);

    m_camera->setCameraFormat(QCameraFormat{});
    m_formatCombo->setCurrentIndex(0);
}

QString MainWindow::formatLabel(const QCameraFormat &format) const
{
    return QStringLiteral("%1 × %2   %3 fps")
        .arg(format.resolution().width())
        .arg(format.resolution().height())
        .arg(format.maxFrameRate(), 0, 'f', 0);
}

void MainWindow::selectFormat(int index)
{
    if (!m_camera || index < 0)
        return;
    const auto format = m_formatCombo->itemData(index).value<QCameraFormat>();
    const bool wasActive = m_camera->isActive();
    if (wasActive)
        m_camera->stop();
    m_receivedFrame = false;
    m_camera->setCameraFormat(format);
    if (wasActive)
        m_camera->start();
    QTimer::singleShot(3000, this, [this] {
        if (m_camera && m_camera->isActive() && !m_receivedFrame)
            m_statusLabel->setText(tr("当前格式没有收到画面，请改回“自动（推荐）”。"));
    });
}

void MainWindow::toggleCamera()
{
    if (!m_camera)
        return;
    m_camera->isActive() ? m_camera->stop() : m_camera->start();
}

void MainWindow::updateCameraState()
{
    if (!m_camera)
        return;
    m_startButton->setText(m_camera->isActive() ? tr("停止预览") : tr("启动预览"));
    m_statusLabel->setText(m_camera->isActive() ? tr("摄像头正在运行") : tr("摄像头已停止"));
}

void MainWindow::showCameraError()
{
    if (!m_camera || m_camera->error() == QCamera::NoError)
        return;
    m_statusLabel->setText(tr("摄像头错误：%1").arg(m_camera->errorString()));
}

void MainWindow::syncControls()
{
    const bool available = static_cast<bool>(m_camera);
    m_exposureSlider->setEnabled(false);
    m_zoomSlider->setEnabled(false);
    m_exposureValue->setText("--");
    m_zoomValue->setText("--");
    if (!available)
        return;

    if (m_camera->supportedFeatures().testFlag(QCamera::Feature::ExposureCompensation)) {
        // Qt exposes whether EV compensation is supported, but not its native
        // range. Keep the UI within the range commonly accepted by backends.
        m_exposureSlider->setRange(-4 * SliderScale, 4 * SliderScale);
        m_exposureSlider->setValue(qRound(m_camera->exposureCompensation() * SliderScale));
        m_exposureSlider->setEnabled(true);
        m_exposureValue->setText(QString::number(m_camera->exposureCompensation(), 'f', 1));
    }

    const float minZoom = m_camera->minimumZoomFactor();
    const float maxZoom = m_camera->maximumZoomFactor();
    if (maxZoom > minZoom) {
        m_zoomSlider->setRange(qRound(minZoom * SliderScale), qRound(maxZoom * SliderScale));
        m_zoomSlider->setValue(qRound(m_camera->zoomFactor() * SliderScale));
        m_zoomSlider->setEnabled(true);
        m_zoomValue->setText(QString::number(m_camera->zoomFactor(), 'f', 1) + QStringLiteral("×"));
    }
}
