#include "viewzone.h"

#include <QPainter>
#include <QMouseEvent>
#include <QWheelEvent>

ViewZone::ViewZone(QWidget *parent)
    : QWidget(parent)
{
}

void ViewZone::setZone(int topY, int bottomY)
{
    if (m_zoneTop == topY && m_zoneBottom == bottomY) {
        return;
    }
    m_zoneTop = topY;
    m_zoneBottom = bottomY;
    update();
}

void ViewZone::paintEvent(QPaintEvent * /*event*/)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, false);

    const QRect rect = this->rect();

    QColor dimColor(0, 0, 0, 40);
    painter.fillRect(rect, dimColor);

    int zoneTop = qMax(m_zoneTop, rect.top());
    int zoneBottom = qMin(m_zoneBottom, rect.bottom());
    if (zoneTop < zoneBottom) {
        QColor zoneColor(palette().color(QPalette::Highlight));
        zoneColor.setAlpha(60);
        painter.fillRect(QRect(0, zoneTop, rect.width(), zoneBottom - zoneTop), zoneColor);
    }

    QColor borderColor(palette().color(QPalette::Highlight));
    borderColor.setAlpha(120);
    painter.setPen(borderColor);
    if (m_zoneTop >= rect.top() && m_zoneTop <= rect.bottom()) {
        painter.drawLine(0, m_zoneTop, rect.width(), m_zoneTop);
    }
    if (m_zoneBottom >= rect.top() && m_zoneBottom <= rect.bottom()) {
        painter.drawLine(0, m_zoneBottom, rect.width(), m_zoneBottom);
    }
}

void ViewZone::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragging = true;
        emit zoneClicked(event->pos().y());
    }
}

void ViewZone::mouseMoveEvent(QMouseEvent *event)
{
    if (m_dragging) {
        emit zoneClicked(event->pos().y());
    }
}

void ViewZone::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragging = false;
    }
}

void ViewZone::wheelEvent(QWheelEvent *event)
{
    emit wheelScrolled(event->angleDelta().y());
    event->accept();
}
