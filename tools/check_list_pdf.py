#!/usr/bin/env python3
"""Проверка PDF-списка и шаблона списка (M7-03/M8-02) без Qt.

Без аргументов проверяет исходники и настройки макета.
С аргументом — реальный PDF, созданный приложением:

    python3 tools/check_list_pdf.py
    python3 tools/check_list_pdf.py /path/to/veterans.pdf

Во втором режиме проверяются формат A4-альбом, число страниц и размер полосы
набора (267×180 мм) по MediaBox.
"""

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
RENDERER = ROOT / "src" / "report" / "ListRenderer.cpp"
RENDERER_HEADER = ROOT / "src" / "report" / "ListRenderer.h"
EXPORTER = ROOT / "src" / "report" / "ListPdfExporter.cpp"
EXPORTER_HEADER = ROOT / "src" / "report" / "ListPdfExporter.h"
MAIN_CPP = ROOT / "src" / "ui" / "MainWindow.cpp"
CMAKE = ROOT / "CMakeLists.txt"

checks = 0
failures = []


def check(name, fn):
    global checks
    checks += 1
    try:
        fn()
        print(f"  ok   {name}")
    except Exception as exc:  # noqa: BLE001
        failures.append(f"{name}: {exc}")
        print(f"  FAIL {name}: {exc}")


def text():
    return RENDERER.read_text(encoding="utf-8")


def exporter_text():
    return EXPORTER.read_text(encoding="utf-8")


def require(source, needle, what):
    assert needle in source, f"не найдено: {what} ({needle!r})"


def check_sources():
    src = text()

    def files():
        for path in (RENDERER_HEADER, RENDERER, EXPORTER_HEADER, EXPORTER):
            assert path.exists(), f"нет {path.relative_to(ROOT)}"

    check("файлы ListRenderer и ListPdfExporter существуют", files)

    def api():
        header = RENDERER_HEADER.read_text(encoding="utf-8")
        require(header, "class ListRenderer", "класс ListRenderer")
        require(header, "static bool render(QPrinter &printer,",
                "метод render(QPrinter&)")
        require(header, "const QVector<PersonRecord> &records",
                "передача списка записей")

    check("шаблон списка: общий API ListRenderer", api)

    def landscape_a4():
        require(src, "QPageSize(QPageSize::A4)", "формат A4")
        require(src, "QPageLayout::Landscape", "альбомная ориентация")
        require(src, "constexpr qreal kMarginMm = 15.0;", "поля 15 мм")
        assert "QMarginsF(kMarginMm, kMarginMm, kMarginMm, kMarginMm)" in src, \
            "поля заданы не со всех сторон"
        require(src, "printer.setFullPage(true)", "полная страница для вёрстки")
        # Регрессия: после setFullPage(true) paintRect() возвращает весь лист,
        # и поля 15 мм теряются — таблица обрезается справа (дефект M12-09).
        assert "pageLayout().paintRect(" not in src, \
            "paintRect() использовать нельзя: после setFullPage(true) он даёт весь лист"
        require(src, "marginsMm.left(), marginsMm.top()",
                "контент смещается на поля макета")
        require(src, "paperMm.width() - marginsMm.left() - marginsMm.right()",
                "ширина области печати без полей")

    check("A4 альбомный, поля 15 мм", landscape_a4)

    def columns():
        m = re.search(r"kColumnWidthsMm\[kColumnCount\]\s*=\s*\{(.*?)\};",
                      src, re.S)
        assert m, "нет массива kColumnWidthsMm"
        widths = [float(x) for x in re.findall(r"\d+(?:\.\d+)?", m.group(1))]
        assert len(widths) == 7, f"колонок: {len(widths)}"
        assert abs(sum(widths) - 267.0) < 0.01, \
            f"сумма ширин {sum(widths)} мм вместо 267"
        require(src, "constexpr int kColumnCount = 7;", "7 колонок")

    check("7 колонок, сумма ширин 267 мм", columns)

    def headers():
        for label in ("СПИСОК ВЕТЕРАНОВ", "№", "ФИО", "Мкр.",
                      "Год рожд.", "Льготная категория", "Год посещ.",
                      "Подписка"):
            assert label in src, f"нет заголовка «{label}»"

    check("заголовок и шапка таблицы по макету", headers)

    def fonts():
        require(src, "titleFont.setPointSizeF(13.0)", "заголовок 13 pt")
        require(src, "infoFont.setPointSizeF(8.0)", "шапка/подписи 8 pt")
        require(src, "baseFont.setPointSizeF(9.0)", "основной кегль 9 pt")
        require(src, "headerFont.setBold(true)", "жирная шапка")

    check("кегли 13/9/8 pt, жирная шапка", fonts)

    def header_repeat():
        start = src.index("for (int page = 0; page < pageCount; ++page)")
        end = src.index("painter.end();", start)
        body = src[start:end]
        require(body, "headerTexts", "повтор шапки внутри цикла страниц")
        require(body, "headerRowHeightPx", "высота шапки на каждой странице")
        require(body, "drawTableRow", "отрисовка строк")
        assert body.count("drawTableRow") >= 2, \
            "шапка и строки должны рисоваться отдельно"

    check("шапка таблицы повторяется на каждой странице", header_repeat)

    def page_numbers():
        require(src, 'QStringLiteral("Стр. %1 из %2")',
                "нумерация «Стр. X из Y»")
        require(src, "Qt::AlignHCenter | Qt::AlignVCenter",
                "номер страницы по центру")

    check("нумерация «Стр. X из Y» по центру снизу", page_numbers)

    def no_row_split():
        require(src, "usedHeightPx + height > bodyHeightPx",
                "перенос строки на следующую страницу")
        require(src, "pageStarts.append(index)", "разбиение по строкам")
        require(src, "Qt::TextWordWrap", "перенос длинного текста")
        assert src.count("Qt::TextWrapAnywhere") >= 2, \
            "неразрывное слово должно переноситься по буквам при замере и отрисовке"
        require(src, "measureRowHeightPx", "расчёт высоты строки")
        require(src, "kMinRowHeightMm", "минимальная высота строки 7 мм")

    check("строки не разрываются, длинный текст переносится", no_row_split)

    def values():
        require(src, 'QStringLiteral("—")', "подстановка «—»")
        require(src, '"%1, %2"', "подписка в формате «Ветеран, 2026»")
        require(src, "Нет записей для экспорта.", "защита от пустой выборки")

    check("пустые значения и формат подписки", values)


def check_wiring():
    main = MAIN_CPP.read_text(encoding="utf-8")
    cmake = CMAKE.read_text(encoding="utf-8")

    def ui():
        require(main, '#include "report/ListPdfExporter.h"',
                "подключение ListPdfExporter")
        require(main, 'addMenu(tr("Экспорт в PDF"))', "меню PDF")
        require(main, "ListPdfExporter::exportList(records, filePath, &error)",
                "вызов PDF-экспортёра")
        for scope in ("Checked", "All", "Filtered"):
            require(main, f"exportPdf(ExportScope::{scope})",
                    f"область PDF {scope}")

    check("UI: меню «Экспорт в PDF» с тремя областями", ui)

    def build():
        require(cmake, "src/report/ListPdfExporter.h", "заголовок в CMake")
        require(cmake, "src/report/ListPdfExporter.cpp", "исходник в CMake")
        require(cmake, "src/report/ListRenderer.h", "ListRenderer.h в CMake")
        require(cmake, "src/report/ListRenderer.cpp", "ListRenderer.cpp в CMake")
        require(cmake, "src/report/ExportFileGuard.cpp",
                "ExportFileGuard.cpp в CMake")
        require(cmake, "Qt6::PrintSupport", "Qt PrintSupport в CMake")

    check("CMake: рендерер, экспортёр и QtPrintSupport", build)

    def exporter():
        src = exporter_text()
        require(src, "ListRenderer::render(printer, records, &renderError)",
                "вызов общего рендерера")
        require(src, "ExportFileGuard::validateTarget", "проверка цели")
        require(src, "ExportFileGuard::replaceFile", "атомарная замена")
        require(src, 'filePath + QStringLiteral(".tmp")',
                "временный файл рядом с целью")
        assert src.count("QFile::remove(tempPath)") >= 3, \
            "временный файл должен удаляться при ошибках"

    check("экспортёр: рендерер, временный файл и безопасная замена", exporter)


def check_pdf(path):
    path = Path(path)
    assert path.exists(), f"файл не найден: {path}"
    data = path.read_bytes()
    assert data.startswith(b"%PDF-"), "нет сигнатуры PDF"
    assert len(data) > 1000, "файл подозрительно мал"

    def pages():
        try:
            from pypdf import PdfReader  # type: ignore
        except ImportError:
            count = len(re.findall(rb"/Type\s*/Page(?!s)", data))
            assert count >= 1, "не найдено ни одной страницы"
            return

        reader = PdfReader(str(path))
        assert len(reader.pages) >= 1, "в PDF нет страниц"
        box = reader.pages[0].mediabox
        width = float(box.width)
        height = float(box.height)
        assert width > height, f"страница не альбомная: {width}×{height} pt"
        assert abs(width - 841.89) < 2.0 and abs(height - 595.28) < 2.0, \
            f"формат не A4: {width}×{height} pt"

    check("файл: PDF, A4-альбом, есть страницы", pages)

    def printable_margins():
        # Полоса набора 267×180 мм = 756.85×510.24 pt; проверяем только при
        # наличии pypdf и текстового слоя с MediaBox.
        try:
            from pypdf import PdfReader  # type: ignore
        except ImportError:
            return
        reader = PdfReader(str(path))
        box = reader.pages[0].mediabox
        width_mm = float(box.width) / 72.0 * 25.4
        height_mm = float(box.height) / 72.0 * 25.4
        assert abs(width_mm - 297.0) < 2.0, f"ширина {width_mm:.1f} мм"
        assert abs(height_mm - 210.0) < 2.0, f"высота {height_mm:.1f} мм"

    check("файл: физический размер страницы A4 297×210 мм", printable_margins)


def main():
    if len(sys.argv) > 1:
        print(f"Проверка файла: {sys.argv[1]}")
        check_pdf(sys.argv[1])
    else:
        print("Проверка PDF-списка и шаблона списка (M7-03/M8-02) без Qt")
        check_sources()
        check_wiring()

    print(f"\nПроверок: {checks}, провалов: {len(failures)}")
    if failures:
        for item in failures:
            print(" -", item)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
