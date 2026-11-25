#pragma once

#include <QSqlDatabase>
#include <QString>

/// Открытие локального файла SQLite.
///
/// Порядок операций: проверка/создание каталога → открытие файла →
/// настройка pragma → PRAGMA quick_check → миграции. При любой ошибке
/// соединение закрывается, база остаётся нетронутой.
class Database
{
public:
    struct OpenResult {
        bool ok = false;
        bool created = false;  ///< файл создан при этом открытии
        QString error;         ///< текст для показа пользователю
    };

    /// Открывает (или создаёт) базу по указанному пути.
    static OpenResult open(const QString &path);

    /// Закрывает и удаляет соединение.
    static void close();

    static bool isOpen();

    /// Соединение по умолчанию; пустое, если база не открыта.
    static QSqlDatabase connection();

private:
    static bool configure(QSqlDatabase &db, QString *error);
    static bool quickCheck(QSqlDatabase &db, QString *error);
};
