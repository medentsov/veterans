#include "report/ExportFileGuard.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QStringList>

namespace {

bool fail(QString *error, const QString &message)
{
    if (error)
        *error = message;
    return false;
}

} // namespace

bool ExportFileGuard::validateTarget(const QString &filePath, QString *error)
{
    if (error)
        error->clear();

    if (filePath.trimmed().isEmpty())
        return fail(error, QStringLiteral("Не указан файл для экспорта."));

#ifdef Q_OS_WIN
    // Windows без longPathAware-манифеста не открывает пути длиннее MAX_PATH.
    const QString native = QDir::toNativeSeparators(filePath);
    if (native.size() >= 260 && !native.startsWith(QStringLiteral("\\\\?\\"))) {
        return fail(error,
                    QStringLiteral(
                        "Путь к файлу «%1» длиннее 260 символов. "
                        "Сократите путь или имя файла.")
                        .arg(native));
    }
#endif

    const QStringList parts = filePath.split(
        QRegularExpression(QStringLiteral("[/\\\\]")), Qt::SkipEmptyParts);
    for (const QString &part : parts) {
        if (part.size() > 255) {
            return fail(error,
                        QStringLiteral(
                            "Слишком длинное имя папки или файла: «%1» "
                            "(больше 255 символов).")
                            .arg(part.left(80) + QStringLiteral("…")));
        }
    }

    const QFileInfo target(filePath);
    const QDir dir = target.absoluteDir();
    if (!dir.exists()) {
        return fail(error, QStringLiteral("Каталог «%1» не существует.")
                               .arg(QDir::toNativeSeparators(
                                   dir.absolutePath())));
    }
    if (!QFileInfo(dir.absolutePath()).isWritable()) {
        return fail(error, QStringLiteral("Нет прав на запись в каталог «%1».")
                               .arg(QDir::toNativeSeparators(
                                   dir.absolutePath())));
    }
    if (target.exists() && target.isDir()) {
        return fail(error, QStringLiteral("«%1» — это каталог, а не файл.")
                               .arg(QDir::toNativeSeparators(filePath)));
    }
    if (target.exists() && !target.isWritable()) {
        return fail(error, QStringLiteral("Файл «%1» защищён от записи.")
                               .arg(QDir::toNativeSeparators(filePath)));
    }

    // На Windows открытый в Excel/просмотрщике файл нельзя перезаписать.
    // Пробное открытие ничего не меняет, но сразу даёт понятную ошибку.
    if (target.exists()) {
        QFile probe(filePath);
        if (!probe.open(QIODevice::ReadWrite)) {
            return fail(error,
                        QStringLiteral(
                            "Файл «%1» открыт в другой программе "
                            "или недоступен для записи. Закройте его и повторите.")
                            .arg(QDir::toNativeSeparators(filePath)));
        }
        probe.close();
    }

    return true;
}

bool ExportFileGuard::replaceFile(const QString &tempPath,
                                  const QString &targetPath,
                                  QString *error)
{
    if (error)
        error->clear();

    const QString backupPath = targetPath + QStringLiteral(".bak");
    QFile::remove(backupPath);

    const bool targetExists = QFileInfo::exists(targetPath);

    if (targetExists && !QFile::rename(targetPath, backupPath)) {
        return fail(error,
                    QStringLiteral(
                        "Не удалось заменить файл «%1»: возможно, он открыт "
                        "в другой программе.")
                        .arg(QDir::toNativeSeparators(targetPath)));
    }

    if (!QFile::rename(tempPath, targetPath)) {
        if (targetExists)
            QFile::rename(backupPath, targetPath);
        return fail(error, QStringLiteral("Не удалось записать файл «%1».")
                               .arg(QDir::toNativeSeparators(targetPath)));
    }

    if (targetExists)
        QFile::remove(backupPath);

    return true;
}
