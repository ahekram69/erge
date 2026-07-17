#include "NativeCameraControls.h"

#include <CoreMediaIO/CMIOHardware.h>

#include <QHash>

#include <algorithm>
#include <cmath>
#include <vector>

namespace {

constexpr long UiMinimum = 0;
constexpr long UiMaximum = 1000;

struct Mapping {
    NativeCameraControls::Id id;
    const char *name;
    CMIOClassID classId;
};

constexpr Mapping mappings[] = {
    { NativeCameraControls::Id::Brightness, "亮度", kCMIOBrightnessControlClassID },
    { NativeCameraControls::Id::Contrast, "对比度", kCMIOContrastControlClassID },
    { NativeCameraControls::Id::Hue, "色相", kCMIOHueControlClassID },
    { NativeCameraControls::Id::Saturation, "饱和度", kCMIOSaturationControlClassID },
    { NativeCameraControls::Id::Sharpness, "锐度", kCMIOSharpnessControlClassID },
    { NativeCameraControls::Id::Gamma, "Gamma", kCMIOGammaControlClassID },
    { NativeCameraControls::Id::WhiteBalance, "白平衡", kCMIOWhiteBalanceControlClassID },
    { NativeCameraControls::Id::BacklightCompensation, "背光补偿", kCMIOBacklightCompensationControlClassID },
    { NativeCameraControls::Id::Gain, "增益", kCMIOGainControlClassID },
    { NativeCameraControls::Id::Zoom, "变焦", kCMIOZoomControlClassID },
    { NativeCameraControls::Id::Exposure, "曝光", kCMIOExposureControlClassID },
    { NativeCameraControls::Id::Exposure, "曝光", kCMIOShutterControlClassID },
    { NativeCameraControls::Id::Focus, "对焦", kCMIOFocusControlClassID },
};

CMIOObjectPropertyAddress address(CMIOObjectPropertySelector selector)
{
    return { selector, kCMIOObjectPropertyScopeGlobal, kCMIOObjectPropertyElementMain };
}

template<typename T>
bool getScalar(CMIOObjectID object, CMIOObjectPropertySelector selector, T &value)
{
    const auto property = address(selector);
    UInt32 size = sizeof(T);
    UInt32 used = 0;
    return CMIOObjectGetPropertyData(object, &property, 0, nullptr, size, &used, &value) == noErr
        && used == sizeof(T)
        && size == sizeof(T);
}

template<typename T>
bool setScalar(CMIOObjectID object, CMIOObjectPropertySelector selector, const T &value)
{
    const auto property = address(selector);
    Boolean settable = false;
    if (CMIOObjectIsPropertySettable(object, &property, &settable) != noErr || !settable)
        return false;
    return CMIOObjectSetPropertyData(object, &property, 0, nullptr, sizeof(T), &value) == noErr;
}

std::vector<CMIOObjectID> objectList(CMIOObjectID object,
                                     CMIOObjectPropertySelector selector,
                                     const CMIOClassID *qualifier = nullptr)
{
    const auto property = address(selector);
    const UInt32 qualifierSize = qualifier ? sizeof(*qualifier) : 0;
    UInt32 size = 0;
    if (CMIOObjectGetPropertyDataSize(object, &property, qualifierSize, qualifier, &size) != noErr
        || size == 0 || size % sizeof(CMIOObjectID) != 0) {
        return {};
    }
    std::vector<CMIOObjectID> result(size / sizeof(CMIOObjectID));
    UInt32 used = 0;
    if (CMIOObjectGetPropertyData(object, &property, qualifierSize, qualifier,
                                  size, &used, result.data()) != noErr) {
        return {};
    }
    result.resize(used / sizeof(CMIOObjectID));
    return result;
}

QString cfStringProperty(CMIOObjectID object, CMIOObjectPropertySelector selector)
{
    CFStringRef string = nullptr;
    if (!getScalar(object, selector, string) || !string)
        return {};
    const CFIndex length = CFStringGetLength(string);
    const CFIndex capacity = CFStringGetMaximumSizeForEncoding(length, kCFStringEncodingUTF8) + 1;
    QByteArray utf8(static_cast<qsizetype>(capacity), Qt::Uninitialized);
    QString result;
    if (CFStringGetCString(string, utf8.data(), capacity, kCFStringEncodingUTF8))
        result = QString::fromUtf8(utf8.constData());
    CFRelease(string);
    return result;
}

class MacCameraControls final : public NativeCameraControls
{
public:
    bool open(const QString &deviceName) override
    {
        m_error.clear();
        m_controls.clear();
        m_bindings.clear();

        const auto devices = objectList(kCMIOObjectSystemObject, kCMIOHardwarePropertyDevices);
        CMIODeviceID selected = kCMIOObjectUnknown;
        for (CMIOObjectID device : devices) {
            const QString name = cfStringProperty(device, kCMIOObjectPropertyName);
            if (name.compare(deviceName, Qt::CaseInsensitive) == 0) {
                selected = device;
                break;
            }
        }
        if (selected == kCMIOObjectUnknown)
            return fail(QStringLiteral("CoreMediaIO 未找到与“%1”匹配的摄像头").arg(deviceName));

        const CMIOClassID featureClass = kCMIOFeatureControlClassID;
        const auto controls = objectList(selected, kCMIOObjectPropertyOwnedObjects, &featureClass);
        for (const Mapping &mapping : mappings) {
            if (m_bindings.contains(mapping.id))
                continue;
            const auto found = std::find_if(controls.cbegin(), controls.cend(),
                                            [&mapping](CMIOObjectID control) {
                CMIOClassID classId = 0;
                return getScalar(control, kCMIOObjectPropertyClass, classId)
                    && classId == mapping.classId;
            });
            if (found != controls.cend())
                addControl(mapping, *found);
        }

        if (m_controls.isEmpty())
            return fail(QStringLiteral("摄像头驱动没有通过 macOS CoreMediaIO 公布标准可调参数"));
        return true;
    }

    QList<Control> controls() const override { return m_controls; }

    bool setValue(Id id, long value) override
    {
        auto binding = m_bindings.find(id);
        if (binding == m_bindings.end())
            return fail(QStringLiteral("找不到该摄像头参数"));

        if (binding->autoSupported) {
            const UInt32 manual = 0;
            if (!setScalar(binding->object, kCMIOFeatureControlPropertyAutomaticManual, manual))
                return fail(QStringLiteral("摄像头拒绝切换到手动模式"));
        }

        const double position = std::clamp(
            (static_cast<double>(value) - UiMinimum) / (UiMaximum - UiMinimum), 0.0, 1.0);
        const Float32 nativeValue = static_cast<Float32>(
            binding->minimum + position * (binding->maximum - binding->minimum));
        if (!setScalar(binding->object, kCMIOFeatureControlPropertyNativeValue, nativeValue))
            return fail(QStringLiteral("摄像头拒绝该参数值"));

        if (Control *control = findControl(id)) {
            control->value = std::clamp(value, UiMinimum, UiMaximum);
            control->automatic = false;
        }
        m_error.clear();
        return true;
    }

    bool setAutomatic(Id id, bool enabled) override
    {
        auto binding = m_bindings.find(id);
        if (binding == m_bindings.end() || !binding->autoSupported)
            return fail(QStringLiteral("摄像头不支持该参数的自动模式"));
        const UInt32 automatic = enabled ? 1 : 0;
        if (!setScalar(binding->object, kCMIOFeatureControlPropertyAutomaticManual, automatic))
            return fail(QStringLiteral("摄像头拒绝切换自动模式"));
        if (Control *control = findControl(id))
            control->automatic = enabled;
        m_error.clear();
        return true;
    }

    QString errorString() const override { return m_error; }

private:
    struct Binding {
        CMIOObjectID object = kCMIOObjectUnknown;
        double minimum = 0;
        double maximum = 0;
        bool autoSupported = false;
    };

    bool fail(const QString &message)
    {
        m_error = message;
        return false;
    }

    Control *findControl(Id id)
    {
        const auto found = std::find_if(m_controls.begin(), m_controls.end(),
                                        [id](const Control &control) { return control.id == id; });
        return found == m_controls.end() ? nullptr : &*found;
    }

    void addControl(const Mapping &mapping, CMIOObjectID object)
    {
        AudioValueRange range{};
        Float32 nativeValue = 0;
        if (!getScalar(object, kCMIOFeatureControlPropertyNativeRange, range)
            || !getScalar(object, kCMIOFeatureControlPropertyNativeValue, nativeValue)
            || !std::isfinite(range.mMinimum) || !std::isfinite(range.mMaximum)
            || range.mMaximum <= range.mMinimum) {
            return;
        }

        const auto valueProperty = address(kCMIOFeatureControlPropertyNativeValue);
        Boolean valueSettable = false;
        if (CMIOObjectIsPropertySettable(object, &valueProperty, &valueSettable) != noErr
            || !valueSettable) {
            return;
        }

        UInt32 automatic = 0;
        const auto autoProperty = address(kCMIOFeatureControlPropertyAutomaticManual);
        Boolean autoSettable = false;
        const bool autoSupported = CMIOObjectHasProperty(object, &autoProperty)
            && CMIOObjectIsPropertySettable(object, &autoProperty, &autoSettable) == noErr
            && autoSettable
            && getScalar(object, kCMIOFeatureControlPropertyAutomaticManual, automatic);

        const double ratio = std::clamp(
            (static_cast<double>(nativeValue) - range.mMinimum)
                / (range.mMaximum - range.mMinimum), 0.0, 1.0);
        const long uiValue = std::lround(UiMinimum + ratio * (UiMaximum - UiMinimum));

        Control control;
        control.id = mapping.id;
        control.name = QString::fromUtf8(mapping.name);
        control.minimum = UiMinimum;
        control.maximum = UiMaximum;
        control.step = 1;
        control.defaultValue = uiValue;
        control.value = uiValue;
        control.autoSupported = autoSupported;
        control.automatic = autoSupported && automatic != 0;
        control.defaultAutomatic = control.automatic;
        m_controls.append(control);

        Binding binding;
        binding.object = object;
        binding.minimum = range.mMinimum;
        binding.maximum = range.mMaximum;
        binding.autoSupported = autoSupported;
        m_bindings.insert(mapping.id, binding);
    }

    QString m_error;
    QList<Control> m_controls;
    QHash<Id, Binding> m_bindings;
};

} // namespace

std::unique_ptr<NativeCameraControls> createNativeCameraControls()
{
    return std::make_unique<MacCameraControls>();
}
