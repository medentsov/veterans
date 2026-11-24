#!/usr/bin/env python3
"""Проверка области экспорта (M7-02) без Qt.

Проверяет, что в главном окне есть выбор «отмеченные / все / результат
фильтра», что записи собираются из нужного источника и что Excel-экспорт
получает именно эту выборку.

    python3 tools/check_export_scope.py
"""

from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
MAIN_CPP = ROOT / "src" / "ui" / "MainWindow.cpp"
MAIN_H = ROOT / "src" / "ui" / "MainWindow.h"
SCOPE_H = ROOT / "src" / "report" / "ExportScope.h"
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


def cpp():
    return MAIN_CPP.read_text(encoding="utf-8")


def header():
    return MAIN_H.read_text(encoding="utf-8")


def require(text, needle, what):
    assert needle in text, f"не найдено: {what} ({needle!r})"


def check_scope_enum():
    def exists():
        assert SCOPE_H.exists(), "нет src/report/ExportScope.h"
        text = SCOPE_H.read_text(encoding="utf-8")
        for value in ("Checked", "All", "Filtered"):
            require(text, value, f"область {value}")

    check("ExportScope: три области (отмеченные/все/фильтр)", exists)


def check_menu():
    def menu_items():
        text = cpp()
        require(text, 'addMenu(tr("Экспорт в Excel"))', "меню экспорта Excel")
        require(text, 'addMenu(tr("Экспорт в PDF"))', "меню экспорта PDF")
        assert text.count('tr("Отмеченные записи")') >= 2, \
            "нет пункта «Отмеченные записи» в обоих меню"
        assert text.count('tr("Все записи")') >= 2, \
            "нет пункта «Все записи» в обоих меню"
        assert text.count('tr("Результат фильтра")') >= 2, \
            "нет пункта «Результат фильтра» в обоих меню"

    check("Файл → Экспорт в Excel/PDF: три области", menu_items)

    def triggers():
        text = cpp()
        for method in ("exportExcel", "exportPdf"):
            for scope in ("ExportScope::Checked", "ExportScope::All",
                          "ExportScope::Filtered"):
                require(text, f"{method}({scope})",
                        f"вызов {method} для {scope}")

    check("каждый пункт вызывает свой экспорт со своей областью", triggers)


def check_collect():
    def sources():
        text = cpp()
        start = text.index("recordsForScope(")
        end = text.index("defaultExportFileName(", start)
        body = text[start:end]
        require(body, "model->checkedRecords()", "отмеченные из модели")
        require(body, "proxy->mapToSource(", "фильтр через прокси")
        require(body, "model->recordAt(row)", "все записи из модели")

    check("выборка: галочки, весь список, результат фильтра", sources)

    def hidden_checked():
        text = cpp()
        start = text.index("recordsForScope(")
        end = text.index("defaultExportFileName(", start)
        body = text[start:end]
        assert body.index("checkedRecords") < body.index("ExportScope::Filtered"), \
            "галочки должны собираться независимо от фильтра"

    check("отмеченные не зависят от активного фильтра", hidden_checked)


def check_export_call():
    def export_flow():
        text = cpp()
        start = text.index("void MainWindow::exportRecords")
        body = text[start:]
        require(body, "recordsForScope(m_model, m_proxy, scope)",
                "выборка по области")
        require(body, "records.isEmpty()", "проверка пустой выборки")
        require(body, "QFileDialog::getSaveFileName", "диалог сохранения")
        require(body, "ExcelExporter::exportList(records, filePath, &error)",
                "передача выборки в ExcelExporter")
        require(body, "ListPdfExporter::exportList(records, filePath, &error)",
                "передача выборки в ListPdfExporter")

    check("exportRecords: выборка → диалог → Excel/PDF", export_flow)


def check_action_states():
    def states():
        text = cpp()
        start = text.index("void MainWindow::updateExportActions")
        end = text.index("void MainWindow::exportExcel", start)
        body = text[start:end]
        require(body, "checkedIds().size()", "количество отмеченных")
        require(body, "rowCount()", "количество записей")
        require(body, "setEnabled(checked > 0)", "включение «отмеченные»")
        require(body, "setEnabled(total > 0)", "включение «все»")
        require(body, "setEnabled(hasFilter && found > 0)",
                "включение «результат фильтра»")

    check("доступность пунктов по количеству записей", states)

    def updated():
        text = cpp()
        start = text.index("void MainWindow::updateActionStates")
        end = text.index("void MainWindow::updateExportActions", start)
        assert "updateExportActions();" in text[start:end], \
            "updateExportActions не вызывается из updateActionStates"
        require(header(), "void updateExportActions();",
                "объявление updateExportActions")

    check("состояние пунктов обновляется вместе с таблицей", updated)


def check_wiring():
    def includes():
        require(header(), '#include "report/ExportScope.h"',
                "подключение ExportScope в MainWindow.h")
        require(cpp(), '#include "report/ExcelExporter.h"',
                "подключение ExcelExporter в MainWindow.cpp")

    check("подключения заголовков", includes)

    def cmake():
        text = CMAKE.read_text(encoding="utf-8")
        require(text, "src/report/ExportScope.h", "ExportScope.h в CMake")

    check("CMake: ExportScope.h в списке исходников", cmake)


def check_default_names():
    def names():
        text = cpp()
        start = text.index("QString defaultExportFileName")
        end = text.index("QString defaultExportDir", start)
        body = text[start:end]
        require(body, "_отмеченные", "суффикс имени для отмеченных")
        require(body, "_фильтр", "суффикс имени для фильтра")
        require(body, "yyyy-MM-dd", "дата в имени файла")
        require(body, "Ветераны", "базовое имя файла")

    check("имя файла по умолчанию учитывает область", names)


def main():
    print("Проверка области экспорта (M7-02) без Qt")
    check_scope_enum()
    check_menu()
    check_collect()
    check_export_call()
    check_action_states()
    check_wiring()
    check_default_names()

    print(f"\nПроверок: {checks}, провалов: {len(failures)}")
    if failures:
        for item in failures:
            print(" -", item)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
