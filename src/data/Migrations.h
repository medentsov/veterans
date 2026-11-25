#pragma once

#include <QSqlDatabase>
#include <QString>

/// Миграции схемы БД.
///
/// Версия схемы хранится в PRAGMA user_version и увеличивается на 1 с каждой
/// миграцией. Миграции применяются по порядку и только те, что ещё не применены;
/// каждая выполняется в транзакции — при ошибке база остаётся на прошлой версии.
namespace Migrations {

/// Текущая версия схемы или -1 при ошибке.
int currentVersion(QSqlDatabase &db, QString *error = nullptr);

/// Доводит схему до актуальной версии.
bool apply(QSqlDatabase &db, QString *error = nullptr);

} // namespace Migrations
