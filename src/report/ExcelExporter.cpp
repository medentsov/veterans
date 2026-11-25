#include "report/ExcelExporter.h"

#include "report/ExportFileGuard.h"

#include "xlsxdocument.h"
#include "xlsxformat.h"
#include "xlsxworksheet.h"
#include "xlsxzipreader_p.h"
#include "xlsxzipwriter_p.h"

#include <QByteArray>
#include <QColor>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStringList>
#include <QVariant>
#include <QtGlobal>

namespace {

constexpr int kColumnCount = 7;
constexpr int kHeaderRow = 1;
constexpr int kFirstDataRow = 2;

QString dash()
{
    return QStringLiteral("—");
}

QString subscriptionText(const PersonRecord &record)
{
    if (record.subscriptionId == 0 || record.subscriptionName.isEmpty())
        return dash();
    if (record.subscriptionYear > 0) {
        return QStringLiteral("%1, %2")
            .arg(record.subscriptionName)
            .arg(record.subscriptionYear);
    }
    return record.subscriptionName;
}

/// QXlsx не умеет автофильтр и закрепление строки (в 1.5.1.1 таких методов
/// нет), поэтому после сохранения добавляем два элемента в XML листа.
/// Правка локальная: содержимое ячеек и стили уже записаны QXlsx.
bool injectWorksheetFeatures(const QString &filePath, int lastRow, QString *error)
{
    const QString sheetFile = QStringLiteral("xl/worksheets/sheet1.xml");
    const QString tempPath = filePath + QStringLiteral(".tmp");

    {
        QXlsx::ZipReader reader(filePath);
        if (!reader.exists()) {
            *error = QStringLiteral("Не удалось прочитать сохранённый файл «%1».")
                         .arg(QDir::toNativeSeparators(filePath));
            return false;
        }

        const QStringList files = reader.filePaths();
        if (!files.contains(sheetFile)) {
            *error = QStringLiteral("В файле «%1» не найден лист Excel.")
                         .arg(QDir::toNativeSeparators(filePath));
            return false;
        }

        QByteArray sheet = reader.fileData(sheetFile);
        if (sheet.isEmpty()) {
            *error = QStringLiteral("Лист Excel в файле «%1» пуст.")
                         .arg(QDir::toNativeSeparators(filePath));
            return false;
        }

        // Закрепляем первую строку: <pane> должен идти первым в <sheetView>.
        if (!sheet.contains("<pane ")) {
            // indexOf("<sheetView") попадает и в контейнер <sheetViews>, поэтому
            // ищем именно элемент <sheetView ...>.
            qsizetype viewStart = sheet.indexOf("<sheetView");
            // Пропускаем контейнер <sheetViews>: после имени элемента идёт «s».
            while (viewStart >= 0 && viewStart + 10 < sheet.size()
                   && sheet.at(viewStart + 10) == 's') {
                viewStart = sheet.indexOf("<sheetView", viewStart + 10);
            }
            if (viewStart < 0) {
                *error = QStringLiteral("В листе Excel не найден блок sheetView.");
                return false;
            }

            qsizetype tagEnd = sheet.indexOf('>', viewStart);
            if (tagEnd < 0) {
                *error = QStringLiteral("Повреждён блок sheetView в листе Excel.");
                return false;
            }

            const QByteArray pane =
                "<pane ySplit=\"1\" topLeftCell=\"A2\" activePane=\"bottomLeft\" "
                "state=\"frozen\"/>"
                "<selection pane=\"bottomLeft\" activeCell=\"A2\" sqref=\"A2\"/>";

            if (tagEnd > viewStart && sheet.at(tagEnd - 1) == '/') {
                // Самозакрывающийся <sheetView .../> превращаем в парный.
                sheet.remove(tagEnd - 1, 1);
                tagEnd -= 1;
                sheet.insert(tagEnd + 1, pane + "</sheetView>");
            } else {
                sheet.insert(tagEnd + 1, pane);
            }
        }

        // Автофильтр ставится сразу после sheetData — так требует схема OOXML.
        if (!sheet.contains("<autoFilter ")) {
            const QByteArray closeSheetData = "</sheetData>";
            qsizetype pos = sheet.indexOf(closeSheetData);
            if (pos < 0) {
                *error = QStringLiteral("В листе Excel не найден блок sheetData.");
                return false;
            }
            pos += closeSheetData.size();

            const QByteArray filter = "<autoFilter ref=\"A1:G"
                + QByteArray::number(qMax(1, lastRow)) + "\"/>";
            sheet.insert(pos, filter);
        }

        QFile::remove(tempPath);
        QXlsx::ZipWriter writer(tempPath);
        for (const QString &name : files) {
            const QByteArray data =
                name == sheetFile ? sheet : reader.fileData(name);
            writer.addFile(name, data);
        }
        writer.close();

        if (writer.error()) {
            QFile::remove(tempPath);
            *error = QStringLiteral(
                         "Не удалось обновить оформление файла «%1».")
                         .arg(QDir::toNativeSeparators(filePath));
            return false;
        }
    }

    // Подменяем файл только после успешной записи копии.
    QString replaceError;
    if (!ExportFileGuard::replaceFile(tempPath, filePath, &replaceError)) {
        QFile::remove(tempPath);
        *error = replaceError;
        return false;
    }

    return true;
}

} // namespace

bool ExcelExporter::exportList(const QVector<PersonRecord> &records,
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

    if (filePath.trimmed().isEmpty())
        return fail(QStringLiteral("Не указан файл для экспорта."));

    QString pathError;
    if (!ExportFileGuard::validateTarget(filePath, &pathError))
        return fail(pathError);

    QXlsx::Document xlsx;
    const QStringList sheetNames = xlsx.sheetNames();
    if (!sheetNames.isEmpty())
        xlsx.renameSheet(sheetNames.first(), QStringLiteral("Ветераны"));

    // Шапка: белый жирный текст на тёмно-синей заливке.
    QXlsx::Format headerFormat;
    headerFormat.setFontBold(true);
    headerFormat.setFontSize(10);
    headerFormat.setFontColor(Qt::white);
    headerFormat.setHorizontalAlignment(QXlsx::Format::AlignHCenter);
    headerFormat.setVerticalAlignment(QXlsx::Format::AlignVCenter);
    headerFormat.setTextWrap(true);
    headerFormat.setBorderStyle(QXlsx::Format::BorderThin);
    headerFormat.setBorderColor(QColor(0x1F, 0x38, 0x64));
    headerFormat.setFillPattern(QXlsx::Format::PatternSolid);
    headerFormat.setPatternBackgroundColor(QColor(0x30, 0x54, 0x96));

    // Тело таблицы: тонкие границы, числа по центру, текст слева.
    QXlsx::Format bodyBase;
    bodyBase.setFontSize(10);
    bodyBase.setVerticalAlignment(QXlsx::Format::AlignVCenter);
    bodyBase.setBorderStyle(QXlsx::Format::BorderThin);
    bodyBase.setBorderColor(QColor(0xD0, 0xD7, 0xE1));

    QXlsx::Format bodyLeft = bodyBase;
    bodyLeft.setHorizontalAlignment(QXlsx::Format::AlignLeft);

    QXlsx::Format bodyCenter = bodyBase;
    bodyCenter.setHorizontalAlignment(QXlsx::Format::AlignHCenter);

    QXlsx::Format zebraLeft = bodyLeft;
    zebraLeft.setFillPattern(QXlsx::Format::PatternSolid);
    zebraLeft.setPatternBackgroundColor(QColor(0xF2, 0xF5, 0xFA));

    QXlsx::Format zebraCenter = bodyCenter;
    zebraCenter.setFillPattern(QXlsx::Format::PatternSolid);
    zebraCenter.setPatternBackgroundColor(QColor(0xF2, 0xF5, 0xFA));

    const QStringList headers{
        QStringLiteral("№"),
        QStringLiteral("ФИО"),
        QStringLiteral("Микрорайон"),
        QStringLiteral("Год рождения"),
        QStringLiteral("Льготная категория"),
        QStringLiteral("Год посещения"),
        QStringLiteral("Подписка на газету"),
    };
    for (int column = 0; column < headers.size(); ++column)
        xlsx.write(kHeaderRow, column + 1, headers.at(column), headerFormat);

    xlsx.setRowHeight(kHeaderRow, 30);

    int row = kFirstDataRow;
    for (const PersonRecord &record : records) {
        const bool zebra = (row % 2) == 0;
        const QXlsx::Format &left = zebra ? zebraLeft : bodyLeft;
        const QXlsx::Format &center = zebra ? zebraCenter : bodyCenter;

        xlsx.write(row, 1, row - kHeaderRow, center);
        xlsx.write(row, 2, record.fullName(), left);
        xlsx.write(row, 3, record.microdistrict, center);

        if (record.birthDate.isValid())
            xlsx.write(row, 4, record.birthDate.year(), center);
        else
            xlsx.write(row, 4, dash(), center);

        xlsx.write(row, 5,
                   record.categoryName.isEmpty() ? dash() : record.categoryName,
                   left);

        if (record.visitYear > 0)
            xlsx.write(row, 6, record.visitYear, center);
        else
            xlsx.write(row, 6, dash(), center);

        xlsx.write(row, 7, subscriptionText(record), left);
        ++row;
    }

    // Ширины в символах Excel; подобраны под печатный макет (PRINT-LAYOUT.md).
    const double widths[kColumnCount] = {6.0, 42.0, 13.0, 14.0, 30.0, 14.0, 26.0};
    for (int column = 0; column < kColumnCount; ++column)
        xlsx.setColumnWidth(column + 1, widths[column]);

    if (!xlsx.saveAs(filePath)) {
        return fail(QStringLiteral(
                        "Не удалось сохранить файл «%1». "
                        "Проверьте, что он не открыт в Excel и доступен для записи.")
                        .arg(QDir::toNativeSeparators(filePath)));
    }

    const QFileInfo saved(filePath);
    if (!saved.exists() || saved.size() == 0) {
        return fail(QStringLiteral("Файл «%1» не создан или пуст.")
                        .arg(QDir::toNativeSeparators(filePath)));
    }

    QString patchError;
    if (!injectWorksheetFeatures(filePath, row - 1, &patchError)) {
        if (error)
            *error = patchError;
        return false;
    }

    return true;
}
