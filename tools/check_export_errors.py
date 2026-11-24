#!/usr/bin/env python3
"""Проверка обработки ошибок экспорта (M7-05) без Qt.

Проверяет понятные сообщения и безопасную запись для трёх типовых проблем:
файл занят другой программой, нет прав на запись, слишком длинный путь.
Общая логика вынесена в `src/report/ExportFileGuard.{h,cpp}`.

    python3 tools/check_export_errors.py
"""

from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
GUARD_H = ROOT / "src" / "report" / "ExportFileGuard.h"
GUARD_CPP = ROOT / "src" / "report" / "ExportFileGuard.cpp"
EXCEL = ROOT / "src" / "report" / "ExcelExporter.cpp"
PDF = ROOT / "src" / "report" / "ListPdfExporter.cpp"
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


def check_files():
    def exists():
        for path in (GUARD_H, GUARD_CPP, EXCEL, PDF):
            assert path.exists(), f"нет {path.relative_to(ROOT)}"

    check("файлы ExportFileGuard и экспортёров существуют", exists)

    def cmake():
        text = read(CMAKE)
        for source in ("src/report/ExportFileGuard.h",
                       "src/report/ExportFileGuard.cpp"):
            require(text, source, f"{source} в CMake")

    check("CMake: ExportFileGuard подключён", cmake)


def check_long_path():
    src = read(GUARD_CPP)

    def max_path():
        require(src, "#ifdef Q_OS_WIN", "проверка только для Windows")
        require(src, "native.size() >= 260", "предел MAX_PATH")
        require(src, 'startsWith(QStringLiteral("\\\\\\\\?\\\\"))',
                "исключение для \\\\?\\-путей")
        require(src, "длиннее 260 символов", "понятное сообщение о пути")

    check("длинный путь: предел 260 символов и \\\\?\\", max_path)

    def component():
        require(src, "part.size() > 255", "предел имени компонента 255")
        require(src, "Слишком длинное имя папки или файла",
                "сообщение о длинном имени")

    check("длинное имя папки/файла: предел 255 символов", component)


def check_permissions():
    src = read(GUARD_CPP)

    def directory():
        require(src, "!dir.exists()", "проверка существования каталога")
        require(src, "!QFileInfo(dir.absolutePath()).isWritable()",
                "проверка прав на каталог")
        require(src, "Нет прав на запись в каталог", "сообщение о правах")

    check("нет прав: каталог существует и доступен для записи", directory)

    def target():
        require(src, "target.exists() && target.isDir()",
                "цель не должна быть каталогом")
        require(src, "target.exists() && !target.isWritable()",
                "проверка защиты файла от записи")
        require(src, "защищён от записи", "сообщение о защищённом файле")

    check("цель: не каталог и не защищена от записи", target)


def check_busy_file():
    src = read(GUARD_CPP)

    def probe():
        require(src, "QFile probe(filePath);", "пробное открытие файла")
        require(src, "!probe.open(QIODevice::ReadWrite)",
                "проверка блокировки на запись")
        require(src, "открыт в другой программе",
                "сообщение о занятом файле")
        require(src, "Закройте его и повторите", "подсказка пользователю")

    check("занятый файл: пробное открытие и понятная ошибка", probe)


def check_replace():
    src = read(GUARD_CPP)

    def backup():
        require(src, 'targetPath + QStringLiteral(".bak")',
                "страховочная копия .bak")
        require(src, "QFile::rename(targetPath, backupPath)",
                "перенос цели в .bak")
        require(src, "QFile::rename(tempPath, targetPath)",
                "перенос временного файла на место цели")
        require(src, "QFile::rename(backupPath, targetPath)",
                "откат при неудаче")
        require(src, "QFile::remove(backupPath)", "удаление .bak после успеха")

    check("замена файла: .bak, откат и очистка", backup)

    def errors():
        require(src, "Не удалось заменить файл", "ошибка замены")
        require(src, "Не удалось записать файл", "ошибка записи")

    check("замена файла: сообщения об ошибках", errors)


def check_exporters():
    excel = read(EXCEL)
    pdf = read(PDF)

    def validate():
        for name, src in (("Excel", excel), ("PDF", pdf)):
            require(src, "ExportFileGuard::validateTarget(filePath, &pathError)",
                    f"{name}: проверка цели перед записью")

    check("оба экспортёра проверяют цель до записи", validate)

    def temp_files():
        for name, src in (("Excel", excel), ("PDF", pdf)):
            require(src, 'filePath + QStringLiteral(".tmp")',
                    f"{name}: временный файл рядом с целью")
            assert src.count("QFile::remove(tempPath)") >= 3, \
                f"{name}: временный файл должен удаляться при ошибках"

    check("оба экспортёра пишут во временный файл и убирают его", temp_files)

    def replace():
        require(excel, "ExportFileGuard::replaceFile(tempPath, filePath,",
                "Excel: общая безопасная замена")
        require(pdf, "ExportFileGuard::replaceFile(tempPath, filePath,",
                "PDF: общая безопасная замена")

    check("оба экспортёра используют общую замену файла", replace)


def main():
    print("Проверка обработки ошибок экспорта (M7-05) без Qt")
    check_files()
    check_long_path()
    check_permissions()
    check_busy_file()
    check_replace()
    check_exporters()

    print(f"\nПроверок: {checks}, провалов: {len(failures)}")
    if failures:
        for item in failures:
            print(" -", item)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
