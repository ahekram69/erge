#include "NativeCameraControls.h"
#include "Mp4Writer.h"
#include <QtWidgets>
#include <QAbstractVideoSurface>
#include <QCamera>
#include <QCameraInfo>
#include <QCameraViewfinderSettings>
#include <QVideoSurfaceFormat>
#include <QMutex>
#include <QMutexLocker>
#include <windows.h>
#include <objbase.h>
#include <memory>

static bool english = false;
static QString text(const char *zh, const char *en) { return QString::fromUtf8(english ? en : zh); }

class FrameSurface : public QAbstractVideoSurface {
    QMutex mutex;
    QImage latest;
public:
    using QAbstractVideoSurface::QAbstractVideoSurface;
    QList<QVideoFrame::PixelFormat> supportedPixelFormats(QAbstractVideoBuffer::HandleType handle) const override {
        if (handle != QAbstractVideoBuffer::NoHandle) return {};
        return {QVideoFrame::Format_RGB32, QVideoFrame::Format_ARGB32, QVideoFrame::Format_RGB24};
    }
    bool present(const QVideoFrame &incoming) override {
        QVideoFrame frame(incoming);
        if (!frame.map(QAbstractVideoBuffer::ReadOnly)) return false;
        const auto format = QVideoFrame::imageFormatFromPixelFormat(frame.pixelFormat());
        QImage image;
        if (format != QImage::Format_Invalid)
            image = QImage(frame.bits(), frame.width(), frame.height(), frame.bytesPerLine(), format).copy();
        frame.unmap();
        if (image.isNull()) return false;
        if (surfaceFormat().scanLineDirection() == QVideoSurfaceFormat::BottomToTop) image=image.mirrored(false,true);
        QMutexLocker lock(&mutex);
        latest=image; // Bounded latest-frame mailbox: never queue unbounded 4K images.
        return true;
    }
    QImage take() { QMutexLocker lock(&mutex); QImage result; result.swap(latest); return result; }
};

class Preview : public QWidget {
public:
    QImage image;
    using QWidget::QWidget;
    void paintEvent(QPaintEvent *) override {
        QPainter p(this); p.fillRect(rect(), QColor("#e9ebef"));
        if (!image.isNull()) {
            const QSize fit=image.size().scaled(size(), Qt::KeepAspectRatio);
            p.drawImage(QRect(QPoint((width()-fit.width())/2,(height()-fit.height())/2),fit), image);
        }
    }
};

class LegacyWindow : public QMainWindow {
    std::unique_ptr<NativeCameraControls> controls=createNativeCameraControls();
    std::unique_ptr<QCamera> camera;
    FrameSurface surface;
    Mp4Writer recorder;
    QElapsedTimer recordClock, frameClock;
    QTimer frameTimer, deviceTimer;
    QList<QCameraInfo> devices;
    QList<QCameraViewfinderSettings> formats;
    QString currentId, videoPath;
    QStringList knownIds;
    bool requested=true;
    int retries=0;
    Preview *preview;
    QComboBox *deviceBox, *formatBox, *rotation;
    QPushButton *startButton, *recordButton;
    QCheckBox *mirror, *flip;
    QWidget *side, *actions;
    QVBoxLayout *colorLayout, *lensLayout;
    QLabel *status, *recordStatus;
    QImage frame;
    QString presetKey() const { return "presets/"+QString::fromLatin1(currentId.toUtf8().toHex()); }
    QString mediaPath(QStandardPaths::StandardLocation where, const QString &suffix) {
        QDir dir(QStandardPaths::writableLocation(where)+"/USB Camera Control Legacy");
        if (!dir.mkpath(".")) { status->setText(text("无法创建保存目录","Cannot create output folder")); return {}; }
        return dir.filePath(QDateTime::currentDateTime().toString("yyyyMMdd-HHmmss-zzz")+suffix);
    }
    QImage transformed() const {
        QTransform transform;
        transform.scale(mirror->isChecked() ? -1 : 1, flip->isChecked() ? -1 : 1);
        transform.rotate(rotation->currentData().toInt());
        return frame.transformed(transform);
    }
    static void clear(QLayout *layout) {
        while (auto *item=layout->takeAt(0)) { delete item->widget(); delete item; }
    }
    void refreshControls() {
        clear(colorLayout); clear(lensLayout);
        const auto list=controls->controls();
        actions->setEnabled(camera && !list.empty());
        for (const auto &control:list) {
            auto *box=new QWidget;
            auto *layout=new QVBoxLayout(box);
            auto *row=new QHBoxLayout;
            const char *names[]={"Brightness","Contrast","Hue","Saturation","Sharpness","Gamma","White balance","Backlight compensation","Gain","Zoom","Exposure","Focus"};
            row->addWidget(new QLabel(english ? QString::fromLatin1(names[int(control.id)]) : control.name));
            row->addStretch();
            auto *value=new QSpinBox;
            value->setRange(int(control.minimum),int(control.maximum));
            value->setSingleStep(qMax(1,int(control.step))); value->setValue(int(control.value));
            value->setMaximumWidth(90);
            auto *automatic=new QCheckBox(text("自动","Auto"));
            automatic->setVisible(control.autoSupported); automatic->setChecked(control.automatic);
            row->addWidget(value); row->addWidget(automatic); layout->addLayout(row);
            auto *slider=new QSlider(Qt::Horizontal);
            slider->setRange(value->minimum(),value->maximum());slider->setSingleStep(value->singleStep());slider->setValue(value->value());
            slider->setEnabled(!control.automatic); value->setEnabled(!control.automatic);
            layout->addWidget(slider);
            auto *hint=new QLabel(text("自动调节中，关闭自动后可手动修改","Adjusting automatically; turn off Auto to edit"));
            hint->setWordWrap(true);hint->setVisible(control.automatic);layout->addWidget(hint);
            const auto update=[this,id=control.id,slider,value](int n) {
                const auto before=controls->controls();
                if (!controls->setValue(id,n)) {
                    status->setText(text("摄像头未接受参数","Camera rejected this value"));
                    for (const auto &c:before) if(c.id==id) n=int(c.value);
                }
                QSignalBlocker a(slider),b(value);slider->setValue(n);value->setValue(n);
            };
            connect(slider,&QSlider::valueChanged,this,update);
            connect(value,QOverload<int>::of(&QSpinBox::valueChanged),this,update);
            connect(automatic,&QCheckBox::toggled,this,[this,id=control.id,automatic,slider,value,hint](bool enabled) {
                if (!controls->setAutomatic(id,enabled)) { QSignalBlocker b(automatic);automatic->setChecked(!enabled);status->setText(text("自动模式切换失败","Auto mode change failed"));return; }
                slider->setEnabled(!enabled);value->setEnabled(!enabled);hint->setVisible(enabled);
            });
            const bool lens=control.id==NativeCameraControls::Id::Focus || control.id==NativeCameraControls::Id::Zoom;
            (lens?lensLayout:colorLayout)->addWidget(box);
        }
        colorLayout->addStretch();lensLayout->addStretch();
    }
    void stopRecording() {
        if (!recorder.active()) return;
        const bool ok=recorder.finish();
        status->setText(ok ? text("录像已保存：","Video saved: ")+videoPath : recorder.error());
        recordButton->setText(text("开始录像","Record"));recordStatus->setText(text("未在录像","Not recording"));
        deviceBox->setEnabled(true);formatBox->setEnabled(true);mirror->setEnabled(true);flip->setEnabled(true);rotation->setEnabled(true);
    }
    void closeCamera() {
        stopRecording();
        if(camera) {camera->stop();camera.reset();}
        surface.take();frame={};preview->image={};preview->update();
        actions->setEnabled(false);
    }
    void openDevice() {
        closeCamera();
        const int i=deviceBox->currentIndex(); if(i<0 || i>=devices.size()) return;
        currentId=devices[i].deviceName();QSettings().setValue("device",currentId);
        camera.reset(new QCamera(devices[i])); camera->setViewfinder(&surface);
        connect(camera.get(),QOverload<QCamera::Error>::of(&QCamera::error),this,[this](QCamera::Error){ if(camera) status->setText(camera->errorString()); });
        connect(camera.get(),&QCamera::statusChanged,this,[this](QCamera::Status s){
            if(s==QCamera::LoadedStatus && formats.isEmpty()) {
                formats=camera->supportedViewfinderSettings();
                QSignalBlocker blocker(formatBox); formatBox->clear();formatBox->addItem(text("自动（推荐）","Auto (recommended)"),-1);
                for(int i=0;i<formats.size();++i) {const auto f=formats[i];formatBox->addItem(QString("%1 × %2 · %3 fps").arg(f.resolution().width()).arg(f.resolution().height()).arg(f.maximumFrameRate()),i);}
            }
        });
        formats.clear();controls->open(devices[i].description());refreshControls();
        camera->load(); if(requested) camera->start();
        frameClock.restart(); startButton->setText(requested?text("停止预览","Stop preview"):text("启动预览","Start preview"));
        status->setText(text("正在连接摄像头…","Connecting camera…"));
    }
    void refreshDevices() {
        const auto found=QCameraInfo::availableCameras();QStringList ids;
        for(const auto &d:found) ids<<d.deviceName();
        if(ids==knownIds) return;
        knownIds=ids;devices=found;
        const QString preferred=currentId.isEmpty()?QSettings().value("device").toString():currentId;
        {QSignalBlocker blocker(deviceBox);deviceBox->clear();for(const auto &d:devices)deviceBox->addItem(d.description());deviceBox->setCurrentIndex(qMax(0,ids.indexOf(preferred)));}
        if(devices.empty()) {closeCamera();currentId=preferred;clear(colorLayout);clear(lensLayout);status->setText(text("未连接摄像头，等待重新连接","No camera; waiting for reconnection"));return;}
        retries=0;openDevice();
    }
    void savePreset() {
        QSettings settings;settings.beginGroup(presetKey());settings.setValue("exists",true);
        for(const auto &c:controls->controls()){QString key=QString::number(int(c.id));settings.setValue(key+"/value",qlonglong(c.value));settings.setValue(key+"/auto",c.automatic);}
        settings.sync();status->setText(settings.status()==QSettings::NoError?text("参数已保存","Settings saved"):text("保存失败","Save failed"));
    }
    void applyPreset(bool initial) {
        QSettings settings;settings.beginGroup(presetKey());
        if(!initial&&!settings.value("exists").toBool()){status->setText(text("还没有保存的参数","No saved settings"));return;}
        QStringList failures;
        for(const auto &c:controls->controls()) {
            const QString key=QString::number(int(c.id));
            if(!initial&&!settings.contains(key+"/value"))continue;
            const long value=initial?c.defaultValue:settings.value(key+"/value").toInt();
            const bool automatic=initial?c.defaultAutomatic:settings.value(key+"/auto").toBool();
            bool ok=controls->setValue(c.id,value);
            if(c.autoSupported)ok=controls->setAutomatic(c.id,automatic)&&ok;
            if(!ok)failures<<c.name;
        }
        refreshControls();status->setText(failures.empty()?text("参数已应用","Settings applied"):text("部分参数失败：","Some controls failed: ")+failures.join(", "));
    }
public:
    LegacyWindow() {
        setWindowTitle(QString("USB Camera Control v%1 · Legacy x86 · TEST").arg(APP_VERSION));resize(1180,760);
        auto *central=new QWidget;auto *root=new QHBoxLayout(central);setCentralWidget(central);
        preview=new Preview;preview->setMinimumSize(480,320);
        auto *left=new QVBoxLayout;auto *tools=new QHBoxLayout;
        auto *full=new QPushButton(text("全屏预览","Full screen"));auto *hide=new QPushButton(text("收起面板","Hide controls"));
        tools->addStretch();tools->addWidget(full);tools->addWidget(hide);left->addLayout(tools);left->addWidget(preview,1);root->addLayout(left,1);
        side=new QWidget;side->setFixedWidth(370);root->addWidget(side);auto *panel=new QVBoxLayout(side);
        auto *language=new QComboBox;language->addItems({text("跟随系统","System"),QStringLiteral("简体中文"),QStringLiteral("English")});
        language->setCurrentIndex(QSettings().value("language",0).toInt());panel->addWidget(language);
        connect(language,QOverload<int>::of(&QComboBox::activated),this,[this](int i){QSettings().setValue("language",i);QMessageBox::information(this,"Language",text("重启后生效","Restart to apply"));});
        deviceBox=new QComboBox;formatBox=new QComboBox;panel->addWidget(deviceBox);panel->addWidget(formatBox);
        startButton=new QPushButton(text("启动预览","Start preview"));auto *refresh=new QPushButton(text("刷新设备","Refresh"));auto *row=new QHBoxLayout;row->addWidget(refresh);row->addWidget(startButton);panel->addLayout(row);
        auto *tabs=new QTabWidget;panel->addWidget(tabs,1);
        auto *parameters=new QWidget;auto *pl=new QVBoxLayout(parameters);auto *subtabs=new QTabWidget;pl->addWidget(subtabs,1);
        auto addPage=[subtabs](QString name){auto *scroll=new QScrollArea;scroll->setWidgetResizable(true);auto *body=new QWidget;auto *layout=new QVBoxLayout(body);scroll->setWidget(body);subtabs->addTab(scroll,name);return layout;};
        colorLayout=addPage(text("曝光与色彩","Exposure / color"));lensLayout=addPage(text("对焦与变焦","Focus / zoom"));
        actions=new QWidget;auto *al=new QVBoxLayout(actions);auto *save=new QPushButton(text("保存参数","Save settings"));auto *load=new QPushButton(text("应用参数","Apply settings"));auto *reset=new QPushButton(text("恢复初始设置","Restore initial settings"));
        auto *ar=new QHBoxLayout;ar->addWidget(save);ar->addWidget(load);al->addLayout(ar);al->addWidget(reset);pl->addWidget(actions);actions->setEnabled(false);
        connect(save,&QPushButton::clicked,this,[this]{savePreset();});connect(load,&QPushButton::clicked,this,[this]{applyPreset(false);});connect(reset,&QPushButton::clicked,this,[this]{applyPreset(true);});
        tabs->addTab(parameters,text("参数设置","Parameters"));
        auto *capture=new QWidget;auto *cl=new QVBoxLayout(capture);auto *snapshot=new QPushButton(text("截图","Snapshot"));recordButton=new QPushButton(text("开始录像","Record"));recordStatus=new QLabel(text("未在录像","Not recording"));
        cl->addWidget(snapshot);cl->addWidget(recordButton);cl->addWidget(recordStatus);
        mirror=new QCheckBox(text("左右镜像","Mirror"));flip=new QCheckBox(text("上下翻转","Flip vertically"));rotation=new QComboBox;
        for(int degree:{0,90,180,270})rotation->addItem(QString::number(degree)+QChar(0x00b0),degree);
        mirror->setChecked(QSettings().value("mirror").toBool());flip->setChecked(QSettings().value("flip").toBool());rotation->setCurrentIndex(QSettings().value("rotation",0).toInt());
        cl->addWidget(mirror);cl->addWidget(flip);cl->addWidget(rotation);
        connect(mirror,&QCheckBox::toggled,this,[](bool b){QSettings().setValue("mirror",b);});connect(flip,&QCheckBox::toggled,this,[](bool b){QSettings().setValue("flip",b);});connect(rotation,QOverload<int>::of(&QComboBox::currentIndexChanged),this,[](int i){QSettings().setValue("rotation",i);});
        for(auto where:{QStandardPaths::PicturesLocation,QStandardPaths::MoviesLocation}) {
            auto *folder=new QPushButton(where==QStandardPaths::PicturesLocation?text("图片文件夹","Pictures folder"):text("视频文件夹","Videos folder"));cl->addWidget(folder);
            connect(folder,&QPushButton::clicked,this,[this,where]{const QString file=mediaPath(where,"");if(!file.isEmpty())QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(file).absolutePath()));});
        }
        auto *diagnostics=new QPushButton(text("导出诊断信息","Export diagnostics"));cl->addWidget(diagnostics);cl->addStretch();tabs->addTab(capture,text("拍摄与画面","Capture / image"));
        status=new QLabel(text("未连接摄像头，等待重新连接","No camera; waiting for reconnection"));status->setWordWrap(true);panel->addWidget(status);
        connect(diagnostics,&QPushButton::clicked,this,[this]{const QString path=QFileDialog::getSaveFileName(this,text("导出诊断信息","Export diagnostics"),"camera-diagnostics.txt");if(path.isEmpty())return;QSaveFile file(path);if(!file.open(QIODevice::WriteOnly))return;QTextStream out(&file);out<<"Legacy x86 "<<APP_VERSION<<"\n"<<QSysInfo::prettyProductName()<<"\n"<<deviceBox->currentText()<<"\n"<<frame.size().width()<<"x"<<frame.size().height()<<"\n"<<status->text()<<"\n";out.flush();status->setText(file.commit()?text("已导出","Exported"):text("导出失败","Export failed"));});
        connect(snapshot,&QPushButton::clicked,this,[this]{if(frame.isNull()){status->setText(text("没有可用画面","No frame available"));return;}const QString path=mediaPath(QStandardPaths::PicturesLocation,".png");if(!path.isEmpty())status->setText(transformed().save(path)?text("截图已保存：","Snapshot saved: ")+path:text("截图失败","Snapshot failed"));});
        connect(recordButton,&QPushButton::clicked,this,[this]{
            if(recorder.active()){stopRecording();return;}
            if(frame.isNull()){status->setText(text("没有可用画面","No frame available"));return;}
            videoPath=mediaPath(QStandardPaths::MoviesLocation,".mp4");if(videoPath.isEmpty())return;
            if(!recorder.start(videoPath,transformed().size())){status->setText(text("录像启动失败；可尝试较低分辨率。","Recording failed; try a lower resolution. ")+recorder.error());return;}
            recordClock.start();recordButton->setText(text("停止录像","Stop recording"));deviceBox->setEnabled(false);formatBox->setEnabled(false);mirror->setEnabled(false);flip->setEnabled(false);rotation->setEnabled(false);
        });
        connect(startButton,&QPushButton::clicked,this,[this]{requested=!requested;if(!requested)closeCamera();else{retries=0;openDevice();}startButton->setText(requested?text("停止预览","Stop preview"):text("启动预览","Start preview"));});
        connect(refresh,&QPushButton::clicked,this,[this]{if(recorder.active())return;knownIds.clear();refreshDevices();});
        connect(deviceBox,QOverload<int>::of(&QComboBox::activated),this,[this]{retries=0;openDevice();});
        connect(formatBox,QOverload<int>::of(&QComboBox::activated),this,[this]{if(!camera)return;camera->stop();const int i=formatBox->currentData().toInt();camera->setViewfinderSettings(i>=0&&i<formats.size()?formats[i]:QCameraViewfinderSettings());surface.take();frame={};frameClock.restart();if(requested)camera->start();});
        connect(hide,&QPushButton::clicked,this,[this,hide]{side->setVisible(side->isHidden());hide->setText(side->isHidden()?text("展开面板","Show controls"):text("收起面板","Hide controls"));});
        auto toggle=[this,full,hide]{if(isFullScreen()){showNormal();side->setVisible(side->property("beforeFullScreen").toBool());hide->show();full->setText(text("全屏预览","Full screen"));}else{side->setProperty("beforeFullScreen",!side->isHidden());side->hide();hide->hide();showFullScreen();full->setText(text("退出全屏","Exit full screen"));}};
        connect(full,&QPushButton::clicked,this,toggle);connect(new QShortcut(QKeySequence(Qt::Key_Escape),this),&QShortcut::activated,this,[this,toggle]{if(isFullScreen())toggle();});
        connect(&frameTimer,&QTimer::timeout,this,[this]{
            QImage next=surface.take();
            if(!next.isNull()) {frame=next;frameClock.restart();retries=0;preview->image=transformed();preview->update();
                if(recorder.active()){if(!recorder.write(preview->image,recordClock.nsecsElapsed()/100)){const QString error=recorder.error();stopRecording();status->setText(error);}else recordStatus->setText(text("● 正在录像 ","● Recording ")+QString::number(recordClock.elapsed()/1000)+" s");}
                else status->setText(text("预览正常：","Preview: ")+QString("%1 × %2").arg(frame.width()).arg(frame.height()));
            } else if(camera&&requested&&frameClock.isValid()&&frameClock.elapsed()>7000) {
                stopRecording();if(retries<2){++retries;openDevice();}else status->setText(text("未收到画面，请重新插拔或刷新","No frames. Reconnect the camera or refresh."));
            }
        });frameTimer.start(33);
        connect(&deviceTimer,&QTimer::timeout,this,[this]{refreshDevices();});deviceTimer.start(1000);refreshDevices();
    }
    ~LegacyWindow() override {frameTimer.stop();deviceTimer.stop();closeCamera();}
};

int main(int argc,char **argv) {
    QApplication app(argc,argv);app.setOrganizationName("CameraTools");app.setApplicationName("USB Camera Control Legacy");
    app.setStyle("Fusion");app.setWindowIcon(QIcon(":/icons/app-icon.png"));
    const int language=QSettings().value("language",0).toInt();english=language==2||(language==0&&QLocale::system().language()!=QLocale::Chinese);
    if(app.arguments().contains("--self-test")) {
        const HRESULT hr=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
        int result=0;
        { Mp4Writer writer;QImage image(640,480,QImage::Format_RGB32);image.fill(Qt::red);
          {QPainter painter(&image);painter.fillRect(0,240,640,240,Qt::blue);}
          if(!writer.start("legacy-encoder-test.mp4",image.size())) result=2;
          for(int i=0;i<60&&!result;++i)if(!writer.write(image,qint64(i)*10000000/30))result=3;
          if(!writer.finish())result=4;
          if(result)qCritical()<<writer.error(); }
        if(SUCCEEDED(hr))CoUninitialize();return result;
    }
    app.setStyleSheet("QWidget {font-size:13px;color:#1d1d1f;} QMainWindow {background:#f5f5f7;} QPushButton,QComboBox,QSpinBox {min-height:30px;} QTabWidget::pane {border:1px solid #dedee3;} QSlider::groove:horizontal {height:4px;background:#d9d9df;} QSlider::handle:horizontal {background:#007aff;width:14px;margin:-5px 0;border-radius:7px;}");
    LegacyWindow window;window.show();return app.exec();
}
