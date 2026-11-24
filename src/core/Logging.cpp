#include "core/Logging.h"

#include "core/AppPaths.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMutex>
#include <QMutexLocker>
#include <QtGlobal>

#include <cstdio>

namespace {

constexpr qint64 kMaxBytes = 2 * 1024 * 1024; // 2 МБ
constexpr int kKeepFiles = 5;                 // app.log.1 … app.log.5

QMutex g_mutex;
QFile g_file;
QString g_path;
QtMessageHandler g_previous = nullptr;

QString levelName(QtMsgType type)
{
    switch (type) {
    case QtDebugMsg:    return QStringLiteral("DEBUG");
    case QtInfoMsg:     return QStringLiteral("INFO");
    case QtWarningMsg:  return QStringLiteral("WARNING");
    case QtCriticalMsg: return QStringLiteral("CRITICAL");
    case QtFatalMsg:    return QStringLiteral("FATAL");
    }
    return QStringLiteral("UNKNOWN");
}

void openLocked()
{
    g_file.setFileName(g_path);
    g_file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text);
}

/// Сдвиг архивов: app.log → app.log.1 → … → app.log.5 (старый удаляется).
void rotateLocked()
{
    if (!g_file.isOpen() || g_file.size() < kMaxBytes)
        return;

    g_file.close();

    QFile::remove(g_path + QStringLiteral(".%1").arg(kKeepFiles));
    for (int i = kKeepFiles - 1; i >= 1; --i) {
        const QString from = g_path + QStringLiteral(".%1").arg(i);
        const QString to = g_path + QStringLiteral(".%1").arg(i + 1);
        if (QFile::exists(to))
            QFile::remove(to);
        if (QFile::exists(from))
            QFile::rename(from, to);
    }
    if (QFile::exists(g_path))
        QFile::rename(g_path, g_path + QStringLiteral(".1"));

    openLocked();
}

void messageHandler(QtMsgType type,
                    const QMessageLogContext &context,
                    const QString &message)
{
    QString line = QStringLiteral("%1 [%2] %3")
                       .arg(QDateTime::currentDateTime().toString(
                                QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz")),
                            levelName(type),
                            message);

    if (context.file) {
        line += QStringLiteral(" (%1:%2)")
                    .arg(QFileInfo(QString::fromUtf8(context.file)).fileName())
                    .arg(context.line);
    }
    line += QLatin1Char('\n');

    {
        QMutexLocker lock(&g_mutex);
        rotateLocked();
        if (g_file.isOpen()) {
            g_file.write(line.toUtf8());
            g_file.flush();   // важно для qFatal: запись должна попасть на диск
        }
    }

    // Дублируем в консоль (при запуске из терминала) или штатному обработчику.
    if (g_previous)
        g_previous(type, context, message);
    else
        std::fputs(line.toLocal8Bit().constData(), stderr);
}

} // namespace

void Logging::init()
{
    g_path = QDir(AppPaths::logsDir()).filePath(QStringLiteral("app.log"));

    QMutexLocker lock(&g_mutex);
    openLocked();
    rotateLocked();
    lock.unlock();

    g_previous = qInstallMessageHandler(messageHandler);
}

QString Logging::logFilePath()
{
    return g_path;
}
