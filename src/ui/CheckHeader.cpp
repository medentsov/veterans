#include "ui/CheckHeader.h"

#include <QMouseEvent>
#include <QPainter>
#include <QStyle>
#include <QStyleOptionButton>
#include <QStyleOptionHeader>

CheckHeader::CheckHeader(Qt::Orientation orientation, QWidget *parent)
    : QHeaderView(orientation, parent)
{
    setSectionsClickable(true);
    setHighlightSections(false);
    setSortIndicatorShown(true);
}

void CheckHeader::setCheckState(Qt::CheckState state)
{
    if (m_state == state)
        return;

    m_state = state;
    if (count() > 0)
        updateSection(0);
}

void CheckHeader::paintSection(QPainter *painter, const QRect &rect,
                               int logicalIndex) const
{
    if (logicalIndex != 0) {
        QHeaderView::paintSection(painter, rect, logicalIndex);
        return;
    }

    painter->save();

    QStyleOptionHeader option;
    initStyleOption(&option);
    option.rect = rect;
    option.section = logicalIndex;
    option.text.clear();
    style()->drawControl(QStyle::CE_Header, &option, painter, this);

    QStyleOptionButton check;
    check.state = QStyle::State_Enabled;
    switch (m_state) {
    case Qt::Checked:
        check.state |= QStyle::State_On;
        break;
    case Qt::PartiallyChecked:
        check.state |= QStyle::State_NoChange;
        break;
    default:
        check.state |= QStyle::State_Off;
        break;
    }

    const int size = style()->pixelMetric(QStyle::PM_IndicatorWidth,
                                          nullptr, this);
    check.rect = QRect(rect.center().x() - size / 2,
                       rect.center().y() - size / 2,
                       size, size);
    style()->drawPrimitive(QStyle::PE_IndicatorCheckBox, &check, painter, this);

    painter->restore();
}

void CheckHeader::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton
        && logicalIndexAt(event->pos()) == 0) {
        emit toggleAllRequested();
        event->accept();
        return;
    }

    QHeaderView::mousePressEvent(event);
}
