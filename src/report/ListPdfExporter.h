#pragma once

#include "data/PersonRepository.h"

#include <QString>
#include <QVector>

/// Экспорт списка записей в PDF по макету `docs/PRINT-LAYOUT.md`.
///
/// A4 альбомный, поля 15 мм, шапка повторяется на каждой странице,
/// внизу — «Стр. X из Y». Строки идут в переданном порядке.
class ListPdfExporter
{
public:
    static bool exportList(const QVector<PersonRecord> &records,
                           const QString &filePath,
                           QString *error = nullptr);
};
