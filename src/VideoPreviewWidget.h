#pragma once

#include <QVideoFrame>
#include <QWidget>

class QGraphicsScene;
class QGraphicsVideoItem;
class QGraphicsView;
class QVideoSink;

class VideoPreviewWidget final : public QWidget
{
    Q_OBJECT

public:
    explicit VideoPreviewWidget(QWidget *parent = nullptr);

    QVideoSink *videoSink() const;
    QImage currentImage() const;

    void setMirrored(bool mirrored);
    void setFlippedVertically(bool flipped);
    void setRotation(int degrees);

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    void updateTransform();
    void fitVideo();

    QGraphicsScene *m_scene = nullptr;
    QGraphicsView *m_view = nullptr;
    QGraphicsVideoItem *m_videoItem = nullptr;
    QVideoFrame m_lastFrame;
    bool m_mirrored = false;
    bool m_flippedVertically = false;
    int m_rotation = 0;
};
