#include "data/BackupService.h"

#include "core/AppPaths.h"
#include "data/Database.h"

#include <QDate>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

#include <algorithm>
#include <functional>

namespace {

const char *const kPrefix = "veterans-";

bool fail(QString *error, const QString &text)
{
    if (error)
        *error = text;
    qWarning().noquote() << "[BackupService]" << text;
    return false;
}

QString quoted(const QString &path)
{
    QString escaped = path;
    escaped.replace(QLatin1Char('\''), QStringLiteral("''"));
    return QLatin1Char('\'') + escaped + QLatin1Char('\'');
}

/// Проверка копии на целостность отдельным соединением.
bool validateSnapshot(const QString &path, QString *error)
{
    const QString connection = QStringLiteral("veterans-backup-check");

    bool ok = false;
    QString problem;
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(
            QStringLiteral("QSQLITE"), connection);
        db.setDatabaseName(path);
        if (!db.open()) {
            problem = db.lastError().text().trimmed();
        } else {
            QSqlQuery query(db);
            if (!query.exec(QStringLiteral("PRAGMA quick_check"))) {
                problem = query.lastError().text().trimmed();
            } else {
                ok = true;
                while (query.next()) {
                    if (query.value(0).toString().compare(
                            QStringLiteral("ok"), Qt::CaseInsensitive) != 0) {
                        ok = false;
                        problem = query.value(0).toString();
                        break;
                    }
                }
            }
            db.close();
        }
    }
    QSqlDatabase::removeDatabase(connection);

    if (!ok) {
        return fail(error, QStringLiteral("Копия повреждена (%1): %2")
                               .arg(QDir::toNativeSeparators(path), problem));
    }
    return true;
}

} // namespace

QString BackupService::makeFileName(const QString &stamp)
{
    return QDir(AppPaths::backupsDir())
        .filePath(QString::fromLatin1(kPrefix) + stamp + QStringLiteral(".db"));
}

bool BackupService::createDaily(QString *error)
{
    const QString today = QDate::currentDate().toString(QStringLiteral("yyyyMMdd"));
    const QString prefix = QString::fromLatin1(kPrefix) + today;

    const QStringList files = QDir(AppPaths::backupsDir())
                                  .entryList({prefix + QStringLiteral("*.db")},
                                             QDir::Files, QDir::Name);
    if (!files.isEmpty())
        return true;   // сегодня уже есть

    return createNow(error);
}

bool BackupService::createNow(QString *error)
{
    if (!Database::isOpen())
        return fail(error, QStringLiteral("База данных не открыта — копию сделать нельзя."));

    if (!QDir().mkpath(AppPaths::backupsDir())) {
        return fail(error, QStringLiteral("Не удалось создать каталог резервных копий: %1")
                               .arg(QDir::toNativeSeparators(AppPaths::backupsDir())));
    }

    const QString stamp =
        QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss"));
    const QString target = makeFileName(stamp);

    QSqlQuery query(Database::connection());
    if (!query.exec(QStringLiteral("VACUUM INTO %1").arg(quoted(target)))) {
        return fail(error, QStringLiteral("Не удалось создать резервную копию: %1")
                               .arg(query.lastError().text().trimmed()));
    }
    query.finish();

    prune();
    qInfo().noquote() << "[BackupService] копия создана:"
                      << QDir::toNativeSeparators(target);
    return true;
}

QStringList BackupService::list()
{
    QDir dir(AppPaths::backupsDir());
    QStringList files = dir.entryList({QString::fromLatin1(kPrefix)
                                       + QStringLiteral("*.db")},
                                      QDir::Files, QDir::Name);
    // Имена содержат дату-время, поэтому достаточно отсортировать по убыванию.
    std::sort(files.begin(), files.end(), std::greater<QString>());

    QStringList paths;
    paths.reserve(files.size());
    for (const QString &name : files)
        paths.append(dir.filePath(name));
    return paths;
}

bool BackupService::restore(const QString &backupPath,
                            const QString &targetDatabasePath,
                            QString *error)
{
    if (Database::isOpen()) {
        return fail(error, QStringLiteral(
            "Перед восстановлением закройте базу данных."));
    }
    if (!QFileInfo::exists(backupPath)) {
        return fail(error, QStringLiteral("Файл копии не найден: %1")
                               .arg(QDir::toNativeSeparators(backupPath)));
    }
    if (!validateSnapshot(backupPath, error))
        return false;

    const QFileInfo target(targetDatabasePath);
    if (!target.absoluteDir().exists()
        && !QDir().mkpath(target.absolutePath())) {
        return fail(error, QStringLiteral("Не удалось создать каталог для базы: %1")
                               .arg(QDir::toNativeSeparators(target.absolutePath())));
    }

    // Страховка: сохраняем текущую базу, чтобы восстановление можно было отменить.
    if (target.exists()) {
        const QString safety = makeFileName(
            QStringLiteral("before-restore-")
            + QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss")));
        if (!QFile::copy(targetDatabasePath, safety)) {
            return fail(error, QStringLiteral(
                "Не удалось сохранить текущую базу перед восстановлением."));
        }
    }

    // Убираем основной файл и остатки WAL/SHM.
    QFile::remove(targetDatabasePath);
    QFile::remove(targetDatabasePath + QStringLiteral("-wal"));
    QFile::remove(targetDatabasePath + QStringLiteral("-shm"));

    if (!QFile::copy(backupPath, targetDatabasePath)) {
        return fail(error, QStringLiteral("Не удалось скопировать копию на место базы."));
    }

    qInfo().noquote() << "[BackupService] база восстановлена из"
                      << QDir::toNativeSeparators(backupPath);
    return true;
}

void BackupService::prune()
{
    QStringList files = list();
    while (files.size() > kKeepFiles) {
        const QString oldest = files.takeLast();
        if (!QFile::remove(oldest))
            qWarning().noquote() << "[BackupService] не удалось удалить старую копию:"
                                 << oldest;
    }
}
