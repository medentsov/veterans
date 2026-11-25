#include "data/Database.h"

#include "data/Migrations.h"

#include <QDir>
#include <QFileInfo>
#include <QSqlError>
#include <QSqlQuery>
#include <QStringList>
#include <QVariant>

#include <utility>

namespace {

const char *const kConnectionName = "veterans";

/// Журнал WAL + NORMAL: быстрый и достаточно надёжный режим для одного
/// пользователя. Важно: при открытой базе нельзя копировать только .db —
/// для бэкапа используется SQLite backup API (M2-05).
const char *const kPragmas[] = {
    "PRAGMA foreign_keys = ON",
    "PRAGMA journal_mode = WAL",
    "PRAGMA synchronous = NORMAL",
    "PRAGMA busy_timeout = 5000",
};

QString queryError(const QSqlQuery &query)
{
    return query.lastError().text().trimmed();
}

} // namespace

Database::OpenResult Database::open(const QString &path)
{
    OpenResult result;

    if (path.trimmed().isEmpty()) {
        result.error = QStringLiteral("Не указан файл базы данных.");
        return result;
    }

    // Закрываем возможное предыдущее соединение (смена файла в настройках).
    close();

    const QFileInfo info(path);
    result.created = !info.exists();

    if (!info.absoluteDir().exists()
        && !QDir().mkpath(info.absolutePath())) {
        result.error = QStringLiteral(
            "Не удалось создать каталог для базы данных:\n%1")
            .arg(info.absolutePath());
        return result;
    }

    {
        QSqlDatabase db = QSqlDatabase::addDatabase(
            QStringLiteral("QSQLITE"), QString::fromLatin1(kConnectionName));
        db.setDatabaseName(path);

        if (!db.open()) {
            result.error = QStringLiteral(
                "Не удалось открыть базу данных:\n%1\n\n%2")
                .arg(QDir::toNativeSeparators(path),
                     db.lastError().text().trimmed());
        } else if (!configure(db, &result.error)) {
            // сообщение уже заполнено
        } else if (!quickCheck(db, &result.error)) {
            // сообщение уже заполнено
        } else if (!Migrations::apply(db, &result.error)) {
            // сообщение уже заполнено
        } else {
            result.ok = true;
        }

        if (!result.ok)
            db.close();
    } // здесь дескриптор QSqlDatabase уничтожается — можно удалять соединение

    if (!result.ok)
        close();

    return result;
}

void Database::close()
{
    if (!QSqlDatabase::contains(QString::fromLatin1(kConnectionName)))
        return;

    {
        QSqlDatabase db = QSqlDatabase::database(
            QString::fromLatin1(kConnectionName), false);
        if (db.isOpen())
            db.close();
    } // дескриптор уничтожен до removeDatabase — без предупреждений Qt

    QSqlDatabase::removeDatabase(QString::fromLatin1(kConnectionName));
}

bool Database::isOpen()
{
    if (!QSqlDatabase::contains(QString::fromLatin1(kConnectionName)))
        return false;
    return QSqlDatabase::database(QString::fromLatin1(kConnectionName), false).isOpen();
}

QSqlDatabase Database::connection()
{
    if (!QSqlDatabase::contains(QString::fromLatin1(kConnectionName)))
        return {};
    return QSqlDatabase::database(QString::fromLatin1(kConnectionName), false);
}

bool Database::configure(QSqlDatabase &db, QString *error)
{
    for (const char *pragma : kPragmas) {
        QSqlQuery query(db);
        if (!query.exec(QString::fromLatin1(pragma))) {
            if (error) {
                *error = QStringLiteral("Не удалось настроить базу данных (%1):\n%2")
                             .arg(QString::fromLatin1(pragma), queryError(query));
            }
            return false;
        }
        query.finish();   // PRAGMA journal_mode возвращает строку
    }
    return true;
}

bool Database::quickCheck(QSqlDatabase &db, QString *error)
{
    QSqlQuery query(db);
    if (!query.exec(QStringLiteral("PRAGMA quick_check"))) {
        if (error) {
            *error = QStringLiteral("Не удалось проверить базу данных:\n%1")
                         .arg(queryError(query));
        }
        return false;
    }

    QStringList problems;
    while (query.next()) {
        const QString value = query.value(0).toString();
        if (value.compare(QStringLiteral("ok"), Qt::CaseInsensitive) != 0)
            problems.append(value);
    }

    if (!problems.isEmpty()) {
        if (error) {
            *error = QStringLiteral(
                "Файл базы данных повреждён:\n%1\n\n"
                "Восстановите его из резервной копии (каталог backups).")
                .arg(problems.join(QLatin1Char('\n')));
        }
        return false;
    }
    return true;
}
