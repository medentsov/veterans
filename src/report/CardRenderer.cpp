#include "report/CardRenderer.h"

#include "core/PhoneNumber.h"

#include <QMarginsF>
#include <QPageLayout>
#include <QPageSize>
#include <QPainter>
#include <QPrinter>
#include <QStringList>
#include <QTextDocument>
#include <QTextStream>

namespace {

QString dash()
{
    return QStringLiteral("—");
}

/// В карточке печатается только месяц и год, поэтому нужен именительный
/// падеж: «май 2026», а не «мая 2026» (родительный — для полной даты).
/// Так же пример в `docs/PRINT-LAYOUT.md` (M12-07: падеж и толщина линий
/// приведены к одному виду в макете, чекере и коде).
QString monthNameNominative(int month)
{
    static const char *const names[] = {
        "январь", "февраль", "март", "апрель", "май", "июнь",
        "июль", "август", "сентябрь", "октябрь", "ноябрь", "декабрь",
    };
    if (month < 1 || month > 12)
        return {};
    return QString::fromUtf8(names[month - 1]);
}

QString addressText(const PersonRecord &record)
{
    QStringList parts;
    if (!record.street.isEmpty())
        parts << record.street;
    if (!record.house.isEmpty())
        parts << QStringLiteral("д. %1").arg(record.house);
    if (!record.building.isEmpty())
        parts << QStringLiteral("корп. %1").arg(record.building);
    if (!record.apartment.isEmpty())
        parts << QStringLiteral("кв. %1").arg(record.apartment);
    return parts.isEmpty() ? dash() : parts.join(QStringLiteral(", "));
}

QString visitDateText(const PersonRecord &record)
{
    if (record.visitYear <= 0)
        return dash();

    if (record.visitMonth >= 1 && record.visitMonth <= 12) {
        return QStringLiteral("%1 %2")
            .arg(monthNameNominative(record.visitMonth))
            .arg(record.visitYear);
    }
    return QString::number(record.visitYear);
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

QString phoneText(const PersonRecord &record)
{
    const PhoneType type = record.phoneType == 1 ? PhoneType::Mobile
                                                 : PhoneType::Home;
    const QString number = PhoneNumber::format(type, record.phone);
    if (number.isEmpty())
        return {};

    return QStringLiteral("%1 (%2)")
        .arg(number, PhoneNumber::typeName(type));
}

QString birthDateText(const PersonRecord &record)
{
    return record.birthDate.isValid()
        ? record.birthDate.toString(QStringLiteral("dd.MM.yyyy"))
        : dash();
}

} // namespace

QString CardRenderer::toHtml(const PersonRecord &record,
                             int cardNumber,
                             const QDate &formedDate,
                             const QString &organizationName)
{
    const QString organization = organizationName.trimmed().isEmpty()
        ? QStringLiteral("Госпиталь ветеранов")
        : organizationName.trimmed();

    const QString fullName = record.fullName().isEmpty()
        ? dash()
        : record.fullName().toUpper();

    const QString number = QString::number(cardNumber)
                               .rightJustified(6, QLatin1Char('0'));
    const QString microdistrict = record.microdistrict > 0
        ? QString::number(record.microdistrict)
        : dash();
    const QString phone = phoneText(record);

    QString html;
    QTextStream out(&html);

    out << QStringLiteral(
        "<!DOCTYPE html><html><head><meta charset=\"utf-8\"/>"
        "<style type=\"text/css\">"
        "body { color:#000000; font-family:\"Noto Sans\", sans-serif; "
        "font-size:10pt; }"
        "p { margin:0; }"
        ".hospital { font-size:14pt; font-weight:bold; text-align:center; }"
        ".org { font-size:9pt; text-align:center; margin-top:8px; }"
        ".heading { font-size:12pt; font-weight:bold; }"
        ".number { font-size:10pt; text-align:right; }"
        ".label { font-size:7.5pt; margin-top:23px; }"
        ".value { font-size:11pt; margin-top:4px; }"
        ".row { font-size:10pt; margin-top:23px; }"
        ".fio-label { font-size:7.5pt; }"
        ".fio { font-size:15pt; font-weight:bold; margin-top:8px; }"
        ".footer { font-size:8pt; margin-top:45px; }"
        "</style></head><body>"
        "<div style=\"page-break-inside: avoid;\">"
        "<p class=\"hospital\">ГОСПИТАЛЬ ВЕТЕРАНОВ</p>");

    out << QStringLiteral("<p class=\"org\">")
        << organization.toHtmlEscaped()
        << QStringLiteral("</p>");

    // QTextDocument рисует рамки таблиц только в px: значения «0.75pt» и «0.5pt»
    // молча отбрасываются (проверено отрисовкой в QImage — линия не появляется),
    // поэтому в HTML везде px. 1 px при 96 dpi = 0.2646 мм = 0.75 pt, то есть
    // линия совпадает с макетом, а рамка 0.5 pt печатается минимальной — 1 px.
    out << QStringLiteral(
        "<table width=\"100%\" cellspacing=\"0\" cellpadding=\"0\" "
        "style=\"margin-top:38px;\"><tr>"
        "<td style=\"border-bottom:1px solid #000000;\"></td>"
        "</tr></table>");

    out << QStringLiteral(
        "<table width=\"100%\" cellspacing=\"0\" cellpadding=\"0\" "
        "style=\"margin-top:23px;\"><tr>"
        "<td class=\"heading\">КАРТОЧКА ВЕТЕРАНА</td>"
        "<td class=\"number\">№ ")
        << number
        << QStringLiteral("</td></tr></table>");

    out << QStringLiteral(
        "<table width=\"100%\" cellspacing=\"0\" cellpadding=\"8\" "
        "style=\"margin-top:30px; border:1px solid #000000;\"><tr><td>"
        "<p class=\"fio-label\">ФИО</p>"
        "<p class=\"fio\">")
        << fullName.toHtmlEscaped()
        << QStringLiteral("</p></td></tr></table>");

    out << QStringLiteral(
        "<table width=\"100%\" cellspacing=\"0\" cellpadding=\"0\" "
        "style=\"margin-top:23px;\"><tr>"
        "<td class=\"row\">Дата рождения: <b>")
        << birthDateText(record).toHtmlEscaped()
        << QStringLiteral("</b></td><td class=\"row\">Микрорайон: <b>")
        << microdistrict.toHtmlEscaped()
        << QStringLiteral("</b></td></tr></table>");

    out << QStringLiteral("<p class=\"label\">АДРЕС ПРОПИСКИ:</p>")
        << QStringLiteral("<p class=\"value\">")
        << addressText(record).toHtmlEscaped()
        << QStringLiteral("</p>");

    if (!phone.isEmpty()) {
        out << QStringLiteral("<p class=\"value\" style=\"margin-top:23px;\">"
                              "Телефон: <b>")
            << phone.toHtmlEscaped()
            << QStringLiteral("</b></p>");
    }

    out << QStringLiteral("<p class=\"label\">ЛЬГОТНАЯ КАТЕГОРИЯ:</p>")
        << QStringLiteral("<p class=\"value\">")
        << (record.categoryName.isEmpty() ? dash()
                                          : record.categoryName)
               .toHtmlEscaped()
        << QStringLiteral("</p>");

    out << QStringLiteral(
        "<p class=\"label\">ДАТА ПОСЕЩЕНИЯ ГОСПИТАЛЯ:</p>")
        << QStringLiteral("<p class=\"value\">")
        << visitDateText(record).toHtmlEscaped()
        << QStringLiteral("</p>");

    out << QStringLiteral("<p class=\"label\">ПОДПИСКА НА ГАЗЕТУ:</p>")
        << QStringLiteral("<p class=\"value\">")
        << subscriptionText(record).toHtmlEscaped()
        << QStringLiteral("</p>");

    out << QStringLiteral(
        "<table width=\"100%\" cellspacing=\"0\" cellpadding=\"0\" "
        "style=\"margin-top:45px;\"><tr>"
        "<td style=\"border-top:1px solid #000000;\"></td>"
        "</tr></table>")
        << QStringLiteral(
            "<table width=\"100%\" cellspacing=\"0\" cellpadding=\"0\" "
            "style=\"margin-top:11px;\"><tr>"
            "<td class=\"footer\">Дата формирования: ")
        << formedDate.toString(QStringLiteral("dd.MM.yyyy")).toHtmlEscaped()
        << QStringLiteral("</td>"
                          "<td class=\"footer\" align=\"right\">"
                          "Подпись: ___________</td>"
                          "</tr></table>");

    out << QStringLiteral("</div></body></html>");

    return html;
}

bool CardRenderer::render(QPrinter &printer,
                          const PersonRecord &record,
                          int cardNumber,
                          const QDate &formedDate,
                          const QString &organizationName,
                          QString *error)
{
    if (error)
        error->clear();

    const QPageLayout layout(
        QPageSize(QPageSize::A4), QPageLayout::Portrait,
        QMarginsF(20.0, 15.0, 15.0, 15.0),
        QPageLayout::Millimeter);
    if (!layout.isValid()) {
        if (error)
            *error = QStringLiteral("Не удалось настроить книжный лист A4.");
        return false;
    }

    printer.setPageLayout(layout);
    // setFullPage(false): НЕ включаем «полный лист». У сетевого принтера есть
    // несъёмные поля (у HP LJ 5100 — 4,2 мм слева, 4,7 мм справа), и печать
    // «от края листа» уходила за печатаемую область: справа обрезало номер
    // карточки («№0» вместо «№ 000014»).
    //
    // Геометрию берём у устройства: после setPageLayout() драйвер отдаёт
    // область «лист минус наши поля» (paintRect). Если движок отдаёт полный
    // лист — пересчитываем от paintRect.
    printer.setFullPage(false);

    const QRectF paintPx =
        printer.pageLayout().paintRectPixels(printer.resolution());
    const int deviceWidthPx = printer.width();
    const int deviceHeightPx = printer.height();
    if (deviceWidthPx <= 0 || deviceHeightPx <= 0
        || paintPx.width() <= 0 || paintPx.height() <= 0) {
        if (error)
            *error = QStringLiteral("Не удалось определить печатаемую область.");
        return false;
    }

    const bool deviceIsPaintRect =
        qAbs(deviceWidthPx - paintPx.width()) <= 2
        && qAbs(deviceHeightPx - paintPx.height()) <= 2;

    const qreal contentWidthPx = deviceIsPaintRect ? deviceWidthPx : paintPx.width();
    const qreal contentHeightPx = deviceIsPaintRect ? deviceHeightPx : paintPx.height();
    const qreal offsetXPx = deviceIsPaintRect ? 0.0 : paintPx.x();
    const qreal offsetYPx = deviceIsPaintRect ? 0.0 : paintPx.y();
    if (contentWidthPx <= 0 || contentHeightPx <= 0) {
        if (error)
            *error = QStringLiteral("Не удалось определить размеры страницы.");
        return false;
    }

    // Документ верстается в логических 96 dpi: тогда px в HTML — это 1/96″
    // на любом принтере, и вёрстка предпросмотра совпадает с печатью.
    const qreal logicalPxPerMm = 96.0 / 25.4;
    const qreal pxPerMm = printer.logicalDpiX() / 25.4;
    if (pxPerMm <= 0) {
        if (error)
            *error = QStringLiteral("Не удалось определить разрешение печати.");
        return false;
    }

    // Масштаб логических 96 dpi -> пиксели устройства.
    const qreal deviceScale = pxPerMm / logicalPxPerMm;

    // Область контента в логических px документа.
    const qreal logicalContentWidthPx  = contentWidthPx  / deviceScale;
    const qreal logicalContentHeightPx = contentHeightPx / deviceScale;

    QTextDocument document;
    document.setDocumentMargin(0);
    document.setHtml(toHtml(record, cardNumber, formedDate, organizationName));
    document.setTextWidth(logicalContentWidthPx);
    document.setPageSize(QSizeF(logicalContentWidthPx, logicalContentHeightPx));

    // QTextDocument::print() не используется: он сам нумерует страницы в правом
    // нижнем углу, а макет требует «нумерация не нужна», и он же добавляет
    // вторые поля — контент уезжал на 40 мм вместо 20 мм от края. Поэтому
    // страницы рисуются вручную в область печати, как в ListRenderer.
    QPainter painter;
    if (!painter.begin(&printer)) {
        if (error)
            *error = QStringLiteral("Не удалось начать печать карточки.");
        return false;
    }

    const int pageCount = qMax(1, document.pageCount());
    for (int page = 0; page < pageCount; ++page) {
        if (page > 0)
            printer.newPage();
        painter.save();
        painter.scale(deviceScale, deviceScale);
        painter.translate(offsetXPx / deviceScale,
                          offsetYPx / deviceScale
                              - page * logicalContentHeightPx);
        document.drawContents(
            &painter,
            QRectF(0.0, page * logicalContentHeightPx,
                   logicalContentWidthPx, logicalContentHeightPx));
        painter.restore();
    }
    painter.end();

    return true;
}
