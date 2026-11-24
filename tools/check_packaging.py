#!/usr/bin/env python3
"""Проверка упаковки (M11-01…M11-03) без Qt и Windows.

M11-01: portable-zip через windeployqt.
M11-02: сценарий Inno Setup.
M11-03: манифест DPI, версия и иконка в ресурсах Windows.

    python3 tools/check_packaging.py
"""

from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
CMAKE = ROOT / "CMakeLists.txt"
PACKAGE_PS1 = ROOT / "tools" / "package-portable.ps1"
BUILD_PS1 = ROOT / "build.ps1"
ISS = ROOT / "installer" / "veterans.iss"
MANIFEST = ROOT / "resources" / "veterans.manifest"
RC_IN = ROOT / "resources" / "veterans.rc.in"
ICON = ROOT / "resources" / "icons" / "veterans.ico"

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


def check_portable():
    text = read(PACKAGE_PS1)

    def script():
        require(text, "windeployqt", "вызов windeployqt")
        require(text, "--compiler-runtime", "среда выполнения компилятора")
        require(text, "Compress-Archive", "создание zip")
        require(text, "Veterans-portable.zip", "имя архива")
        require(text, "Veterans.exe", "копирование exe")
        for folder in ("data", "logs", "backups"):
            require(text, folder, f"каталог {folder}")

    check("M11-01: portable-zip через windeployqt", script)

    def executable():
        require(read(CMAKE), "WIN32_EXECUTABLE ON",
                "GUI-приложение без консоли")

    check("M11-01: сборка GUI-приложения", executable)


def check_installer():
    text = read(ISS)

    def sections():
        for section in ("[Setup]", "[Languages]", "[Tasks]", "[Files]",
                        "[Icons]", "[Run]", "[UninstallDelete]"):
            require(text, section, f"секция {section}")

    check("M11-02: секции сценария Inno Setup", sections)

    def setup():
        require(text, "AppId={{", "AppId")
        require(text, "DefaultDirName={autopf}\\Veterans", "каталог установки")
        require(text, "OutputBaseFilename=Veterans-Setup-", "имя установщика")
        require(text, "UninstallDisplayIcon=", "иконка в списке программ")
        require(text, "PrivilegesRequired=admin", "установка с правами")
        require(text, 'MessagesFile: "compiler:Languages\\Russian.isl"',
                "русский язык")

    check("M11-02: параметры установки", setup)

    def shortcuts():
        require(text, "{group}\\{#AppName}", "ярлык в меню «Пуск»")
        require(text, "{autodesktop}\\{#AppName}", "ярлык на рабочем столе")
        require(text, "desktopicon", "задача ярлыка")
        require(text, "{uninstallexe}", "ярлык удаления")
        require(text, "Flags: nowait postinstall skipifsilent",
                "запуск после установки")

    check("M11-02: ярлыки, деинсталляция и запуск", shortcuts)

    def data_survives():
        require(text, "[UninstallDelete]", "очистка каталогов")
        require(text, "LOCALAPPDATA", "данные вне каталога установки")
        require(text, 'Flags: ignoreversion recursesubdirs createallsubdirs',
                "копирование файлов сборки")

    check("M11-02: данные переживают обновление", data_survives)


def check_manifest():
    def files():
        for path in (MANIFEST, RC_IN, ICON):
            assert path.exists(), f"нет {path.relative_to(ROOT)}"
        header = ICON.read_bytes()[:6]
        assert header[:4] == b"\x00\x00\x01\x00", "файл не похож на .ico"

    check("M11-03: манифест, ресурс и иконка существуют", files)

    def manifest():
        text = read(MANIFEST)
        require(text, 'manifestVersion="1.0"', "версия манифеста")
        require(text, 'level="asInvoker"', "уровень прав")
        require(text, "<dpiAware", "осведомлённость о DPI")
        require(text, "PerMonitorV2", "Per-Monitor V2")
        require(text, "<longPathAware", "длинные пути")

    check("M11-03: манифест DPI и прав", manifest)

    def resource():
        text = read(RC_IN)
        require(text, "VERSIONINFO", "блок версии")
        require(text, "FILEVERSION @PROJECT_VERSION_MAJOR@",
                "версия из CMake")
        require(text, "IDI_ICON1 ICON", "иконка")
        require(text, "1 24", "манифест RT_MANIFEST")
        require(text, "OriginalFilename", "имя файла в свойствах")
        require(text, "Veterans.exe", "исполняемый файл")

    check("M11-03: ресурс версии и иконки", resource)

    def cmake():
        text = read(CMAKE)
        require(text, "if(WIN32)", "ресурсы только для Windows")
        require(text, "enable_language(RC)", "язык ресурсов")
        require(text, "configure_file(resources/veterans.rc.in",
                "генерация .rc")
        require(text, "veterans.ico", "путь к иконке")
        require(text, "veterans.manifest", "путь к манифесту")
        require(text, "target_sources(Veterans PRIVATE", "подключение .rc")

    check("M11-03: CMake собирает ресурсы", cmake)


def check_build_script():
    """M1-07: скрипт сборки и windeployqt. Запуск проверяется на Windows."""

    def exists():
        assert BUILD_PS1.exists(), "нет build.ps1 в корне репозитория"

    check("M1-07: build.ps1 существует", exists)

    text = read(BUILD_PS1)

    def params():
        require(text, "[CmdletBinding()]", "строгий режим параметров")
        require(text, '$ErrorActionPreference = "Stop"', "остановка при ошибке")
        require(text, '[string]$Preset = "release"', "пресет release по умолчанию")
        require(text, "[switch]$Tests", "опция сборки автотестов")
        require(text, "[switch]$NoDeploy", "опция сборки без windeployqt")

    check("M1-07: параметры build.ps1", params)

    def build():
        require(text, "Get-Command cmake", "проверка наличия cmake")
        require(text, "cmake @configureArgs", "настройка CMake пресетом")
        require(text, "cmake --build --preset", "сборка пресетом")
        require(text, "VETERANS_BUILD_TESTS=ON", "включение автотестов")
        require(text, "Veterans.exe", "поиск собранного exe")
        assert text.count("$LASTEXITCODE -ne 0") >= 2, \
            "коды возврата cmake не проверяются"

    check("M1-07: сборка через CMake-пресет", build)

    def deploy():
        require(text, "windeployqt.exe", "windeployqt")
        require(text, "--compiler-runtime", "среда выполнения компилятора")
        require(text, "--no-translations", "без лишних переводов")
        require(text, "[string]$QtBin", "путь к Qt через параметр")
        require(text, "QT_QPA_PLATFORM", "offscreen для автотестов")

    check("M1-07: windeployqt и прогон тестов", deploy)


def main():
    print("Проверка упаковки (M11-01…M11-03) и сборки (M1-07) без Qt и Windows")
    check_build_script()
    check_portable()
    check_installer()
    check_manifest()

    print(f"\nПроверок: {checks}, провалов: {len(failures)}")
    if failures:
        for item in failures:
            print(" -", item)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
