#!/usr/bin/env python3
"""Проверка README и отсутствия WinAPI (M10-04/M11-05) без Qt.

M10-04: в README есть раздел про Linux/AppImage, а в исходниках нет WinAPI.
M11-05: README описывает запуск, расположение БД, бэкап и типовые проблемы.

    python3 tools/check_docs.py
"""

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
README = ROOT / "README.md"
SRC = ROOT / "src"

FORBIDDEN = (
    r"\bwindows\.h\b",
    r"#include\s*<windows",
    r"\bWinMain\b",
    r"\bHWND\b",
    r"\bHINSTANCE\b",
    r"__declspec",
    r"\bWINAPI\b",
)

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


def source_files():
    for suffix in ("*.h", "*.cpp"):
        yield from SRC.rglob(suffix)


def check_readme():
    def exists():
        assert README.exists(), "нет README.md"
        assert len(read(README).strip()) > 500, "README слишком короткий"

    check("README.md существует", exists)

    def launch():
        text = read(README)
        require(text, "## Запуск", "раздел «Запуск»")
        require(text, "portable", "описание portable-режима")
        require(text, "%LOCALAPPDATA%", "путь в профиле пользователя")

    check("M11-05: README — запуск", launch)

    def database():
        text = read(README)
        require(text, "## Расположение данных", "раздел о данных")
        require(text, "data/veterans.db", "файл базы данных")
        require(text, "settings.ini", "файл настроек")
        require(text, "Сменить файл", "смена файла БД")

    check("M11-05: README — расположение БД", database)

    def backup():
        text = read(README)
        require(text, "## Резервное копирование", "раздел о копиях")
        require(text, "VACUUM INTO", "механизм копии")
        require(text, "10", "лимит хранения копий")

    check("M11-05: README — резервное копирование", backup)

    def problems():
        text = read(README)
        require(text, "## Типовые проблемы", "раздел о проблемах")
        require(text, "Не удалось открыть базу", "проблема с БД")
        require(text, "Файл занят", "занятый файл экспорта")
        require(text, "масштаб DPI", "масштабирование")

    check("M11-05: README — типовые проблемы", problems)


def check_no_winapi():
    def readme_note():
        text = read(README)
        require(text, "## Linux и AppImage", "раздел про Linux/AppImage")
        require(text, "AppImage", "упоминание AppImage")
        require(text, "WinAPI", "пояснение про WinAPI")
        require(text, "windows.h", "перечень отсутствующих заголовков")

    check("M10-04: README — заметка о Linux/AppImage", readme_note)

    def sources():
        problems = []
        for path in source_files():
            text = read(path)
            for pattern in FORBIDDEN:
                if re.search(pattern, text):
                    problems.append(f"{path.relative_to(ROOT)}: {pattern}")
        assert not problems, "найден WinAPI: " + ", ".join(problems)

    check("M10-04: в исходниках нет WinAPI", sources)


def main():
    print("Проверка README и отсутствия WinAPI (M10-04/M11-05) без Qt")
    check_readme()
    check_no_winapi()

    print(f"\nПроверок: {checks}, провалов: {len(failures)}")
    if failures:
        for item in failures:
            print(" -", item)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
