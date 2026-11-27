#include "ui/CheckHeader.h"

#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
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

    // Рисуем чекбокс вручную: для PartiallyChecked используем минус/дефис
    // вместо закрашенного прямоугольника (State_NoChange), чтобы не
    // сбивать с толку пользователя («лишний прямоугольник» в заголовке).
    const int size = style()->pixelMetric(QStyle::PM_IndicatorWidth,
                                          nullptr, this);
    const QRect checkRect(rect.center().x() - size / 2,
                          rect.center().y() - size / 2,
                          size, size);

    // Фон чекбокса
    QStyleOptionButton btn;
    btn.rect = checkRect;
    btn.state = QStyle::State_Enabled | QStyle::State_Raised;
    style()->drawPrimitive(QStyle::PE_PanelButtonCommand, &btn, painter, this);

    // Рамка
    QPen pen = painter->pen();
    pen.setColor(palette().color(QPalette::WindowText));
    painter->setPen(pen);
    painter->setBrush(Qt::NoBrush);
    painter->drawRect(checkRect.adjusted(0, 0, -1, -1));

    // Содержимое в зависимости от состояния
    const int inset = qMax(2, size / 6);
    const QRect inner = checkRect.adjusted(inset, inset, -inset, -inset);

    if (m_state == Qt::Checked) {
        // Галочка
        painter->setRenderHint(QPainter::Antialiasing, true);
        QPainterPath tick;
        tick.moveTo(inner.left() + inner.width() * 0.2, inner.center().y());
        tick.lineTo(inner.center().x(), inner.bottom() - inner.height() * 0.15);
        tick.lineTo(inner.right() - inner.width() * 0.15, inner.top() + inner.height() * 0.15);
        painter->setPen(QPen(palette().color(QPalette::WindowText), qMax(1.5, size / 10.0),
                             Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter->drawPath(tick);
    } else if (m_state == Qt::PartiallyChecked) {
        // Минус/дефис вместо закрашенного квадрата
        const int barH = qMax(2, size / 8);
        const QRect bar(inner.center().x() - inner.width() / 3,
                        inner.center().y() - barH / 2,
                        inner.width() * 2 / 3, barH);
        painter->fillRect(bar, palette().color(QPalette::WindowText));
    }
    // Unchecked — пустой квадрат, ничего не рисуем внутри

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
