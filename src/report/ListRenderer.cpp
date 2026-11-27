#include "report/ListRenderer.h"

#include <QColor>
#include <QDate>
#include <QFont>
#include <QFontMetricsF>
#include <QMarginsF>
#include <QPageLayout>
#include <QPageSize>
#include <QPainter>
#include <QPen>
#include <QPrinter>
#include <QStringList>
#include <QtGlobal>

namespace {

constexpr int kColumnCount = 7;
constexpr qreal kTotalWidthMm = 267.0;
constexpr qreal kMinRowHeightMm = 7.0;
constexpr qreal kMinHeaderHeightMm = 8.0;
constexpr qreal kCellPaddingXMm = 1.5;
constexpr qreal kCellPaddingYMm = 1.0;
constexpr qreal kMarginMm = 15.0;

/// Ширины колонок из PRINT-LAYOUT.md, раздел 3. Сумма — 267 мм.
constexpr qreal kColumnWidthsMm[kColumnCount] = {
    12.0,   // №
    75.0,   // ФИО
    24.0,   // Микрорайон
    22.0,   // Год рождения
    58.0,   // Льготная категория
    26.0,   // Год посещения
    50.0,   // Подписка на газету
};

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

QString birthYearText(const PersonRecord &record)
{
    return record.birthDate.isValid() ? QString::number(record.birthDate.year())
                                      : dash();
}

QString microdistrictText(const PersonRecord &record)
{
    return record.microdistrict > 0 ? QString::number(record.microdistrict)
                                    : dash();
}

QVector<QString> rowTexts(const PersonRecord &record, int number)
{
    return {
        QString::number(number),
        record.fullName().isEmpty() ? dash() : record.fullName(),
        microdistrictText(record),
        birthYearText(record),
        record.categoryName.isEmpty() ? dash() : record.categoryName,
        record.visitYear > 0 ? QString::number(record.visitYear) : dash(),
        subscriptionText(record),
    };
}

Qt::Alignment columnAlignment(int column)
{
    switch (column) {
    case 0:   // №
    case 2:   // Микрорайон
    case 3:   // Год рождения
    case 5:   // Год посещения
        return Qt::AlignHCenter;
    default:
        return Qt::AlignLeft;
    }
}

/// Есть ли в тексте слово шире ячейки: только тогда разрешаем перенос
/// по буквам, иначе Qt рвёт обычные слова посреди («Год ро/жд.»).
bool needsAnywhereBreak(const QString &text,
                        const QFontMetricsF &metrics,
                        qreal available)
{
    const QStringList words = text.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    for (const QString &word : words) {
        if (metrics.horizontalAdvance(word) > available)
            return true;
    }
    return false;
}

/// Высота строки: текст переносится по словам внутри ячейки, поэтому длинное
/// ФИО/категория увеличивают строку, но она остаётся целой на одной странице.
qreal measureRowHeightPx(const QVector<QString> &texts,
                         const QFontMetricsF &metrics,
                         const qreal *widthsPx,
                         qreal pxPerMm,
                         qreal minHeightMm)
{
    const qreal padX = kCellPaddingXMm * pxPerMm;
    const qreal padY = kCellPaddingYMm * pxPerMm;

    qreal maxHeight = minHeightMm * pxPerMm;
    for (int column = 0; column < kColumnCount; ++column) {
        const qreal available = qMax<qreal>(1.0, widthsPx[column] - 2.0 * padX);
        const QString text = texts.value(column);
        int flags = int(Qt::AlignLeft) | int(Qt::TextWordWrap);
        if (needsAnywhereBreak(text, metrics, available))
            flags |= int(Qt::TextWrapAnywhere);
        const QRectF bounds = metrics.boundingRect(
            QRectF(0, 0, available, 100000.0), flags, text);
        const qreal height = bounds.height() + 2.0 * padY;
        if (height > maxHeight)
            maxHeight = height;
    }

    return maxHeight;
}

void drawTableRow(QPainter &painter,
                  const QVector<QString> &texts,
                  const QFont &font,
                  qreal topPx,
                  qreal heightPx,
                  const qreal *widthsPx,
                  qreal totalWidthPx,
                  qreal pxPerMm,
                  bool header)
{
    if (header) {
        // Заливка шапки 5 % (светло-серая), как в макете.
        painter.fillRect(QRectF(0, topPx, totalWidthPx, heightPx),
                         QColor(0xF2, 0xF2, 0xF2));
    }

    QPen pen(Qt::black);
    pen.setWidthF(qMax(1.0, 0.15 * pxPerMm));
    painter.setPen(pen);
    painter.setFont(font);

    const qreal padX = kCellPaddingXMm * pxPerMm;
    const qreal padY = kCellPaddingYMm * pxPerMm;
    const QFontMetricsF metrics(font, painter.device());

    qreal x = 0;
    for (int column = 0; column < kColumnCount; ++column) {
        const QRectF cell(x, topPx, widthsPx[column], heightPx);
        painter.drawRect(cell);

        const QRectF textRect =
            cell.adjusted(padX, padY, -padX, -padY);
        const QString text = texts.value(column);
        int flags = int(columnAlignment(column))
            | int(Qt::TextWordWrap) | int(Qt::AlignVCenter);
        if (needsAnywhereBreak(text, metrics, textRect.width()))
            flags |= int(Qt::TextWrapAnywhere);
        painter.drawText(textRect, flags, text);

        x += widthsPx[column];
    }
}

} // namespace

bool ListRenderer::render(QPrinter &printer,
                          const QVector<PersonRecord> &records,
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

    const QPageLayout layout(
        QPageSize(QPageSize::A4), QPageLayout::Landscape,
        QMarginsF(kMarginMm, kMarginMm, kMarginMm, kMarginMm),
        QPageLayout::Millimeter);
    if (!layout.isValid())
        return fail(QStringLiteral("Не удалось настроить альбомный лист A4."));
    printer.setPageLayout(layout);
    // setFullPage(false): НЕ включаем «полный лист». У сетевого принтера есть
    // несъёмные поля (у HP LJ 5100 — 4,2 мм слева и 4,7 мм справа), поэтому
    // печать «от края листа» уходила за границу печатаемой области: справа
    // обрезало «Записей: 2» до «Записе», а подвал «Стр. X из Y» не печатался.
    //
    // Геометрию берём у самого устройства. После setPageLayout() драйвер уже
    // отдаёт область, равную листу минус наши поля (paintRect). Если движок
    // (например, PDF) отдаёт полный лист, пересчитываем от paintRect.
    printer.setFullPage(false);

    const QRectF paintPx =
        printer.pageLayout().paintRectPixels(printer.resolution());
    const int deviceWidthPx = printer.width();
    const int deviceHeightPx = printer.height();
    if (deviceWidthPx <= 0 || deviceHeightPx <= 0
        || paintPx.width() <= 0 || paintPx.height() <= 0) {
        return fail(QStringLiteral("Не удалось определить печатаемую область."));
    }

    // Устройство уже равно области печати (типичный случай для драйвера
    // Windows): тогда поля уже учтены, и сдвигать контент не нужно.
    const bool deviceIsPaintRect =
        qAbs(deviceWidthPx - paintPx.width()) <= 2
        && qAbs(deviceHeightPx - paintPx.height()) <= 2;

    const qreal contentWidthPx = deviceIsPaintRect ? deviceWidthPx : paintPx.width();
    const qreal contentHeightPx = deviceIsPaintRect ? deviceHeightPx : paintPx.height();
    const qreal offsetXPx = deviceIsPaintRect ? 0.0 : paintPx.x();
    const qreal offsetYPx = deviceIsPaintRect ? 0.0 : paintPx.y();
    if (contentWidthPx <= 0 || contentHeightPx <= 0)
        return fail(QStringLiteral("Не удалось определить размеры страницы."));

    // pxPerMm нужен только для толщин линий, отступов и минимальных высот.
    const qreal pxPerMm = printer.logicalDpiX() / 25.4;
    if (pxPerMm <= 0)
        return fail(QStringLiteral("Не удалось определить разрешение печати."));

    // Полезная ширина A4 в альбомной ориентации — ровно 267 мм;
    // коэффициент страхует от округления драйвера/PDF-движка.
    const qreal scale = contentWidthPx / (kTotalWidthMm * pxPerMm);
    qreal widthsPx[kColumnCount];
    for (int column = 0; column < kColumnCount; ++column)
        widthsPx[column] = kColumnWidthsMm[column] * pxPerMm * scale;

    QFont baseFont;
    baseFont.setPointSizeF(9.0);

    QFont titleFont = baseFont;
    titleFont.setPointSizeF(13.0);
    titleFont.setBold(true);

    QFont infoFont = baseFont;
    infoFont.setPointSizeF(8.0);

    QFont headerFont = infoFont;
    headerFont.setBold(true);

    QFont bodyFont = baseFont;

    const QFontMetricsF titleMetrics(titleFont, &printer);
    const QFontMetricsF infoMetrics(infoFont, &printer);
    const QFontMetricsF headerMetrics(headerFont, &printer);
    const QFontMetricsF bodyMetrics(bodyFont, &printer);

    const QStringList headers{
        QStringLiteral("№"),
        QStringLiteral("ФИО"),
        QStringLiteral("Мкр."),
        QStringLiteral("Год рожд."),
        QStringLiteral("Льготная категория"),
        QStringLiteral("Год посещ."),
        QStringLiteral("Подписка"),
    };

    QVector<QString> headerTexts;
    headerTexts.reserve(kColumnCount);
    for (const QString &header : headers)
        headerTexts.append(header);

    const qreal titleHeightPx = titleMetrics.height();
    const qreal infoHeightPx = infoMetrics.height();
    const qreal gapPx = 2.0 * pxPerMm;
    const qreal headerRowHeightPx = measureRowHeightPx(
        headerTexts, headerMetrics, widthsPx, pxPerMm, kMinHeaderHeightMm);
    const qreal footerHeightPx = infoHeightPx + 2.0 * pxPerMm;

    const qreal tableTopPx =
        titleHeightPx + infoHeightPx + gapPx + headerRowHeightPx;
    const qreal bodyHeightPx =
        contentHeightPx - tableTopPx - footerHeightPx;
    if (bodyHeightPx <= 0)
        return fail(QStringLiteral("Слишком маленькая область печати."));

    QVector<QVector<QString>> rows;
    QVector<qreal> rowHeights;
    rows.reserve(records.size());
    rowHeights.reserve(records.size());
    for (int index = 0; index < records.size(); ++index) {
        const QVector<QString> texts = rowTexts(records.at(index), index + 1);
        rows.append(texts);
        rowHeights.append(measureRowHeightPx(
            texts, bodyMetrics, widthsPx, pxPerMm, kMinRowHeightMm));
    }

    // Разбиение на страницы: строка не разрывается, повторная шапка
    // и номер страницы учитываются в доступной высоте.
    QVector<int> pageStarts;
    pageStarts.append(0);
    qreal usedHeightPx = 0;
    for (int index = 0; index < rows.size(); ++index) {
        const qreal height = rowHeights.at(index);
        if (usedHeightPx > 0 && usedHeightPx + height > bodyHeightPx) {
            pageStarts.append(index);
            usedHeightPx = 0;
        }
        usedHeightPx += height;
    }

    const int pageCount = pageStarts.size();

    QPainter painter;
    if (!painter.begin(&printer)) {
        return fail(QStringLiteral(
            "Не удалось начать печать. Проверьте, что принтер доступен, "
            "а файл не открыт в другой программе."));
    }

    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    const QString title = QStringLiteral("СПИСОК ВЕТЕРАНОВ");
    const QString formedText = QStringLiteral("Сформировано: %1")
                                   .arg(QDate::currentDate().toString(
                                       QStringLiteral("dd.MM.yyyy")));
    const QString totalText =
        QStringLiteral("Записей: %1").arg(records.size());

    bool pagesOk = true;
    for (int page = 0; page < pageCount; ++page) {
        if (page > 0 && !printer.newPage()) {
            pagesOk = false;
            break;
        }

        painter.save();
        painter.translate(offsetXPx, offsetYPx);
        painter.setPen(Qt::black);

        qreal y = 0;

        painter.setFont(titleFont);
        painter.drawText(QRectF(0, y, contentWidthPx, titleHeightPx),
                         Qt::AlignHCenter | Qt::AlignVCenter, title);
        y += titleHeightPx;

        painter.setFont(infoFont);
        painter.drawText(QRectF(0, y, contentWidthPx, infoHeightPx),
                         Qt::AlignLeft | Qt::AlignVCenter, formedText);
        painter.drawText(QRectF(0, y, contentWidthPx, infoHeightPx),
                         Qt::AlignRight | Qt::AlignVCenter, totalText);
        y += infoHeightPx + gapPx;

        drawTableRow(painter, headerTexts, headerFont, y,
                     headerRowHeightPx, widthsPx, contentWidthPx,
                     pxPerMm, true);
        y += headerRowHeightPx;

        const int start = pageStarts.at(page);
        const int end = (page + 1 < pageCount) ? pageStarts.at(page + 1)
                                               : rows.size();
        for (int index = start; index < end; ++index) {
            const qreal height = rowHeights.at(index);
            drawTableRow(painter, rows.at(index), bodyFont, y, height,
                         widthsPx, contentWidthPx, pxPerMm, false);
            y += height;
        }

        painter.setFont(infoFont);
        painter.drawText(
            QRectF(0, contentHeightPx - footerHeightPx,
                   contentWidthPx, footerHeightPx),
            Qt::AlignHCenter | Qt::AlignVCenter,
            QStringLiteral("Стр. %1 из %2").arg(page + 1).arg(pageCount));

        painter.restore();
    }

    painter.end();

    if (!pagesOk)
        return fail(QStringLiteral("Не удалось создать страницы документа."));

    return true;
}
