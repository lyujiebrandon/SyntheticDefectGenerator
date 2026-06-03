#include "ZoomableImageLabel.h"

#include <QPainter>
#include <QWheelEvent>
#include <QMouseEvent>
#include <algorithm>
#include <cmath>

ZoomableImageLabel::ZoomableImageLabel(QWidget* parent)
    : QWidget(parent)
{
    setMinimumSize(200, 150);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setMouseTracking(true);
}

void ZoomableImageLabel::setPixmap(const QPixmap& pixmap)
{
    m_pixmap = pixmap;
    fitToView();
}

void ZoomableImageLabel::clearImage()
{
    m_pixmap = QPixmap();
    fitToView();
}

void ZoomableImageLabel::setPlaceholder(const QString& text)
{
    m_placeholder = text;
    update();
}

void ZoomableImageLabel::fitToView()
{
    m_zoom = 1.0;
    m_pan  = {0.0, 0.0};
    update();
}

QRectF ZoomableImageLabel::fitRect() const
{
    if (m_pixmap.isNull()) return {};
    double fw = width(), fh = height();
    double iw = m_pixmap.width(), ih = m_pixmap.height();
    double s  = std::min(fw / iw, fh / ih) * m_zoom;
    double sw = iw * s, sh = ih * s;
    return { (fw - sw) / 2.0 + m_pan.x(),
             (fh - sh) / 2.0 + m_pan.y(),
             sw, sh };
}

void ZoomableImageLabel::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::SmoothPixmapTransform);
    p.fillRect(rect(), QColor(0xe8, 0xea, 0xf8));

    if (m_pixmap.isNull()) {
        p.setPen(QColor(0xa0, 0xa8, 0xd0));
        QFont f = p.font();
        f.setPointSize(9);
        p.setFont(f);
        p.drawText(rect(), Qt::AlignCenter, m_placeholder);
        return;
    }

    p.drawPixmap(fitRect().toRect(), m_pixmap);

    // Overlay hint / zoom indicator in bottom-right corner
    QFont f = p.font();
    f.setPointSize(8);
    p.setFont(f);

    if (m_zoom != 1.0) {
        p.setPen(QColor(30, 55, 160, 200));
        f.setBold(true);
        p.setFont(f);
        p.drawText(rect().adjusted(0, 0, -7, -5),
                   Qt::AlignBottom | Qt::AlignRight,
                   QString("%1%").arg(static_cast<int>(std::round(m_zoom * 100))));
    } else if (m_hovered) {
        p.setPen(QColor(60, 80, 160, 110));
        p.drawText(rect().adjusted(0, 0, -7, -5),
                   Qt::AlignBottom | Qt::AlignRight,
                   "Scroll to zoom  \xB7  Drag to pan  \xB7  Dbl-click to reset");
    }
}

void ZoomableImageLabel::wheelEvent(QWheelEvent* event)
{
    if (m_pixmap.isNull()) return;
    QPointF pos(event->position());
    QPointF ctr(width() / 2.0, height() / 2.0);
    double  factor  = event->angleDelta().y() > 0 ? 1.18 : 1.0 / 1.18;
    double  newZoom = std::clamp(m_zoom * factor, kZoomMin, kZoomMax);
    // Keep the pixel under the cursor fixed
    m_pan   = pos - ctr - (pos - ctr - m_pan) * (newZoom / m_zoom);
    m_zoom  = newZoom;
    update();
    event->accept();
}

void ZoomableImageLabel::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && !m_pixmap.isNull()) {
        m_dragging  = true;
        m_dragStart = event->position();
        m_panStart  = m_pan;
        setCursor(Qt::ClosedHandCursor);
    }
}

void ZoomableImageLabel::mouseMoveEvent(QMouseEvent* event)
{
    if (m_dragging) {
        m_pan = m_panStart + (event->position() - m_dragStart);
        update();
    } else if (!m_pixmap.isNull()) {
        setCursor(Qt::OpenHandCursor);
    }
}

void ZoomableImageLabel::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragging = false;
        setCursor(m_pixmap.isNull() ? Qt::ArrowCursor : Qt::OpenHandCursor);
    }
}

void ZoomableImageLabel::mouseDoubleClickEvent(QMouseEvent*)
{
    fitToView();
}

void ZoomableImageLabel::resizeEvent(QResizeEvent*)
{
    if (m_zoom == 1.0) fitToView();
}

void ZoomableImageLabel::enterEvent(QEnterEvent*)
{
    m_hovered = true;
    if (!m_pixmap.isNull()) setCursor(Qt::OpenHandCursor);
    update();
}

void ZoomableImageLabel::leaveEvent(QEvent*)
{
    m_hovered = false;
    setCursor(Qt::ArrowCursor);
    update();
}
