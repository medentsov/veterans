#!/usr/bin/env python3
"""Проверка экспорта в Excel (M7-01) без Qt.

Без аргументов проверяет исходники и логику правки XML листа.
С аргументом — реальный .xlsx, созданный приложением:

    python3 tools/check_excel_export.py
    python3 tools/check_excel_export.py /path/to/veterans.xlsx

Во втором режиме проверяются шапка, ширины колонок, стили, автофильтр
и закрепление первой строки.
"""

import re
import sys
import zipfile
import xml.etree.ElementTree as ET
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
CPP = ROOT / "src" / "report" / "ExcelExporter.cpp"
HEADER = ROOT / "src" / "report" / "ExcelExporter.h"
CMAKE = ROOT / "CMakeLists.txt"
QXLSX_DIR = ROOT / "third_party" / "QXlsx"

HEADERS = [
    "№",
    "ФИО",
    "Микрорайон",
    "Год рождения",
    "Льготная категория",
    "Год посещения",
    "Подписка на газету",
]

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


def local(tag):
    return tag.rsplit("}", 1)[-1]


def source():
    return CPP.read_text(encoding="utf-8")


def check_sources():
    text = source()

    def files_exist():
        assert HEADER.exists(), "нет ExcelExporter.h"
        assert CPP.exists(), "нет ExcelExporter.cpp"

    check("файлы ExcelExporter существуют", files_exist)

    def headers_present():
        missing = [h for h in HEADERS if f'QStringLiteral("{h}")' not in text]
        assert not missing, f"нет заголовков: {missing}"

    check("шапка: 7 колонок как в PRINT-LAYOUT", headers_present)

    def formatting():
        for needle in (
            "setFontBold(true)",
            "setBorderStyle",
            "PatternSolid",
            "setColumnWidth",
            "setRowHeight",
            "saveAs",
        ):
            assert needle in text, f"не найдено: {needle}"

    check("стили, ширины, высота шапки, сохранение", formatting)

    def empty_values():
        assert 'QStringLiteral("—")' in text, "нет подстановки «—»"

    check("пустые значения заменяются на «—»", empty_values)

    def freeze_and_filter():
        for needle in ('ySplit=\\"1\\"', 'topLeftCell=\\"A2\\"',
                       'state=\\"frozen\\"', '"<autoFilter ref=\\"A1:G"'):
            assert needle in text, f"не найдено: {needle}"

    check("закрепление строки и автофильтр в коде", freeze_and_filter)

    def cmake_wiring():
        cmake = CMAKE.read_text(encoding="utf-8")
        assert "add_subdirectory(third_party/QXlsx/QXlsx)" in cmake
        assert "QXlsx::QXlsx" in cmake
        assert "src/report/ExcelExporter.cpp" in cmake

    check("CMake: QXlsx и исходники подключены", cmake_wiring)

    def vendor():
        assert (QXLSX_DIR / "QXlsx" / "CMakeLists.txt").exists()
        license_text = (QXLSX_DIR / "LICENSE").read_text(encoding="utf-8")
        assert "MIT License" in license_text

    check("QXlsx 1.5.1.1 с лицензией MIT на месте", vendor)


def apply_freeze(sheet):
    """Зеркало вставки из ExcelExporter: <pane> первым внутри <sheetView>.

    Контейнер <sheetViews> тоже начинается с «<sheetView», поэтому его нужно
    пропускать — иначе <pane> попадает не в тот элемент и Excel просит
    восстановить файл (дефект M12-10).
    """
    pane = (
        '<pane ySplit="1" topLeftCell="A2" activePane="bottomLeft" '
        'state="frozen"/>'
        '<selection pane="bottomLeft" activeCell="A2" sqref="A2"/>'
    )
    start = sheet.find("<sheetView")
    while start >= 0 and sheet[start + 10:start + 11] == "s":
        start = sheet.find("<sheetView", start + 10)
    assert start >= 0, "в листе нет элемента <sheetView>"
    end = sheet.index(">", start)
    if sheet[end - 1] == "/":
        # Самозакрывающийся <sheetView .../> превращаем в парный: «/» убираем,
        # «>» оставляем, <pane> ставим сразу после него.
        return sheet[:end - 1] + ">" + pane + "</sheetView>" + sheet[end + 1:]
    return sheet[:end + 1] + pane + sheet[end + 1:]


SHEET_TEMPLATE = (
    '<?xml version="1.0" encoding="UTF-8" standalone="yes"?>'
    '<worksheet xmlns="http://schemas.openxmlformats.org/'
    'spreadsheetml/2006/main">'
    '<dimension ref="A1:G3"/>'
    '<sheetViews>{view}</sheetViews>'
    '<sheetFormatPr defaultRowHeight="15"/>'
    '<sheetData><row r="1"><c r="A1" t="s"><v>0</v></c></row></sheetData>'
    '<pageMargins left="0.7" right="0.7" top="0.75" bottom="0.75"'
    ' header="0.3" footer="0.3"/>'
    '</worksheet>'
)


def check_xml_logic():
    """Повторяет правку листа: <pane> первым в sheetView, autoFilter после sheetData."""

    def validate():
        # QXlsx пишет либо парный <sheetView>, либо самозакрывающийся — оба
        # варианта обязаны получить <pane> внутри себя, а не в <sheetViews>.
        views = (
            '<sheetView workbookViewId="0"></sheetView>',
            '<sheetView showRuler="0" workbookViewId="0"/>',
        )
        for view in views:
            sheet = apply_freeze(SHEET_TEMPLATE.format(view=view))

            close = "</sheetData>"
            pos = sheet.index(close) + len(close)
            sheet = sheet[:pos] + '<autoFilter ref="A1:G3"/>' + sheet[pos:]

            root = ET.fromstring(sheet)
            container = next(el for el in root.iter()
                             if local(el.tag) == "sheetViews")
            tags = [local(el.tag) for el in list(container)]
            assert tags == ["sheetView"], f"в sheetViews лишние элементы: {tags}"

            element = container[0]
            children = [local(el.tag) for el in list(element)]
            assert children and children[0] == "pane", \
                f"pane не первый внутри sheetView: {children}"
            pane = element[0]
            assert pane.get("ySplit") == "1"
            assert pane.get("topLeftCell") == "A2"
            assert pane.get("state") == "frozen"

            auto = next(el for el in root.iter() if local(el.tag) == "autoFilter")
            assert auto.get("ref") == "A1:G3"

            tags = [local(el.tag) for el in list(root)]
            assert tags.index("autoFilter") == tags.index("sheetData") + 1, tags

    check("XML листа: pane внутри sheetView, autoFilter после sheetData", validate)

    def source_guard():
        source = CPP.read_text(encoding="utf-8")
        assert 'sheet.mid(viewStart + 10, 1) == QLatin1String("s")' in source, \
            "нет пропуска контейнера <sheetViews> при поиске <sheetView>"

    check("исходник: <sheetViews> не путается с <sheetView>", source_guard)


def shared_strings(zf):
    if "xl/sharedStrings.xml" not in zf.namelist():
        return []
    root = ET.fromstring(zf.read("xl/sharedStrings.xml"))
    result = []
    for si in root:
        result.append("".join(t.text or "" for t in si.iter() if local(t.tag) == "t"))
    return result


def check_xlsx(path):
    path = Path(path)
    assert path.exists(), f"файл не найден: {path}"
    assert path.stat().st_size > 0, "файл пуст"

    with zipfile.ZipFile(path) as zf:
        names = zf.namelist()
        assert "xl/worksheets/sheet1.xml" in names, "нет листа sheet1.xml"
        assert "xl/styles.xml" in names, "нет styles.xml"
        sheet = ET.fromstring(zf.read("xl/worksheets/sheet1.xml"))
        styles = zf.read("xl/styles.xml").decode("utf-8")
        strings = shared_strings(zf)

    def freeze():
        view = next((el for el in sheet.iter() if local(el.tag) == "sheetView"),
                    None)
        assert view is not None, "нет <sheetView>"
        pane = next((el for el in list(view) if local(el.tag) == "pane"), None)
        assert pane is not None, "<pane> должен быть внутри <sheetView>"
        assert pane.get("ySplit") == "1", pane.attrib
        assert pane.get("state") == "frozen", pane.attrib

    check("файл: первая строка закреплена", freeze)

    def autofilter():
        auto = next((el for el in sheet.iter() if local(el.tag) == "autoFilter"), None)
        assert auto is not None, "нет <autoFilter>"
        assert re.fullmatch(r"A1:G\d+", auto.get("ref", "")), auto.attrib

    check("файл: автофильтр A1:G…", autofilter)

    def header():
        row = next((el for el in sheet.iter()
                    if local(el.tag) == "row" and el.get("r") == "1"), None)
        assert row is not None, "нет строки 1"
        values = []
        for cell in row:
            value = cell.find("{*}v")
            if value is None:
                values.append("")
            elif cell.get("t") == "s":
                values.append(strings[int(value.text)])
            else:
                values.append(value.text)
        assert values[:7] == HEADERS, values[:7]

    check("файл: шапка из 7 колонок", header)

    def widths():
        cols = [el for el in sheet.iter() if local(el.tag) == "col"]
        assert len(cols) >= 7, f"колонок с шириной: {len(cols)}"
        assert all(float(c.get("width", "0")) > 0 for c in cols[:7])

    check("файл: заданы ширины колонок", widths)

    def styling():
        assert "305496" in styles, "нет заливки шапки"
        assert "F2F5FA" in styles, "нет чередования строк"
        assert re.search(r"<b\s*/>|<b>", styles), "нет жирного шрифта"

    check("файл: заливка шапки, жирный шрифт, чередование", styling)


def main():
    if len(sys.argv) > 1:
        print(f"Проверка файла: {sys.argv[1]}")
        check_xlsx(sys.argv[1])
    else:
        print("Проверка исходников и XML-логики (без Qt)")
        check_sources()
        check_xml_logic()

    print(f"\nПроверок: {checks}, провалов: {len(failures)}")
    if failures:
        for item in failures:
            print(" -", item)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
