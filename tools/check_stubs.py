#!/usr/bin/env python3
"""Проверка заглушек будущего: импорт из Excel (M10-01/M10-02) без Qt.

Проверяет неактивный пункт меню с tooltip, интерфейс `IImportService`,
пустую реализацию `NullImportService` и точку вызова в главном окне.

    python3 tools/check_stubs.py
"""

from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
INTERFACE_H = ROOT / "src" / "import" / "IImportService.h"
NULL_H = ROOT / "src" / "import" / "NullImportService.h"
NULL_CPP = ROOT / "src" / "import" / "NullImportService.cpp"
MAIN_CPP = ROOT / "src" / "ui" / "MainWindow.cpp"
MAIN_H = ROOT / "src" / "ui" / "MainWindow.h"
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


def read(path):
    return path.read_text(encoding="utf-8")


def require(source, needle, what):
    assert needle in source, f"не найдено: {what} ({needle!r})"


def between(source, start_marker, end_marker):
    start = source.index(start_marker)
    end = source.index(end_marker, start)
    return source[start:end]


def check_files():
    def exists():
        for path in (INTERFACE_H, NULL_H, NULL_CPP):
            assert path.exists(), f"нет {path.relative_to(ROOT)}"

    check("файлы импорта существуют", exists)

    def cmake():
        text = read(CMAKE)
        for source in ("src/import/IImportService.h",
                       "src/import/NullImportService.h",
                       "src/import/NullImportService.cpp"):
            require(text, source, f"{source} в CMake")

    check("CMake: файлы импорта подключены", cmake)


def check_interface():
    text = read(INTERFACE_H)

    def api():
        require(text, "struct ImportResult", "структура результата")
        require(text, "class IImportService", "интерфейс")
        require(text, "virtual ~IImportService() = default;",
                "виртуальный деструктор")
        require(text, "virtual QString name() const = 0;", "name()")
        require(text, "virtual bool isAvailable() const = 0;",
                "isAvailable()")
        require(text, "virtual ImportResult importFromFile(const QString &path) = 0;",
                "importFromFile()")

    check("M10-02: интерфейс IImportService", api)


def check_null_service():
    header = read(NULL_H)
    body = read(NULL_CPP)

    def impl():
        require(header, "class NullImportService : public IImportService",
                "класс-реализация")
        require(body, "bool NullImportService::isAvailable() const",
                "isAvailable")
        require(body, "return false;", "импорт недоступен")
        require(body, "ImportResult NullImportService::importFromFile",
                "importFromFile")
        require(body, "result.ok = false;", "результат с ошибкой")
        require(body, "будет реализован позже", "пояснение пользователю")

    check("M10-02: пустая реализация NullImportService", impl)

    def no_database():
        for path in (NULL_H, NULL_CPP):
            text = read(path)
            assert "PersonRepository" not in text, \
                f"{path.name}: реализация не должна трогать базу"
            assert "Database::" not in text, \
                f"{path.name}: реализация не должна трогать базу"

    check("M10-02: пустая реализация не трогает БД", no_database)


def check_ui():
    main = read(MAIN_CPP)
    header = read(MAIN_H)

    def menu():
        require(main, 'm_importAction = fileMenu->addAction(tr("Импорт из Excel"))',
                "пункт «Импорт из Excel»")
        require(main, "m_importAction->setEnabled(false);", "пункт неактивен")
        require(main, "Импорт из Excel — будет реализовано позже",
                "tooltip «будет реализовано позже»")

    check("M10-01: неактивный пункт «Импорт из Excel»", menu)

    def wiring():
        require(main, "connect(m_importAction, &QAction::triggered,\n"
                      "            this, &MainWindow::onImportExcel);",
                "точка вызова в UI")
        require(header, "void onImportExcel();", "слот импорта")
        require(main, "m_importService = std::make_unique<NullImportService>();",
                "создание пустой реализации")
        body = between(main, "void MainWindow::onImportExcel()",
                       "void MainWindow::onSettings()")
        require(body, "m_importService->isAvailable()",
                "проверка доступности")
        require(body, "m_importService->importFromFile(fileName)",
                "вызов импорта")

    check("M10-02: точка вызова импорта в главном окне", wiring)


def main():
    print("Проверка заглушек будущего: импорт из Excel (M10-01/M10-02) без Qt")
    check_files()
    check_interface()
    check_null_service()
    check_ui()

    print(f"\nПроверок: {checks}, провалов: {len(failures)}")
    if failures:
        for item in failures:
            print(" -", item)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
