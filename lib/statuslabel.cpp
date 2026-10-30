#include "statuslabel.h"

#include <QSize>

StatusLabel::StatusLabel(int preferredWidth, const QString &text, QWidget *parent)
    : QLabel(text, parent)
    , m_preferredWidth(preferredWidth)
{
    setTextFormat(Qt::PlainText);
    setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
}

QSize StatusLabel::sizeHint() const
{
    QSize size = QLabel::sizeHint();
    size.setWidth(m_preferredWidth);
    return size;
}

QSize StatusLabel::minimumSizeHint() const
{
    return QSize();
}

void StatusLabel::mouseReleaseEvent(QMouseEvent *event)
{
    QLabel::mouseReleaseEvent(event);
    if (event) {
        emit clicked(event->button());
    }
}

void StatusLabel::mouseDoubleClickEvent(QMouseEvent *event)
{
    QLabel::mouseDoubleClickEvent(event);
    if (event && event->button() == Qt::LeftButton) {
        emit doubleClicked();
        emit customContextMenuRequested(event->pos());
    }
}
