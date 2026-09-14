#pragma once
#include <QImage>
#include <QString>
#include <memory>

// Uses Windows 7 Media Foundation instead of Qt 5's missing camera recorder.
class Mp4Writer {
public:
    Mp4Writer();
    ~Mp4Writer();
    bool start(const QString &path, QSize size);
    bool write(const QImage &image, qint64 timestamp100ns);
    bool finish();
    bool active() const;
    QString error() const;
private:
    struct Impl;
    std::unique_ptr<Impl> d;
};
