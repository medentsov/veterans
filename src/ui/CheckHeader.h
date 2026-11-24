#pragma once

#include <QHeaderView>

/// Заголовок таблицы с трёхпозиционным чекбоксом «выбрать все».
///
/// Чекбокс рисуется только в колонке 0 (колонка отметок). Клик по нему
/// не сортирует таблицу, а испускает toggleAllRequested; главное окно
/// решает, какие строки отметить — все видимые или только отфильтрованные.
class CheckHeader : public QHeaderView
{
    Q_OBJECT

public:
    explicit CheckHeader(Qt::Orientation orientation,
                         QWidget *parent = nullptr);

    void setCheckState(Qt::CheckState state);
    Qt::CheckState checkState() const { return m_state; }

signals:
    void toggleAllRequested();

protected:
    void paintSection(QPainter *painter, const QRect &rect,
                      int logicalIndex) const override;
    void mousePressEvent(QMouseEvent *event) override;

private:
    Qt::CheckState m_state = Qt::Unchecked;
};
