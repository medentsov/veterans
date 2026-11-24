#pragma once

#include "data/PersonRepository.h"

#include <QString>
#include <QVector>

class QPrinter;

/// Шаблон печатного списка (M8-02).
///
/// Рисует альбомный A4 с полями 15 мм: шапка повторяется на каждой странице,
/// строки не разрываются, внизу — «Стр. X из Y». Один и тот же рендерер
/// используется для PDF (M7-03) и для печати/предпросмотра (M8-03+).
class ListRenderer
{
public:
    /// Настраивает переданный принтер и печатает список. Для PDF вызывающий
    /// код заранее задаёт формат и имя файла; для реального принтера —
    /// выбирает его в QPrintDialog.
    static bool render(QPrinter &printer,
                       const QVector<PersonRecord> &records,
                       QString *error = nullptr);
};
