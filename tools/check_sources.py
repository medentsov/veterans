#!/usr/bin/env python3
"""Проверка полноты репозитория: исходники на диске и в git.

Ловит потерю файлов, которая уже случалась: правило `data/` в `.gitignore`
(без якоря в корне) исключило весь каталог `src/data/`, и слой БД не попал
в коммит — проект не собирался на чистой машине.

Проверяется:

1. Каждый файл, на который ссылается `CMakeLists.txt` (и `tests/CMakeLists.txt`),
   существует на диске.
2. Каждый исходник проекта (`src`, `tests`, `tools`, `docs`, `resources`,
   `installer`, корневые файлы) отслеживается git.
3. Ни один исходник не попадает под правила `.gitignore`.

    python3 tools/check_sources.py
"""

import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
CMAKE = ROOT / "CMakeLists.txt"
TESTS_CMAKE = ROOT / "tests" / "CMakeLists.txt"

# Каталоги проекта, содержимое которых обязано быть в git.
PROJECT_DIRS = ("src", "tests", "tools", "docs", "resources", "installer")

# Корневые файлы, без которых сборка/документация неполны.
ROOT_FILES = (
    "AGENTS.md",
    "CMakeLists.txt",
    "CMakePresets.json",
    "README.md",
    "build.ps1",
    ".gitignore",
)

# Служебные файлы, которые в git не нужны.
SKIP_NAMES = {"__pycache__", ".git", "build", "build-verify", "dist", "out"}

SOURCE_SUFFIXES = {
    ".cpp", ".h", ".hpp", ".py", ".md", ".sql", ".qss", ".qrc", ".in",
    ".ps1", ".iss", ".manifest", ".ico", ".png", ".txt",
}

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


def git(*args, cwd=ROOT):
    return subprocess.run(
        ["git", *args], cwd=cwd, capture_output=True, text=True, check=False
    )


def tracked_files():
    result = git("ls-files", "-z")
    if result.returncode != 0:
        return None
    return {p for p in result.stdout.split("\0") if p}


def project_files():
    """Все файлы, которые обязаны быть в репозитории (относительные пути)."""
    files = set()
    for name in ROOT_FILES:
        if (ROOT / name).exists():
            files.add(name)
    for directory in PROJECT_DIRS:
        base = ROOT / directory
        if not base.is_dir():
            continue
        for path in base.rglob("*"):
            if not path.is_file():
                continue
            if any(part in SKIP_NAMES for part in path.relative_to(ROOT).parts):
                continue
            if path.suffix.lower() not in SOURCE_SUFFIXES:
                continue
            files.add(path.relative_to(ROOT).as_posix())
    return files


def cmake_referenced_sources():
    """Пути из CMakeLists.txt: src/…, tests/…, resources/… и т. п."""
    pattern = re.compile(
        r"(?<![\w/])((?:src|tests|resources|installer)/[\w./-]+"
        r"\.(?:cpp|h|hpp|qrc|in|sql|qss|png|ico|manifest|iss|ps1|txt))"
    )
    found = set()
    for path in (CMAKE, TESTS_CMAKE):
        if not path.exists():
            continue
        found.update(pattern.findall(path.read_text(encoding="utf-8")))
    return found


def check_cmake_paths():
    def exists():
        referenced = cmake_referenced_sources()
        assert len(referenced) >= 30, f"слишком мало путей в CMake: {len(referenced)}"
        missing = sorted(p for p in referenced if not (ROOT / p).exists())
        assert not missing, "нет на диске: " + ", ".join(missing)

    check("CMakeLists: все указанные файлы есть на диске", exists)


def check_git_completeness():
    tracked = tracked_files()

    def has_git():
        assert tracked is not None, "не удалось получить список файлов git"

    check("git доступен", has_git)

    if tracked is None:
        return

    def tracked_all():
        expected = project_files()
        missing = sorted(expected - tracked)
        assert not missing, (
            f"не в git: {len(missing)} файл(ов): " + ", ".join(missing[:12])
        )

    check("исходники проекта отслеживаются git", tracked_all)

    def not_ignored():
        expected = sorted(project_files() - tracked)
        ignored = []
        for rel in expected:
            if git("check-ignore", "-q", rel).returncode == 0:
                ignored.append(rel)
        assert not ignored, "игнорируются .gitignore: " + ", ".join(ignored[:12])

    check("исходники не попадают под .gitignore", not_ignored)

    def ignore_rules():
        text = (ROOT / ".gitignore").read_text(encoding="utf-8")
        rules = [
            line.strip() for line in text.splitlines()
            if line.strip() and not line.strip().startswith("#")
        ]
        # Каталоги данных приложения в корне — с якорем, иначе под правило
        # попадают src/data/ и другие одноимённые каталоги.
        for name in ("data/", "logs/", "backups/"):
            assert name not in rules, (
                f"правило '{name}' без якоря: замените на '/{name}'"
            )
        assert "/data/" in rules and "/logs/" in rules and "/backups/" in rules, \
            "нет корневых правил /data/, /logs/, /backups/"

    check(".gitignore: каталоги данных только в корне", ignore_rules)


def main():
    print("Проверка полноты репозитория (CMake ↔ диск ↔ git)")
    if not (ROOT / ".git").exists():
        print("  — не git-репозиторий: проверки git пропущены")
    check_cmake_paths()
    check_git_completeness()

    print(f"\nПроверок: {checks}, провалов: {len(failures)}")
    if failures:
        for item in failures:
            print(" -", item)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
