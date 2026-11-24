#include "report/PrintService.h"

#include "core/AppPaths.h"
#include "report/CardRenderer.h"
#include "report/ListRenderer.h"

#include <QDate>
#include <QMarginsF>
#include <QMessageBox>
#include <QObject>
#include <QPageLayout>
#include <QPageSize>
#include <QPrintPreviewDialog>
#include <QPrinter>
#include <QSettings>

namespace {

void showPrintError(QWidget *parent, const QString &error)
{
    QMessageBox::warning(
        parent, QObject::tr("Не удалось напечатать"),
        error.isEmpty() ? QObject::tr("Неизвестная ошибка печати.") : error);
}

QString organizationName()
{
    QSettings settings(AppPaths::settingsFile(), QSettings::IniFormat);
    return settings.value(QStringLiteral("Print/organizationName")).toString();
}

} // namespace

void PrintService::printList(QWidget *parent,
                             const QVector<PersonRecord> &records)
{
    if (records.isEmpty()) {
        QMessageBox::information(parent, QObject::tr("Печать"),
                                 QObject::tr("Нет записей для печати."));
        return;
    }

    QPrinter printer(QPrinter::HighResolution);
    printer.setDocName(QObject::tr("Список ветеранов"));
    printer.setPageLayout(QPageLayout(
        QPageSize(QPageSize::A4), QPageLayout::Landscape,
        QMarginsF(15.0, 15.0, 15.0, 15.0), QPageLayout::Millimeter));

    QPrintPreviewDialog preview(&printer, parent);
    preview.setWindowTitle(QObject::tr("Предпросмотр списка"));
    preview.resize(1000, 700);

    bool errorShown = false;
    const auto reportError = [&](const QString &error) {
        if (errorShown)
            return;
        errorShown = true;
        showPrintError(parent, error);
    };

    // Кнопка «Печать» в QPrintPreviewDialog открывает QPrintDialog:
    // принтер, копии, ориентация и поля.
    QObject::connect(&preview, &QPrintPreviewDialog::paintRequested,
                     &preview, [&](QPrinter *p) {
        QString error;
        if (!ListRenderer::render(*p, records, &error))
            reportError(error);
    });

    preview.exec();
}

void PrintService::printCards(QWidget *parent,
                              const QVector<PersonRecord> &records)
{
    if (records.isEmpty()) {
        QMessageBox::information(parent, QObject::tr("Печать"),
                                 QObject::tr("Нет записей для печати."));
        return;
    }

    QPrinter printer(QPrinter::HighResolution);
    printer.setDocName(QObject::tr("Карточки ветеранов"));
    printer.setPageLayout(QPageLayout(
        QPageSize(QPageSize::A4), QPageLayout::Portrait,
        QMarginsF(20.0, 15.0, 15.0, 15.0), QPageLayout::Millimeter));

    QPrintPreviewDialog preview(&printer, parent);
    preview.setWindowTitle(QObject::tr("Предпросмотр карточек"));
    preview.resize(1000, 700);

    bool errorShown = false;
    const auto reportError = [&](const QString &error) {
        if (errorShown)
            return;
        errorShown = true;
        showPrintError(parent, error);
    };

    const QDate formedDate = QDate::currentDate();
    const QString organization = organizationName();

    // Кнопка «Печать» в QPrintPreviewDialog открывает QPrintDialog:
    // принтер, копии, ориентация и поля.
    QObject::connect(&preview, &QPrintPreviewDialog::paintRequested,
                     &preview, [&](QPrinter *p) {
        bool firstPage = true;
        for (const PersonRecord &record : records) {
            if (!firstPage && !p->newPage()) {
                reportError(QObject::tr(
                    "Не удалось создать новую страницу для карточки."));
                return;
            }
            firstPage = false;

            QString error;
            if (!CardRenderer::render(*p, record, record.id, formedDate,
                                      organization, &error)) {
                reportError(error);
                return;
            }
        }
    });

    preview.exec();
}
