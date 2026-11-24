#include "core/AppPaths.h"

#include <QCoreApplication>
#include <QDir>
#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryFile>

namespace {

AppPaths::Mode g_mode = AppPaths::Mode::Portable;
QString g_root;

/// Проверка «можно ли писать в каталог» без побочных эффектов:
/// пробуем создать и сразу удалить временный файл.
bool isWritable(const QString &dir)
{
    QTemporaryFile probe(QDir(dir).filePath(QStringLiteral(".__write_test_XXXXXX")));
    if (!probe.open())
        return false;

    const bool ok = probe.exists();
    probe.close();   // QTemporaryFile удаляет файл сам
    return ok;
}

QString profileRoot()
{
    const QString base = QStandardPaths::writableLocation(
        QStandardPaths::GenericDataLocation);
    return QDir(base).filePath(QStringLiteral("Veterans"));
}

} // namespace

bool AppPaths::init(QString *errorMessage)
{
    const QString appDir = QCoreApplication::applicationDirPath();

    if (isWritable(appDir)) {
        g_mode = Mode::Portable;
        g_root = appDir;
    } else {
        g_mode = Mode::UserProfile;
        g_root = profileRoot();
    }

    return ensureDir(dataDir(), errorMessage)
        && ensureDir(logsDir(), errorMessage)
        && ensureDir(backupsDir(), errorMessage);
}

AppPaths::Mode AppPaths::mode()
{
    return g_mode;
}

bool AppPaths::isPortable()
{
    return g_mode == Mode::Portable;
}

QString AppPaths::rootDir()
{
    return g_root;
}

QString AppPaths::dataDir()
{
    return QDir(g_root).filePath(QStringLiteral("data"));
}

QString AppPaths::logsDir()
{
    return QDir(g_root).filePath(QStringLiteral("logs"));
}

QString AppPaths::backupsDir()
{
    return QDir(g_root).filePath(QStringLiteral("backups"));
}

QString AppPaths::settingsFile()
{
    return QDir(g_root).filePath(QStringLiteral("settings.ini"));
}

QString AppPaths::defaultDatabaseFile()
{
    return QDir(dataDir()).filePath(QStringLiteral("veterans.db"));
}

QString AppPaths::databaseFile()
{
    QSettings settings(settingsFile(), QSettings::IniFormat);
    const QString saved =
        settings.value(QStringLiteral("Database/path")).toString().trimmed();
    if (saved.isEmpty())
        return defaultDatabaseFile();
    return QDir::cleanPath(saved);
}

void AppPaths::setDatabaseFile(const QString &path)
{
    QSettings settings(settingsFile(), QSettings::IniFormat);
    const QString trimmed = path.trimmed();
    if (trimmed.isEmpty()
        || QDir::cleanPath(trimmed) == QDir::cleanPath(defaultDatabaseFile())) {
        settings.remove(QStringLiteral("Database/path"));
    } else {
        settings.setValue(QStringLiteral("Database/path"),
                          QDir::cleanPath(trimmed));
    }
    settings.sync();
}

bool AppPaths::ensureDir(const QString &path, QString *errorMessage)
{
    QDir dir;
    if (dir.mkpath(path))
        return true;

    if (errorMessage)
        *errorMessage = QCoreApplication::translate(
            "AppPaths", "Не удалось создать каталог: %1").arg(path);
    return false;
}
