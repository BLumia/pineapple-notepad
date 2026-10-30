#pragma once

#include <QWidget>

class QPaintEvent;
class QMouseEvent;
class QWheelEvent;

class ViewZone : public QWidget
{
    Q_OBJECT

public:
    explicit ViewZone(QWidget *parent = nullptr);

    void setZone(int topY, int bottomY);
    int zoneTop() const { return m_zoneTop; }
    int zoneBottom() const { return m_zoneBottom; }
    int zoneCenter() const { return (m_zoneTop + m_zoneBottom) / 2; }

signals:
    void zoneClicked(int yPos);
    void wheelScrolled(int delta);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

private:
    int m_zoneTop = 0;
    int m_zoneBottom = 100;
    bool m_dragging = false;
};
