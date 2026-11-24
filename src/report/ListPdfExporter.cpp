#include "report/ListPdfExporter.h"

#include "report/ExportFileGuard.h"
#include "report/ListRenderer.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QPrinter>

bool ListPdfExporter::exportList(const QVector<PersonRecord> &records,
                                 const QString &filePath,
                                 QString *error)
{
    if (error)
        error->clear();

    const auto fail = [error](const QString &message) {
        if (error)
            *error = message;
        return false;
    };

    if (records.isEmpty())
        return fail(QStringLiteral("Нет записей для экспорта."));

    QString pathError;
    if (!ExportFileGuard::validateTarget(filePath, &pathError))
        return fail(pathError);

    // Пишем во временный файл рядом с целью: если печать сорвётся,
    // существующий PDF останется нетронутым (M7-05).
    const QString tempPath = filePath + QStringLiteral(".tmp");
    QFile::remove(tempPath);

    QString renderError;
    bool rendered = false;
    {
        QPrinter printer(QPrinter::HighResolution);
        printer.setOutputFileName(tempPath);
        printer.setOutputFormat(QPrinter::PdfFormat);
        rendered = ListRenderer::render(printer, records, &renderError);
    }

    if (!rendered) {
        QFile::remove(tempPath);
        return fail(renderError);
    }

    const QFileInfo saved(tempPath);
    if (!saved.exists() || saved.size() == 0) {
        QFile::remove(tempPath);
        return fail(QStringLiteral("Файл «%1» не создан или пуст.")
                        .arg(QDir::toNativeSeparators(filePath)));
    }

    QString replaceError;
    if (!ExportFileGuard::replaceFile(tempPath, filePath, &replaceError)) {
        QFile::remove(tempPath);
        return fail(replaceError);
    }

    return true;
}
