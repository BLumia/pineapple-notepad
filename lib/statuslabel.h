#pragma once

#include <QLabel>
#include <QMouseEvent>

class StatusLabel : public QLabel
{
    Q_OBJECT
public:
    explicit StatusLabel(int preferredWidth, const QString &text, QWidget *parent = nullptr);

signals:
    void clicked(Qt::MouseButton button);
    void doubleClicked();

protected:
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;

private:
    int m_preferredWidth = 0;
};
