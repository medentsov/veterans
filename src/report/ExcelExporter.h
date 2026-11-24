#pragma once

#include "data/PersonRepository.h"

#include <QString>
#include <QVector>

/// Экспорт списка записей в Excel (`.xlsx`).
///
/// Колонки совпадают с печатным списком (`docs/PRINT-LAYOUT.md`, раздел 3):
/// №, ФИО, микрорайон, год рождения, льготная категория, год посещения,
/// подписка. Шапка оформлена, первая строка закреплена, включён автофильтр.
/// Пустые значения выводятся как «—».
///
/// Область экспорта (отмеченные / все / результат фильтра) выбирает
/// вызывающий код — см. M7-02.
class ExcelExporter
{
public:
    /// Сохраняет записи в файл `filePath` в переданном порядке.
    /// При ошибке возвращает false и заполняет `error` понятным текстом.
    static bool exportList(const QVector<PersonRecord> &records,
                           const QString &filePath,
                           QString *error = nullptr);
};
