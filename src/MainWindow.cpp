#include "MainWindow.h"
#include "NativeCameraControls.h"
#include "VideoPreviewWidget.h"

#include <QApplication>
#include <QCamera>
#include <QCameraFormat>
#include <QCheckBox>
#include <QPermissions>
#include <QComboBox>
#include <QDateTime>
#include <QDir>
#include <QFrame>
#include <QGroupBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QMediaRecorder>
#include <QPushButton>
#include <QScrollArea>
#include <QSettings>
#include <QSlider>
#include <QStatusBar>
#include <QStandardPaths>
#include <QStyle>
#include <QTabWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <QVideoFrame>
#include <QVideoSink>
#include <QUrl>

#include <algorithm>

namespace {
constexpr int SliderScale = 100;

void clearLayout(QLayout *layout)
{
    while (QLayoutItem *item = layout->takeAt(0)) {
        if (QLayout *childLayout = item->layout())
            clearLayout(childLayout);
        delete item->widget();
        delete item;
    }
}
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    m_nativeControls = createNativeCameraControls();
    m_recorder = std::make_unique<QMediaRecorder>();
    m_captureSession.setRecorder(m_recorder.get());
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
    const QString buildId = QStringLiteral(APP_BUILD_ID).left(7);
    setWindowTitle(tr("USB 摄像头控制 v%1 (%2)")
                       .arg(QStringLiteral(APP_VERSION), buildId));
    resize(1180, 720);
    statusBar()->addPermanentWidget(
        new QLabel(tr("版本 v%1 · %2").arg(QStringLiteral(APP_VERSION), buildId), this));

    auto *central = new QWidget(this);
    central->setObjectName(QStringLiteral("appRoot"));
    auto *root = new QVBoxLayout(central);
    root->setContentsMargins(18, 14, 18, 16);
    root->setSpacing(14);

    auto *header = new QFrame(central);
    header->setObjectName(QStringLiteral("headerBar"));
    auto *headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(18, 12, 18, 12);
    auto *headingColumn = new QVBoxLayout;
    headingColumn->setSpacing(2);
    auto *appTitle = new QLabel(tr("USB Camera Control"), header);
    appTitle->setObjectName(QStringLiteral("appTitle"));
    auto *appSubtitle = new QLabel(tr("专业 USB 摄像头预览与参数控制"), header);
    appSubtitle->setObjectName(QStringLiteral("appSubtitle"));
    headingColumn->addWidget(appTitle);
    headingColumn->addWidget(appSubtitle);
    headerLayout->addLayout(headingColumn);
    headerLayout->addStretch();
    m_connectionBadge = new QLabel(tr("●  正在连接"), header);
    m_connectionBadge->setObjectName(QStringLiteral("connectionBadge"));
    m_connectionBadge->setProperty("state", QStringLiteral("waiting"));
    headerLayout->addWidget(m_connectionBadge);
    root->addWidget(header);

    auto *content = new QHBoxLayout;
    content->setSpacing(14);
    root->addLayout(content, 1);

    m_videoWidget = new VideoPreviewWidget(central);
    connect(m_videoWidget->videoSink(), &QVideoSink::videoFrameChanged,
            this, [this](const QVideoFrame &frame) {
                if (!frame.isValid())
                    return;
                if (!m_receivedFrame) {
                    m_receivedFrame = true;
                    setConnectionBadge(tr("●  预览正常"), QStringLiteral("ready"));
                    m_statusLabel->setText(tr("预览正常：%1 × %2")
                                               .arg(frame.width())
                                               .arg(frame.height()));
                }
            });
    content->addWidget(m_videoWidget, 1);

    auto *panel = new QFrame(central);
    panel->setObjectName(QStringLiteral("sidePanel"));
    panel->setMinimumWidth(320);
    panel->setMaximumWidth(390);
    auto *panelLayout = new QVBoxLayout(panel);
    panelLayout->setContentsMargins(0, 0, 0, 0);
    panelLayout->setSpacing(10);

    auto *deviceGroup = new QGroupBox(tr("摄像头"), panel);
    auto *deviceLayout = new QVBoxLayout(deviceGroup);
    m_deviceCombo = new QComboBox(deviceGroup);
    m_formatCombo = new QComboBox(deviceGroup);
    m_startButton = new QPushButton(tr("启动预览"), deviceGroup);
    m_startButton->setObjectName(QStringLiteral("primaryButton"));
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

    auto *tabs = new QTabWidget(panel);
    tabs->setObjectName(QStringLiteral("controlTabs"));

    auto *parameterPage = new QWidget(tabs);
    auto *parameterPageLayout = new QVBoxLayout(parameterPage);
    parameterPageLayout->setContentsMargins(0, 8, 0, 0);
    auto *parameterScroll = new QScrollArea(parameterPage);
    parameterScroll->setWidgetResizable(true);
    parameterScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    parameterScroll->setFrameShape(QFrame::NoFrame);
    auto *parameterContent = new QWidget(parameterScroll);
    parameterContent->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    auto *parameterLayout = new QVBoxLayout(parameterContent);
    parameterLayout->setContentsMargins(0, 0, 4, 0);
    parameterLayout->setSpacing(8);

    auto *capturePage = new QWidget(tabs);
    auto *capturePageLayout = new QVBoxLayout(capturePage);
    capturePageLayout->setContentsMargins(0, 8, 0, 0);

    auto *captureGroup = new QGroupBox(tr("拍摄与画面"), capturePage);
    auto *captureLayout = new QVBoxLayout(captureGroup);
    auto *captureButtons = new QHBoxLayout;
    m_snapshotButton = new QPushButton(tr("截图"), captureGroup);
    m_recordButton = new QPushButton(tr("开始录像"), captureGroup);
    m_recordButton->setObjectName(QStringLiteral("recordButton"));
    captureButtons->addWidget(m_snapshotButton);
    captureButtons->addWidget(m_recordButton);
    captureLayout->addLayout(captureButtons);

    m_mirrorCheck = new QCheckBox(tr("左右镜像"), captureGroup);
    m_verticalCheck = new QCheckBox(tr("上下翻转"), captureGroup);
    m_rotationCombo = new QComboBox(captureGroup);
    m_rotationCombo->addItem(tr("不旋转"), 0);
    m_rotationCombo->addItem(tr("顺时针 90°"), 90);
    m_rotationCombo->addItem(tr("旋转 180°"), 180);
    m_rotationCombo->addItem(tr("顺时针 270°"), 270);
    captureLayout->addWidget(m_mirrorCheck);
    captureLayout->addWidget(m_verticalCheck);
    captureLayout->addWidget(new QLabel(tr("画面旋转"), captureGroup));
    captureLayout->addWidget(m_rotationCombo);
    capturePageLayout->addWidget(captureGroup);
    capturePageLayout->addStretch();

    auto *imageGroup = new QGroupBox(tr("基础参数"), parameterContent);
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
    parameterLayout->addWidget(imageGroup);

    auto *nativeGroup = new QGroupBox(tr("摄像头参数"), parameterContent);
    m_nativeControlsLayout = new QVBoxLayout(nativeGroup);
    m_nativeControlsLayout->addWidget(new QLabel(tr("选择摄像头后读取硬件参数"), nativeGroup));
    nativeGroup->setVisible(static_cast<bool>(m_nativeControls));
    parameterLayout->addWidget(nativeGroup);
    parameterLayout->addStretch();
    parameterScroll->setWidget(parameterContent);
    parameterPageLayout->addWidget(parameterScroll);

    tabs->addTab(parameterPage, tr("参数设置"));
    tabs->addTab(capturePage, tr("拍摄与画面"));
    panelLayout->addWidget(tabs, 1);

    m_statusLabel = new QLabel(tr("正在查找摄像头…"), panel);
    m_statusLabel->setObjectName(QStringLiteral("statusCard"));
    m_statusLabel->setWordWrap(true);
    panelLayout->addWidget(m_statusLabel);
    content->addWidget(panel);
    setCentralWidget(central);

    connect(refreshButton, &QPushButton::clicked, this, &MainWindow::refreshDevices);
    connect(m_deviceCombo, &QComboBox::currentIndexChanged, this, &MainWindow::selectCamera);
    connect(m_formatCombo, &QComboBox::currentIndexChanged, this, &MainWindow::selectFormat);
    connect(m_startButton, &QPushButton::clicked, this, &MainWindow::toggleCamera);
    connect(m_snapshotButton, &QPushButton::clicked, this, &MainWindow::takeSnapshot);
    connect(m_recordButton, &QPushButton::clicked, this, &MainWindow::toggleRecording);
    connect(m_mirrorCheck, &QCheckBox::toggled, this, [this](bool enabled) {
        m_videoWidget->setMirrored(enabled);
        QSettings().setValue(QStringLiteral("preview/mirrored"), enabled);
    });
    connect(m_verticalCheck, &QCheckBox::toggled, this, [this](bool enabled) {
        m_videoWidget->setFlippedVertically(enabled);
        QSettings().setValue(QStringLiteral("preview/flippedVertically"), enabled);
    });
    connect(m_rotationCombo, &QComboBox::currentIndexChanged, this,
            [this](int index) {
                const int rotation = m_rotationCombo->itemData(index).toInt();
                m_videoWidget->setRotation(rotation);
                QSettings().setValue(QStringLiteral("preview/rotation"), rotation);
            });
    connect(m_recorder.get(), &QMediaRecorder::recorderStateChanged,
            this, &MainWindow::updateRecorderState);
    connect(m_recorder.get(), &QMediaRecorder::errorOccurred,
            this, &MainWindow::showRecorderError);
    connect(tabs, &QTabWidget::currentChanged, this, [](int index) {
        QSettings().setValue(QStringLiteral("ui/lastControlTab"), index);
    });

    QSettings settings;
    m_mirrorCheck->setChecked(settings.value(QStringLiteral("preview/mirrored"), false).toBool());
    m_verticalCheck->setChecked(
        settings.value(QStringLiteral("preview/flippedVertically"), false).toBool());
    const int savedRotation = settings.value(QStringLiteral("preview/rotation"), 0).toInt();
    const int rotationIndex = m_rotationCombo->findData(savedRotation);
    m_rotationCombo->setCurrentIndex(rotationIndex >= 0 ? rotationIndex : 0);
    tabs->setCurrentIndex(settings.value(QStringLiteral("ui/lastControlTab"), 0).toInt());

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
    QByteArray previousId = m_deviceCombo->currentData().toByteArray();
    if (previousId.isEmpty())
        previousId = QSettings().value(QStringLiteral("device/lastId")).toByteArray();
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
        setConnectionBadge(tr("●  未连接"), QStringLiteral("error"));
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
    QSettings().setValue(QStringLiteral("device/lastId"), devices.at(index).id());
    openCamera(devices.at(index));
}

void MainWindow::openCamera(const QCameraDevice &device)
{
    if (m_camera)
        m_camera->stop();

    m_camera = std::make_unique<QCamera>(device);
    m_currentDeviceName = device.description();
    setConnectionBadge(tr("●  正在启动"), QStringLiteral("waiting"));
    m_receivedFrame = false;
    m_captureSession.setCamera(m_camera.get());
    m_captureSession.setVideoSink(m_videoWidget->videoSink());
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

    clearLayout(m_nativeControlsLayout);

    const auto controls = m_nativeControls->controls();
    if (controls.isEmpty()) {
        auto *message = new QLabel(m_nativeControls->errorString(), this);
        message->setWordWrap(true);
        m_nativeControlsLayout->addWidget(message);
        return;
    }

    auto *buttonGrid = new QGridLayout;
    buttonGrid->setHorizontalSpacing(8);
    buttonGrid->setVerticalSpacing(8);
    auto *saveButton = new QPushButton(tr("保存参数"), this);
    auto *loadButton = new QPushButton(tr("应用参数"), this);
    auto *resetButton = new QPushButton(tr("恢复默认"), this);
    saveButton->setToolTip(tr("保存当前摄像头的参数"));
    loadButton->setToolTip(tr("应用之前保存的参数"));
    resetButton->setToolTip(tr("恢复摄像头的设备默认参数"));
    buttonGrid->addWidget(saveButton, 0, 0);
    buttonGrid->addWidget(loadButton, 0, 1);
    buttonGrid->addWidget(resetButton, 1, 0, 1, 2);
    buttonGrid->setColumnStretch(0, 1);
    buttonGrid->setColumnStretch(1, 1);
    m_nativeControlsLayout->addLayout(buttonGrid);
    connect(saveButton, &QPushButton::clicked, this, &MainWindow::saveNativePreset);
    connect(loadButton, &QPushButton::clicked, this, &MainWindow::loadNativePreset);
    connect(resetButton, &QPushButton::clicked, this, &MainWindow::resetNativeControls);

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

QString MainWindow::presetGroup() const
{
    const QByteArray encoded = m_currentDeviceName.toUtf8().toBase64(
        QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);
    return QStringLiteral("cameraPresets/%1").arg(QString::fromLatin1(encoded));
}

void MainWindow::saveNativePreset()
{
    if (!m_nativeControls || m_currentDeviceName.isEmpty())
        return;

    QSettings settings;
    settings.beginGroup(presetGroup());
    settings.remove(QString());
    settings.setValue(QStringLiteral("deviceName"), m_currentDeviceName);
    for (const auto &control : m_nativeControls->controls()) {
        const QString key = QString::number(static_cast<int>(control.id));
        settings.setValue(key + QStringLiteral("/value"),
                          static_cast<qlonglong>(control.value));
        settings.setValue(key + QStringLiteral("/automatic"), control.automatic);
    }
    settings.endGroup();
    settings.sync();
    m_statusLabel->setText(tr("已保存“%1”的当前设置").arg(m_currentDeviceName));
}

void MainWindow::loadNativePreset()
{
    if (!m_nativeControls || m_currentDeviceName.isEmpty())
        return;

    QSettings settings;
    settings.beginGroup(presetGroup());
    if (!settings.contains(QStringLiteral("deviceName"))) {
        settings.endGroup();
        m_statusLabel->setText(tr("当前摄像头还没有已保存的设置"));
        return;
    }

    int applied = 0;
    for (const auto &control : m_nativeControls->controls()) {
        const QString key = QString::number(static_cast<int>(control.id));
        const QString valueKey = key + QStringLiteral("/value");
        if (!settings.contains(valueKey))
            continue;
        const long value = settings.value(valueKey).toLongLong();
        const bool automatic = settings.value(key + QStringLiteral("/automatic"), false).toBool();
        m_nativeControls->setAutomatic(control.id, false);
        if (m_nativeControls->setValue(control.id, value))
            ++applied;
        if (control.autoSupported && automatic)
            m_nativeControls->setAutomatic(control.id, true);
    }
    settings.endGroup();
    rebuildNativeControls();
    m_statusLabel->setText(tr("已应用保存的设置，共更新 %1 项参数").arg(applied));
}

void MainWindow::resetNativeControls()
{
    if (!m_nativeControls)
        return;

    int applied = 0;
    for (const auto &control : m_nativeControls->controls()) {
        m_nativeControls->setAutomatic(control.id, false);
        if (m_nativeControls->setValue(control.id, control.defaultValue))
            ++applied;
    }
    rebuildNativeControls();
    m_statusLabel->setText(tr("已恢复设备默认值，共重置 %1 项参数").arg(applied));
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
    if (m_camera->isActive() && m_recorder->recorderState() == QMediaRecorder::RecordingState)
        m_recorder->stop();
    m_camera->isActive() ? m_camera->stop() : m_camera->start();
}

void MainWindow::updateCameraState()
{
    if (!m_camera)
        return;
    m_startButton->setText(m_camera->isActive() ? tr("停止预览") : tr("启动预览"));
    if (!m_camera->isActive())
        setConnectionBadge(tr("●  已停止"), QStringLiteral("waiting"));
    m_statusLabel->setText(m_camera->isActive() ? tr("摄像头正在运行") : tr("摄像头已停止"));
}

void MainWindow::showCameraError()
{
    if (!m_camera || m_camera->error() == QCamera::NoError)
        return;
    setConnectionBadge(tr("●  摄像头错误"), QStringLiteral("error"));
    m_statusLabel->setText(tr("摄像头错误：%1").arg(m_camera->errorString()));
}

void MainWindow::setConnectionBadge(const QString &text, const QString &state)
{
    if (!m_connectionBadge)
        return;
    m_connectionBadge->setText(text);
    m_connectionBadge->setProperty("state", state);
    m_connectionBadge->style()->unpolish(m_connectionBadge);
    m_connectionBadge->style()->polish(m_connectionBadge);
}

void MainWindow::takeSnapshot()
{
    const QImage image = m_videoWidget->currentImage();
    if (image.isNull()) {
        m_statusLabel->setText(tr("截图失败：当前还没有可用画面"));
        return;
    }

    const QString pictures = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    QDir directory(pictures + QStringLiteral("/USB Camera Control"));
    if (!directory.exists() && !directory.mkpath(QStringLiteral("."))) {
        m_statusLabel->setText(tr("截图失败：无法创建图片保存目录"));
        return;
    }

    const QString fileName = QStringLiteral("截图_%1.png")
        .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss_zzz")));
    const QString path = directory.filePath(fileName);
    if (image.save(path, "PNG"))
        m_statusLabel->setText(tr("截图已保存：%1").arg(QDir::toNativeSeparators(path)));
    else
        m_statusLabel->setText(tr("截图失败：无法写入文件"));
}

void MainWindow::toggleRecording()
{
    if (m_recorder->recorderState() == QMediaRecorder::RecordingState) {
        m_recorder->stop();
        return;
    }
    if (!m_camera) {
        m_statusLabel->setText(tr("录像失败：未选择摄像头"));
        return;
    }
    if (!m_camera->isActive())
        m_camera->start();

    const QString movies = QStandardPaths::writableLocation(QStandardPaths::MoviesLocation);
    QDir directory(movies + QStringLiteral("/USB Camera Control"));
    if (!directory.exists() && !directory.mkpath(QStringLiteral("."))) {
        m_statusLabel->setText(tr("录像失败：无法创建视频保存目录"));
        return;
    }

    const QString fileName = QStringLiteral("录像_%1.mp4")
        .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss")));
    m_recorder->setOutputLocation(QUrl::fromLocalFile(directory.filePath(fileName)));
    m_recorder->setQuality(QMediaRecorder::HighQuality);
    m_recorder->record();
}

void MainWindow::updateRecorderState()
{
    const bool recording = m_recorder->recorderState() == QMediaRecorder::RecordingState;
    m_recordButton->setText(recording ? tr("停止录像") : tr("开始录像"));
    m_recordButton->setProperty("recording", recording);
    m_recordButton->style()->unpolish(m_recordButton);
    m_recordButton->style()->polish(m_recordButton);
    if (recording) {
        m_statusLabel->setText(tr("正在录像…"));
    } else if (!m_recorder->actualLocation().isEmpty()) {
        m_statusLabel->setText(tr("录像已保存：%1")
                                   .arg(QDir::toNativeSeparators(
                                       m_recorder->actualLocation().toLocalFile())));
    }
}

void MainWindow::showRecorderError()
{
    if (m_recorder->error() == QMediaRecorder::NoError)
        return;
    m_statusLabel->setText(tr("录像错误：%1").arg(m_recorder->errorString()));
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
