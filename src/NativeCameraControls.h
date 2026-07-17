#pragma once

#include <QList>
#include <QString>

#include <memory>

class NativeCameraControls
{
public:
    enum class Id {
        Brightness,
        Contrast,
        Hue,
        Saturation,
        Sharpness,
        Gamma,
        WhiteBalance,
        BacklightCompensation,
        Gain,
        Zoom,
        Exposure,
        Focus,
    };

    struct Control {
        Id id;
        QString name;
        long minimum = 0;
        long maximum = 0;
        long step = 1;
        long defaultValue = 0;
        long value = 0;
        bool autoSupported = false;
        bool automatic = false;
        bool defaultAutomatic = false;
    };

    virtual ~NativeCameraControls() = default;
    virtual bool open(const QString &deviceName) = 0;
    virtual QList<Control> controls() const = 0;
    virtual bool setValue(Id id, long value) = 0;
    virtual bool setAutomatic(Id id, bool enabled) = 0;
    virtual QString errorString() const = 0;
};

std::unique_ptr<NativeCameraControls> createNativeCameraControls();
