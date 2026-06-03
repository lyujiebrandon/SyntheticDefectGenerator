#pragma once

#include <QWidget>
#include <QPixmap>

class ZoomableImageLabel : public QWidget
{
    Q_OBJECT
public:
    explicit ZoomableImageLabel(QWidget* parent = nullptr);

    void setPixmap(const QPixmap& pixmap);
    void clearImage();
    void setPlaceholder(const QString& text);
    bool hasImage() const { return !m_pixmap.isNull(); }
    void fitToView();

protected:
    void paintEvent(QPaintEvent*) override;
    void wheelEvent(QWheelEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void mouseDoubleClickEvent(QMouseEvent*) override;
    void resizeEvent(QResizeEvent*) override;
    void enterEvent(QEnterEvent*) override;
    void leaveEvent(QEvent*) override;

private:
    QRectF fitRect() const;

    QPixmap m_pixmap;
    QString m_placeholder  = "No image loaded";
    double  m_zoom         = 1.0;
    QPointF m_pan;
    QPointF m_dragStart;
    QPointF m_panStart;
    bool    m_dragging     = false;
    bool    m_hovered      = false;

    static constexpr double kZoomMin = 0.05;
    static constexpr double kZoomMax = 16.0;
};
