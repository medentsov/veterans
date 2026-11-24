#!/usr/bin/env python3
"""Проверка диалога сохранения экспорта (M7-04) без Qt.

Проверяет, что Excel/PDF экспорт открывает диалог сохранения, предлагает
имя файла с областью и датой, а папка последнего экспорта запоминается
в `settings.ini` и переживает перезапуск.

    python3 tools/check_export_dialog.py
"""

from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
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


def cpp():
    return MAIN_CPP.read_text(encoding="utf-8")


def require(source, needle, what):
    assert needle in source, f"не найдено: {what} ({needle!r})"


def between(source, start_marker, end_marker):
    start = source.index(start_marker)
    end = source.index(end_marker, start)
    return source[start:end]


def export_body():
    return between(cpp(), "void MainWindow::exportRecords",
                   "void MainWindow::onToggleAllChecked")


def check_dialog():
    def save_dialog():
        body = export_body()
        require(body, "QFileDialog::getSaveFileName", "диалог сохранения")
        require(body, "this, title, suggestedPath, filter",
                "параметры диалога: заголовок, имя, фильтр")
        require(body, "if (filePath.isEmpty())", "отмена диалога")
        require(body, "return;", "выход без экспорта при отмене")

    check("диалог сохранения и отмена без экспорта", save_dialog)

    def suggested_path():
        body = export_body()
        require(body, "QDir(exportStartDir())",
                "папка последнего экспорта")
        require(body, ".filePath(defaultExportFileName(",
                "имя файла в выбранной папке")
        require(body, "defaultExportFileName(",
                "имя файла по области и расширению")

    check("предлагаемый путь: папка + имя по умолчанию", suggested_path)


def check_default_name():
    def name_parts():
        body = between(cpp(), "QString defaultExportFileName",
                       "QString defaultExportDir")
        require(body, "Ветераны", "базовое имя «Ветераны»")
        require(body, "yyyy-MM-dd", "дата в имени файла")
        require(body, "_отмеченные", "суффикс для отмеченных")
        require(body, "_фильтр", "суффикс для результата фильтра")
        require(body, "case ExportScope::Checked", "ветка отмеченных")
        require(body, "case ExportScope::All", "ветка всех записей")
        require(body, "case ExportScope::Filtered", "ветка фильтра")
        require(body, "extension", "расширение xlsx/pdf")

    check("имя файла: область, дата и расширение", name_parts)

    def extension():
        body = export_body()
        require(body,
                'filePath.endsWith(QStringLiteral(".") + extension,',
                "проверка расширения")
        require(body, "Qt::CaseInsensitive", "регистронезависимая проверка")
        require(body, 'filePath += QStringLiteral(".") + extension;',
                "добавление расширения")

    check("расширение добавляется, если пользователь его не ввёл", extension)


def check_default_dir():
    def documents():
        body = between(cpp(), "QString defaultExportDir",
                       "QString exportStartDir")
        require(body, "QStandardPaths::DocumentsLocation", "папка «Документы»")
        require(body, "if (dir.isEmpty())", "проверка пустого пути")
        require(body, "QDir::homePath()", "запасная домашняя папка")

    check("папка по умолчанию: «Документы» или домашняя", documents)


def check_remembered_dir():
    def read():
        body = between(cpp(), "QString exportStartDir",
                       "void rememberExportDir")
        require(body,
                "QSettings settings(AppPaths::settingsFile(), "
                "QSettings::IniFormat);",
                "чтение settings.ini")
        require(body, 'QStringLiteral("Export/lastDir")',
                "ключ Export/lastDir")
        require(body, "QDir(saved).exists()", "проверка существования папки")
        require(body, "return defaultExportDir();", "запасная папка")

    check("папка последнего экспорта читается из settings.ini", read)

    def write():
        body = between(cpp(), "void rememberExportDir", "} // namespace")
        require(body, "QFileInfo(filePath).absoluteDir()",
                "папка выбранного файла")
        require(body, "settings.setValue(QStringLiteral(\"Export/lastDir\"),",
                "запись Export/lastDir")
        require(body, "dir.absolutePath()", "абсолютный путь")
        require(body, "settings.sync();", "сброс настроек на диск")

    check("папка последнего экспорта записывается в settings.ini", write)

    def order():
        body = export_body()
        assert body.index("rememberExportDir(filePath);") < \
            body.index("ExcelExporter::exportList"), \
            "папка должна запоминаться до экспорта"
        assert body.index("rememberExportDir(filePath);") < \
            body.index("ListPdfExporter::exportList"), \
            "папка должна запоминаться до PDF-экспорта"

    check("папка запоминается до попытки экспорта", order)


def check_wiring():
    def includes():
        text = cpp()
        for header in ("QFileDialog", "QFileInfo", "QSettings",
                       "QStandardPaths", "QDir", "core/AppPaths.h"):
            require(text, f"#include <{header}>" if "/" not in header
                    else f'#include "{header}"',
                    f"подключение {header}")

    check("подключения для диалога и настроек", includes)

    def cmake():
        text = CMAKE.read_text(encoding="utf-8")
        for source in ("src/report/ExportFileGuard.cpp",
                       "src/report/ListRenderer.cpp",
                       "src/report/CardRenderer.cpp"):
            require(text, source, f"{source} в CMake")

    check("CMake: исходники экспорта и рендереров подключены", cmake)


def main():
    print("Проверка диалога сохранения экспорта (M7-04) без Qt")
    check_dialog()
    check_default_name()
    check_default_dir()
    check_remembered_dir()
    check_wiring()

    print(f"\nПроверок: {checks}, провалов: {len(failures)}")
    if failures:
        for item in failures:
            print(" -", item)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
