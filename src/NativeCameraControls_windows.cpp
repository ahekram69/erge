#include "NativeCameraControls.h"

#include <QHash>

#include <dshow.h>
#include <wrl/client.h>

#include <algorithm>

using Microsoft::WRL::ComPtr;

namespace {

enum class InterfaceKind { VideoProcAmp, CameraControl };

struct Mapping {
    NativeCameraControls::Id id;
    const char *name;
    InterfaceKind kind;
    long property;
};

constexpr Mapping mappings[] = {
    { NativeCameraControls::Id::Brightness, "亮度", InterfaceKind::VideoProcAmp, VideoProcAmp_Brightness },
    { NativeCameraControls::Id::Contrast, "对比度", InterfaceKind::VideoProcAmp, VideoProcAmp_Contrast },
    { NativeCameraControls::Id::Hue, "色相", InterfaceKind::VideoProcAmp, VideoProcAmp_Hue },
    { NativeCameraControls::Id::Saturation, "饱和度", InterfaceKind::VideoProcAmp, VideoProcAmp_Saturation },
    { NativeCameraControls::Id::Sharpness, "锐度", InterfaceKind::VideoProcAmp, VideoProcAmp_Sharpness },
    { NativeCameraControls::Id::Gamma, "Gamma", InterfaceKind::VideoProcAmp, VideoProcAmp_Gamma },
    { NativeCameraControls::Id::WhiteBalance, "白平衡", InterfaceKind::VideoProcAmp, VideoProcAmp_WhiteBalance },
    { NativeCameraControls::Id::BacklightCompensation, "背光补偿", InterfaceKind::VideoProcAmp, VideoProcAmp_BacklightCompensation },
    { NativeCameraControls::Id::Gain, "增益", InterfaceKind::VideoProcAmp, VideoProcAmp_Gain },
    { NativeCameraControls::Id::Zoom, "变焦", InterfaceKind::CameraControl, CameraControl_Zoom },
    { NativeCameraControls::Id::Exposure, "曝光", InterfaceKind::CameraControl, CameraControl_Exposure },
    { NativeCameraControls::Id::Focus, "对焦", InterfaceKind::CameraControl, CameraControl_Focus },
};

const Mapping *findMapping(NativeCameraControls::Id id)
{
    const auto it = std::find_if(std::begin(mappings), std::end(mappings),
                                 [id](const Mapping &mapping) { return mapping.id == id; });
    return it == std::end(mappings) ? nullptr : it;
}

class WindowsCameraControls final : public NativeCameraControls
{
public:
    WindowsCameraControls()
    {
        const HRESULT result = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        m_uninitializeCom = SUCCEEDED(result);
    }

    ~WindowsCameraControls() override
    {
        m_cameraControl.Reset();
        m_videoProcAmp.Reset();
        m_filter.Reset();
        if (m_uninitializeCom)
            CoUninitialize();
    }

    bool open(const QString &deviceName) override
    {
        m_error.clear();
        m_controls.clear();
        m_cameraControl.Reset();
        m_videoProcAmp.Reset();
        m_filter.Reset();

        ComPtr<ICreateDevEnum> deviceEnumerator;
        HRESULT hr = CoCreateInstance(CLSID_SystemDeviceEnum, nullptr, CLSCTX_INPROC_SERVER,
                                      IID_PPV_ARGS(&deviceEnumerator));
        if (FAILED(hr))
            return fail(QStringLiteral("无法创建 Windows 视频设备枚举器"));

        ComPtr<IEnumMoniker> monikers;
        hr = deviceEnumerator->CreateClassEnumerator(CLSID_VideoInputDeviceCategory,
                                                     &monikers, 0);
        if (hr != S_OK)
            return fail(QStringLiteral("Windows 没有返回视频输入设备"));

        ComPtr<IMoniker> moniker;
        ULONG fetched = 0;
        while (monikers->Next(1, &moniker, &fetched) == S_OK) {
            ComPtr<IPropertyBag> properties;
            if (SUCCEEDED(moniker->BindToStorage(nullptr, nullptr, IID_PPV_ARGS(&properties)))) {
                VARIANT value;
                VariantInit(&value);
                if (SUCCEEDED(properties->Read(L"FriendlyName", &value, nullptr))
                    && value.vt == VT_BSTR) {
                    const QString friendlyName = QString::fromWCharArray(value.bstrVal);
                    if (friendlyName.compare(deviceName, Qt::CaseInsensitive) == 0) {
                        hr = moniker->BindToObject(nullptr, nullptr, IID_PPV_ARGS(&m_filter));
                        VariantClear(&value);
                        break;
                    }
                }
                VariantClear(&value);
            }
            moniker.Reset();
        }

        if (!m_filter)
            return fail(QStringLiteral("未找到与“%1”匹配的 DirectShow 设备").arg(deviceName));

        m_filter.As(&m_videoProcAmp);
        m_filter.As(&m_cameraControl);
        queryControls();
        if (m_controls.isEmpty())
            return fail(QStringLiteral("摄像头没有公开标准 UVC 控制参数"));
        return true;
    }

    QList<Control> controls() const override { return m_controls; }

    bool setValue(Id id, long value) override
    {
        const Mapping *mapping = findMapping(id);
        if (!mapping)
            return false;
        bool succeeded = false;
        if (mapping->kind == InterfaceKind::VideoProcAmp && m_videoProcAmp)
            succeeded = SUCCEEDED(m_videoProcAmp->Set(mapping->property, value, VideoProcAmp_Flags_Manual));
        if (mapping->kind == InterfaceKind::CameraControl && m_cameraControl)
            succeeded = SUCCEEDED(m_cameraControl->Set(mapping->property, value, CameraControl_Flags_Manual));
        if (succeeded) {
            if (Control *control = findMutableControl(id)) {
                control->value = value;
                control->automatic = false;
            }
        }
        return succeeded;
    }

    bool setAutomatic(Id id, bool enabled) override
    {
        const Mapping *mapping = findMapping(id);
        if (!mapping)
            return false;
        const Control *control = findControl(id);
        if (!control)
            return false;
        const long value = control->value;
        bool succeeded = false;
        if (mapping->kind == InterfaceKind::VideoProcAmp && m_videoProcAmp) {
            const long flags = enabled ? VideoProcAmp_Flags_Auto : VideoProcAmp_Flags_Manual;
            succeeded = SUCCEEDED(m_videoProcAmp->Set(mapping->property, value, flags));
        }
        if (mapping->kind == InterfaceKind::CameraControl && m_cameraControl) {
            const long flags = enabled ? CameraControl_Flags_Auto : CameraControl_Flags_Manual;
            succeeded = SUCCEEDED(m_cameraControl->Set(mapping->property, value, flags));
        }
        if (succeeded) {
            if (Control *control = findMutableControl(id))
                control->automatic = enabled;
        }
        return succeeded;
    }

    QString errorString() const override { return m_error; }

private:
    bool fail(const QString &message)
    {
        m_error = message;
        return false;
    }

    const Control *findControl(Id id) const
    {
        const auto it = std::find_if(m_controls.cbegin(), m_controls.cend(),
                                     [id](const Control &control) { return control.id == id; });
        return it == m_controls.cend() ? nullptr : &*it;
    }

    Control *findMutableControl(Id id)
    {
        const auto it = std::find_if(m_controls.begin(), m_controls.end(),
                                     [id](const Control &control) { return control.id == id; });
        return it == m_controls.end() ? nullptr : &*it;
    }

    void queryControls()
    {
        for (const Mapping &mapping : mappings) {
            long minimum = 0;
            long maximum = 0;
            long step = 0;
            long defaultValue = 0;
            long capabilities = 0;
            long value = 0;
            long flags = 0;
            HRESULT rangeResult = E_NOINTERFACE;
            HRESULT valueResult = E_NOINTERFACE;

            if (mapping.kind == InterfaceKind::VideoProcAmp && m_videoProcAmp) {
                rangeResult = m_videoProcAmp->GetRange(mapping.property, &minimum, &maximum,
                                                       &step, &defaultValue, &capabilities);
                valueResult = m_videoProcAmp->Get(mapping.property, &value, &flags);
            } else if (mapping.kind == InterfaceKind::CameraControl && m_cameraControl) {
                rangeResult = m_cameraControl->GetRange(mapping.property, &minimum, &maximum,
                                                        &step, &defaultValue, &capabilities);
                valueResult = m_cameraControl->Get(mapping.property, &value, &flags);
            }

            if (FAILED(rangeResult) || FAILED(valueResult) || maximum < minimum)
                continue;

            Control control;
            control.id = mapping.id;
            control.name = QString::fromUtf8(mapping.name);
            control.minimum = minimum;
            control.maximum = maximum;
            control.step = std::max(1L, step);
            // "Restore Initial Settings" means the state observed when this
            // camera was opened, not the driver's factory value. Factory
            // values can be unexpectedly dark and may not match auto modes.
            control.defaultValue = value;
            control.value = value;
            if (mapping.kind == InterfaceKind::VideoProcAmp) {
                control.autoSupported = (capabilities & VideoProcAmp_Flags_Auto) != 0;
                control.automatic = (flags & VideoProcAmp_Flags_Auto) != 0;
            } else {
                control.autoSupported = (capabilities & CameraControl_Flags_Auto) != 0;
                control.automatic = (flags & CameraControl_Flags_Auto) != 0;
            }
            control.defaultAutomatic = control.automatic;
            m_controls.append(control);
        }
    }

    bool m_uninitializeCom = false;
    QString m_error;
    QList<Control> m_controls;
    ComPtr<IBaseFilter> m_filter;
    ComPtr<IAMVideoProcAmp> m_videoProcAmp;
    ComPtr<IAMCameraControl> m_cameraControl;
};

} // namespace

std::unique_ptr<NativeCameraControls> createNativeCameraControls()
{
    return std::make_unique<WindowsCameraControls>();
}
