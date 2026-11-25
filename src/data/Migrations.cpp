#include "data/Migrations.h"

#include <QFile>
#include <QSqlError>
#include <QSqlQuery>
#include <QStringList>
#include <QVariant>

namespace {

struct Migration {
    int version;
    const char *resource;
};

/// Порядок важен: версии строго возрастают.
const Migration kMigrations[] = {
    { 1, ":/db/001_init.sql" },
    { 2, ":/db/002_seed.sql" },
    { 3, ":/db/003_phone.sql" },
};

bool fail(QString *error, const QString &text)
{
    if (error)
        *error = text;
    return false;
}

/// Разбивает SQL-файл на отдельные операторы.
/// QSQLITE выполняет один оператор за вызов, поэтому массив выполняем поштучно.
/// Комментарии `--` вырезаются; точки с запятой внутри строковых литералов
/// в наших миграциях не используются (сознательное ограничение).
QStringList splitStatements(const QString &sql)
{
    QString cleaned;
    const QStringList lines = sql.split(QLatin1Char('\n'));
    for (QString line : lines) {
        const int comment = line.indexOf(QStringLiteral("--"));
        if (comment >= 0)
            line.truncate(comment);
        cleaned += line;
        cleaned += QLatin1Char('\n');
    }

    QStringList statements;
    const QStringList parts = cleaned.split(QLatin1Char(';'));
    for (const QString &part : parts) {
        const QString statement = part.trimmed();
        if (!statement.isEmpty())
            statements.append(statement);
    }
    return statements;
}

bool applyOne(QSqlDatabase &db, const Migration &migration, QString *error)
{
    QFile file(QString::fromLatin1(migration.resource));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return fail(error, QStringLiteral("Не найден ресурс миграции %1 (%2)")
                               .arg(migration.version)
                               .arg(QString::fromLatin1(migration.resource)));
    }

    const QStringList statements =
        splitStatements(QString::fromUtf8(file.readAll()));

    if (!db.transaction())
        return fail(error, QStringLiteral("Не удалось начать транзакцию: %1")
                               .arg(db.lastError().text()));

    for (const QString &statement : statements) {
        QSqlQuery query(db);
        if (!query.exec(statement)) {
            db.rollback();
            return fail(error, QStringLiteral("Миграция %1: %2")
                                   .arg(migration.version)
                                   .arg(query.lastError().text()));
        }
    }

    QSqlQuery versionQuery(db);
    if (!versionQuery.exec(QStringLiteral("PRAGMA user_version = %1")
                               .arg(migration.version))) {
        db.rollback();
        return fail(error, QStringLiteral("Миграция %1: не удалось обновить "
                                          "user_version: %2")
                               .arg(migration.version)
                               .arg(versionQuery.lastError().text()));
    }

    if (!db.commit()) {
        db.rollback();
        return fail(error, QStringLiteral("Миграция %1: ошибка фиксации: %2")
                               .arg(migration.version)
                               .arg(db.lastError().text()));
    }
    return true;
}

} // namespace

int Migrations::currentVersion(QSqlDatabase &db, QString *error)
{
    QSqlQuery query(db);
    if (!query.exec(QStringLiteral("PRAGMA user_version")) || !query.next()) {
        if (error)
            *error = QStringLiteral("Не удалось прочитать версию схемы: %1")
                         .arg(query.lastError().text());
        return -1;
    }
    return query.value(0).toInt();
}

bool Migrations::apply(QSqlDatabase &db, QString *error)
{
    int version = currentVersion(db, error);
    if (version < 0)
        return false;

    for (const Migration &migration : kMigrations) {
        if (migration.version <= version)
            continue;
        if (!applyOne(db, migration, error))
            return false;
        version = migration.version;
    }
    return true;
}
