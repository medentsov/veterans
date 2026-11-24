#pragma once

#include "data/PersonRepository.h"

#include <QDate>
#include <QString>

class QPrinter;

/// Шаблон карточки ветерана (M8-01) по `docs/PRINT-LAYOUT.md`, раздел 2.
///
/// HTML рассчитан на A4 книжной ориентации и печать через QTextDocument:
/// шапка учреждения, рамка с ФИО, поля в порядке ТЗ, подпись внизу.
/// Карточка всегда занимает одну страницу и не разрывается.
class CardRenderer
{
public:
    /// HTML карточки. `cardNumber` — id записи, печатается с ведущими нулями
    /// до 6 знаков. Пустой `organizationName` заменяется значением по умолчанию.
    static QString toHtml(const PersonRecord &record,
                          int cardNumber,
                          const QDate &formedDate = QDate::currentDate(),
                          const QString &organizationName = QString());

    /// Готовит A4 книжную страницу с полями 20/15/15/15 мм и печатает карточку.
    static bool render(QPrinter &printer,
                       const PersonRecord &record,
                       int cardNumber,
                       const QDate &formedDate,
                       const QString &organizationName,
                       QString *error = nullptr);
};
