#include "VideoPreviewWidget.h"

#include <QGraphicsScene>
#include <QGraphicsVideoItem>
#include <QGraphicsView>
#include <QResizeEvent>
#include <QScrollBar>
#include <QTransform>
#include <QVBoxLayout>
#include <QVideoSink>

VideoPreviewWidget::VideoPreviewWidget(QWidget *parent)
    : QWidget(parent)
{
    setMinimumSize(720, 480);

    m_scene = new QGraphicsScene(this);
    m_videoItem = new QGraphicsVideoItem;
    m_videoItem->setAspectRatioMode(Qt::KeepAspectRatio);
    m_scene->addItem(m_videoItem);

    m_view = new QGraphicsView(m_scene, this);
    m_view->setFrameShape(QFrame::NoFrame);
    m_view->setStyleSheet(QStringLiteral("background: #111; border-radius: 6px;"));
    m_view->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_view->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_view->setAlignment(Qt::AlignCenter);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_view);

    connect(m_videoItem->videoSink(), &QVideoSink::videoFrameChanged,
            this, [this](const QVideoFrame &frame) {
                if (frame.isValid())
                    m_lastFrame = frame;
            });
    connect(m_videoItem, &QGraphicsVideoItem::nativeSizeChanged,
            this, [this](const QSizeF &) { fitVideo(); });
}

QVideoSink *VideoPreviewWidget::videoSink() const
{
    return m_videoItem->videoSink();
}

QImage VideoPreviewWidget::currentImage() const
{
    QImage image = m_lastFrame.toImage();
    if (image.isNull())
        return {};

    QTransform transform;
    transform.scale(m_mirrored ? -1.0 : 1.0,
                    m_flippedVertically ? -1.0 : 1.0);
    transform.rotate(m_rotation);
    return image.transformed(transform, Qt::SmoothTransformation);
}

void VideoPreviewWidget::setMirrored(bool mirrored)
{
    m_mirrored = mirrored;
    updateTransform();
}

void VideoPreviewWidget::setFlippedVertically(bool flipped)
{
    m_flippedVertically = flipped;
    updateTransform();
}

void VideoPreviewWidget::setRotation(int degrees)
{
    m_rotation = ((degrees % 360) + 360) % 360;
    updateTransform();
}

void VideoPreviewWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    fitVideo();
}

void VideoPreviewWidget::updateTransform()
{
    const QRectF bounds = m_videoItem->boundingRect();
    const QPointF center = bounds.center();
    QTransform transform;
    transform.translate(center.x(), center.y());
    transform.rotate(m_rotation);
    transform.scale(m_mirrored ? -1.0 : 1.0,
                    m_flippedVertically ? -1.0 : 1.0);
    transform.translate(-center.x(), -center.y());
    m_videoItem->setTransform(transform);
    fitVideo();
}

void VideoPreviewWidget::fitVideo()
{
    if (m_videoItem->nativeSize().isEmpty())
        return;
    m_scene->setSceneRect(m_videoItem->sceneBoundingRect());
    m_view->fitInView(m_scene->sceneRect(), Qt::KeepAspectRatio);
}
