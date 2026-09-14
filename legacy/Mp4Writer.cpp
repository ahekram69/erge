#include "Mp4Writer.h"
#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <wrl/client.h>
#include <algorithm>
using Microsoft::WRL::ComPtr;

struct Mp4Writer::Impl {
    ComPtr<IMFSinkWriter> writer;
    DWORD stream = 0;
    QSize size;
    QString error;
    bool initialized = false;
    qint64 last = -1;
    bool check(HRESULT hr) {
        if (SUCCEEDED(hr)) return true;
        error = QStringLiteral("Media Foundation: 0x%1").arg(quint32(hr), 8, 16, QLatin1Char('0'));
        return false;
    }
};
Mp4Writer::Mp4Writer() : d(new Impl) { d->initialized = d->check(MFStartup(MF_VERSION)); }
Mp4Writer::~Mp4Writer() { finish(); if (d->initialized) MFShutdown(); }
bool Mp4Writer::active() const { return bool(d->writer); }
QString Mp4Writer::error() const { return d->error; }
bool Mp4Writer::start(const QString &path, QSize size) {
    if (active() || !d->initialized) return false;
    if (size.isEmpty() || size.width()%2 || size.height()%2) {
        d->error = QStringLiteral("H.264 requires even frame dimensions"); return false;
    }
    d->error.clear();
    d->last = -1;
    d->size = size;
    ComPtr<IMFSinkWriter> writer;
    if (!d->check(MFCreateSinkWriterFromURL(reinterpret_cast<LPCWSTR>(path.utf16()), nullptr, nullptr, &writer))) return false;
    ComPtr<IMFMediaType> output, input;
    if (!d->check(MFCreateMediaType(&output)) || !d->check(MFCreateMediaType(&input))) return false;
    auto common = [&](IMFMediaType *type, const GUID &subtype) {
        return d->check(type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video))
            && d->check(type->SetGUID(MF_MT_SUBTYPE, subtype))
            && d->check(type->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive))
            && d->check(MFSetAttributeSize(type, MF_MT_FRAME_SIZE, size.width(), size.height()))
            && d->check(MFSetAttributeRatio(type, MF_MT_FRAME_RATE, 30, 1))
            && d->check(MFSetAttributeRatio(type, MF_MT_PIXEL_ASPECT_RATIO, 1, 1));
    };
    if (!common(output.Get(), MFVideoFormat_H264)
        || !d->check(output->SetUINT32(MF_MT_AVG_BITRATE, 8000000))
        || !d->check(writer->AddStream(output.Get(), &d->stream))
        || !common(input.Get(), MFVideoFormat_NV12)
        || !d->check(writer->SetInputMediaType(d->stream, input.Get(), nullptr))
        || !d->check(writer->BeginWriting())) return false;
    d->writer = writer;
    return true;
}
bool Mp4Writer::write(const QImage &source, qint64 timestamp) {
    if (!active() || source.size() != d->size) { d->error = QStringLiteral("Recording frame size changed"); return false; }
    timestamp = std::max(timestamp, d->last + 1);
    const QImage image = source.convertToFormat(QImage::Format_RGB32);
    const int w = image.width(), h = image.height();
    const DWORD length = DWORD(w * h * 3 / 2);
    ComPtr<IMFMediaBuffer> buffer;
    if (!d->check(MFCreateMemoryBuffer(length, &buffer))) return false;
    BYTE *bytes = nullptr;
    if (!d->check(buffer->Lock(&bytes, nullptr, nullptr))) return false;
    auto clamp = [](int n) { return BYTE(std::clamp(n, 0, 255)); };
    // Top-down NV12 with BT.601 limited-range samples; no implicit RGB converter.
    for (int y=0; y<h; ++y) {
        const auto *row = reinterpret_cast<const QRgb *>(image.constScanLine(y));
        for (int x=0; x<w; ++x) {
            const QRgb p=row[x];
            bytes[y*w+x]=clamp(((66*qRed(p)+129*qGreen(p)+25*qBlue(p)+128)>>8)+16);
        }
    }
    for (int y=0; y<h; y+=2) {
        const auto *a = reinterpret_cast<const QRgb *>(image.constScanLine(y));
        const auto *b = reinterpret_cast<const QRgb *>(image.constScanLine(y+1));
        for (int x=0; x<w; x+=2) {
            int r=0,g=0,bl=0;
            for (QRgb p : {a[x],a[x+1],b[x],b[x+1]}) { r+=qRed(p);g+=qGreen(p);bl+=qBlue(p); }
            r/=4;g/=4;bl/=4;
            const int index=w*h+(y/2)*w+x;
            bytes[index]=clamp(((-38*r-74*g+112*bl+128)>>8)+128);
            bytes[index+1]=clamp(((112*r-94*g-18*bl+128)>>8)+128);
        }
    }
    buffer->Unlock();
    ComPtr<IMFSample> sample;
    if (!d->check(buffer->SetCurrentLength(length)) || !d->check(MFCreateSample(&sample))
        || !d->check(sample->AddBuffer(buffer.Get()))
        || !d->check(sample->SetSampleTime(timestamp))
        || !d->check(sample->SetSampleDuration(10000000/30))
        || !d->check(d->writer->WriteSample(d->stream, sample.Get()))) return false;
    d->last=timestamp;
    return true;
}
bool Mp4Writer::finish() {
    if (!active()) return true;
    const HRESULT result=d->writer->Finalize();
    d->writer.Reset();
    return d->check(result);
}
