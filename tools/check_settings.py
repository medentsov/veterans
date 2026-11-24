#!/usr/bin/env python3
"""Проверка настроек и светлых тем (M9-01…M9-06) без Qt.

Проверяет диалог настроек (тема, реквизиты, файл БД, бэкапы), четыре QSS-темы,
применение темы без перезапуска, заглушку «Классификатор улиц», смену файла БД
с перезапуском, сброс настроек и сохранение темы/окна между запусками.

    python3 tools/check_settings.py
"""

from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SETTINGS_H = ROOT / "src" / "ui" / "SettingsDialog.h"
SETTINGS_CPP = ROOT / "src" / "ui" / "SettingsDialog.cpp"
THEME_H = ROOT / "src" / "core" / "Theme.h"
THEME_CPP = ROOT / "src" / "core" / "Theme.cpp"
MAIN_CPP = ROOT / "src" / "ui" / "MainWindow.cpp"
APP_MAIN = ROOT / "src" / "main.cpp"
PRINT_SERVICE = ROOT / "src" / "report" / "PrintService.cpp"
APPPATHS_H = ROOT / "src" / "core" / "AppPaths.h"
APPPATHS_CPP = ROOT / "src" / "core" / "AppPaths.cpp"
QRC = ROOT / "resources" / "app.qrc"
CMAKE = ROOT / "CMakeLists.txt"

THEMES = ("light-warm", "light-cool", "light-green", "light-contrast")

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
        for path in (SETTINGS_H, SETTINGS_CPP, THEME_H, THEME_CPP):
            assert path.exists(), f"нет {path.relative_to(ROOT)}"

    check("файлы SettingsDialog и Theme существуют", exists)

    def cmake():
        text = read(CMAKE)
        for source in ("src/core/Theme.cpp", "src/ui/SettingsDialog.cpp",
                       "src/ui/SettingsDialog.h"):
            require(text, source, f"{source} в CMake")

    check("CMake: Theme и SettingsDialog подключены", cmake)


def check_dialog():
    text = read(SETTINGS_CPP)

    def tabs():
        for tab in ('tr("Общие")', 'tr("База данных")',
                    'tr("Резервные копии")'):
            require(text, f"tabs->addTab(create", "вкладки настроек")
            require(text, tab, f"вкладка {tab}")

    check("диалог: общие, база данных, резервные копии", tabs)

    def theme():
        require(text, "m_theme->addItem(info.name, info.id);",
                "список тем из Theme::all")
        require(text, "m_theme->findData(Theme::currentId())",
                "текущая тема")
        require(text,
                "Theme::apply(m_theme->currentData().toString());",
                "применение темы без перезапуска")

    check("тема: список, текущая и живое применение", theme)

    def organization():
        require(text,
                'settings.value(QStringLiteral("Print/organizationName"))',
                "чтение наименования учреждения")
        require(text,
                'settings.setValue(QStringLiteral("Print/organizationName"),',
                "запись наименования учреждения")

    check("реквизиты: наименование учреждения для шапки", organization)

    def database():
        require(text, "AppPaths::databaseFile()", "путь к файлу БД")
        require(text, "m_databasePath->setReadOnly(true)",
                "файл БД только для чтения")

    check("база данных: путь к файлу", database)

    def backups():
        require(text, "BackupService::list()", "список копий")
        require(text, "BackupService::createNow(&error)",
                "создание копии вручную")
        require(text, "BackupService::kKeepFiles", "лимит хранения")
        require(text, "AppPaths::backupsDir()", "папка копий")
        require(text, "QDesktopServices::openUrl(", "открытие папки")

    check("резервные копии: список, создание и папка", backups)


def check_save():
    text = read(SETTINGS_CPP)

    def save():
        require(text, 'settings.setValue(QStringLiteral("Appearance/theme"),',
                "сохранение темы")
        require(text, "settings.sync();", "сброс настроек на диск")

    check("сохранение темы в settings.ini", save)

    def cancel():
        require(text, "void SettingsDialog::reject()", "обработчик отмены")
        require(text, "Theme::apply(m_initialTheme);",
                "откат темы при отмене")

    check("отмена возвращает исходную тему", cancel)

    def main_window():
        main = read(MAIN_CPP)
        require(main, '#include "ui/SettingsDialog.h"',
                "подключение диалога")
        require(main, "SettingsDialog dialog(this);", "создание диалога")
        require(main, "dialog.exec() != QDialog::Accepted",
                "обработка результата")

    check("главное окно открывает настройки", main_window)


def check_themes():
    def files():
        for theme in THEMES:
            path = ROOT / "resources" / "themes" / f"{theme}.qss"
            assert path.exists(), f"нет {path.relative_to(ROOT)}"
            text = path.read_text(encoding="utf-8")
            assert "QWidget" in text, f"{theme}: пустой или не QSS"

    check("четыре QSS-файла светлых тем", files)

    def qrc():
        text = read(QRC)
        for theme in THEMES:
            require(text, f"themes/{theme}.qss", f"{theme}.qss в app.qrc")

    check("app.qrc: все четыре темы", qrc)

    def registry():
        text = read(THEME_CPP)
        for theme in THEMES:
            require(text, f'QStringLiteral("{theme}")',
                    f"идентификатор {theme}")
        require(text, "QStringLiteral(\"Appearance/theme\")",
                "ключ Appearance/theme")
        require(text, 'QStringLiteral(":/themes/%1.qss")',
                "загрузка QSS из ресурсов")
        require(text, "qApp->setStyleSheet(styleSheet(id));",
                "применение стилей")

    check("Theme: четыре темы и загрузка из ресурсов", registry)

    def startup():
        text = read(APP_MAIN)
        require(text, '#include "core/Theme.h"', "подключение Theme")
        require(text, "Theme::apply(Theme::currentId());",
                "применение темы при запуске")

    check("тема применяется при запуске", startup)

    def print_header():
        text = read(PRINT_SERVICE)
        require(text,
                'settings.value(QStringLiteral("Print/organizationName"))',
                "чтение наименования для печати")
        require(text, "const QString organization = organizationName();",
                "передача в карточку")
        require(text, "organization, &error", "шапка карточки")

    check("наименование из настроек попадает в печать", print_header)


def check_street_classifier():
    text = read(SETTINGS_CPP)

    def checkbox():
        require(text, "m_streetClassifier = new QCheckBox(", "чекбокс")
        require(text, "Классификатор улиц", "подпись чекбокса")
        require(text, "(TBD)", "пометка TBD")
        require(text, "m_streetClassifier->setEnabled(false);",
                "чекбокс неактивен")
        require(text, "m_streetClassifier->setToolTip(", "пояснение")

    check("M9-03: «Классификатор улиц» неактивен с пометкой TBD", checkbox)


def check_database_switch():
    paths_header = read(APPPATHS_H)
    paths_cpp = read(APPPATHS_CPP)

    def paths_api():
        require(paths_header, "static QString databaseFile();",
                "databaseFile()")
        require(paths_header,
                "static void setDatabaseFile(const QString &path);",
                "setDatabaseFile()")
        require(paths_cpp, 'settings.value(QStringLiteral("Database/path"))',
                "чтение выбранного файла")
        require(paths_cpp, 'settings.setValue(QStringLiteral("Database/path"),',
                "запись выбранного файла")
        require(paths_cpp, 'settings.remove(QStringLiteral("Database/path"));',
                "возврат к умолчанию")
        require(paths_cpp, "QDir::cleanPath", "нормализация пути")

    check("M9-04: AppPaths хранит выбранный файл БД", paths_api)

    def startup():
        require(read(APP_MAIN), "Database::open(AppPaths::databaseFile());",
                "открытие выбранного файла при запуске")

    check("M9-04: запуск открывает выбранный файл БД", startup)

    def dialog():
        require(text := read(SETTINGS_CPP),
                "QFileDialog::getOpenFileName(", "выбор файла")
        require(text, "AppPaths::setDatabaseFile(path);",
                "сохранение выбранного файла")
        require(text, "приложение будет перезапущено",
                "предупреждение о перезапуске")
        require(text, "m_restartRequested = true;", "запрос перезапуска")

    check("M9-04: диалог меняет файл с предупреждением", dialog)

    def restart():
        require(read(SETTINGS_H), "bool restartRequested() const",
                "признак перезапуска в диалоге")
        main = read(MAIN_CPP)
        require(main, "if (dialog.restartRequested())",
                "проверка признака")
        require(main, "QProcess::startDetached(",
                "запуск нового экземпляра")
        require(main, "&QCoreApplication::aboutToQuit",
                "запуск после закрытия базы")
        require(main, "qApp->quit();", "завершение текущего экземпляра")

    check("M9-04: перезапуск после закрытия базы", restart)


def check_reset():
    text = read(SETTINGS_CPP)

    def reset():
        require(text, "void SettingsDialog::resetSettings()",
                "обработчик сброса")
        require(text, "tr(\"Сбросить настройки\")", "кнопка сброса")
        require(text, "tr(\"Сброс настроек\")", "подтверждение сброса")
        require(text, "settings.clear();", "очистка settings.ini")
        require(text, "Theme::defaultId()", "тема по умолчанию")
        require(text, "m_restartRequested = false;",
                "сброс отменяет перезапуск")

    check("M9-05: сброс настроек к умолчаниям", reset)

    def header():
        require(read(SETTINGS_H), "void resetSettings();",
                "объявление обработчика")

    check("M9-05: обработчик объявлен в заголовке", header)


def check_window_state():
    main = read(MAIN_CPP)

    def restore():
        require(main, "void MainWindow::restoreWindowState()",
                "восстановление состояния окна")
        require(main, 'settings.beginGroup(QStringLiteral("MainWindow"))',
                "группа MainWindow")
        require(main, "restoreGeometry(geometry)", "геометрия окна")
        require(main, "m_header->restoreState(headerState)",
                "состояние шапки таблицы")

    check("M9-06: восстановление окна при запуске", restore)

    def save():
        require(main, "void MainWindow::saveWindowState()",
                "сохранение состояния окна")
        require(main, "saveGeometry()", "геометрия")
        require(main, "m_header->saveState()", "шапка таблицы")
        body = main[main.index("void MainWindow::closeEvent"):]
        require(body, "saveWindowState();",
                "сохранение при закрытии окна")

    check("M9-06: сохранение окна при закрытии", save)

    def theme():
        require(read(THEME_CPP), 'QStringLiteral("Appearance/theme")',
                "сохранённая тема в settings.ini")
        require(read(APP_MAIN), "Theme::apply(Theme::currentId());",
                "применение сохранённой темы при запуске")

    check("M9-06: тема переживает перезапуск", theme)


def main():
    print("Проверка настроек и светлых тем (M9-01…M9-06) без Qt")
    check_files()
    check_dialog()
    check_save()
    check_themes()
    check_street_classifier()
    check_database_switch()
    check_reset()
    check_window_state()

    print(f"\nПроверок: {checks}, провалов: {len(failures)}")
    if failures:
        for item in failures:
            print(" -", item)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
