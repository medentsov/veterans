#pragma once

#include <QString>

/// Пути приложения и portable-режим.
///
/// Если каталог с exe доступен для записи (флешка, «Рабочий стол», распакованный
/// zip) — все данные лежат рядом с программой. Если нет (Program Files) —
/// данные уходят в профиль пользователя, чтобы приложение работало без прав
/// администратора.
class AppPaths
{
public:
    enum class Mode {
        Portable,    ///< данные рядом с exe
        UserProfile  ///< данные в %LOCALAPPDATA%\Veterans (или ~/.local/share/Veterans)
    };

    /// Определяет режим и создаёт рабочие каталоги.
    /// Возвращает false и заполняет errorMessage, если каталоги создать нельзя.
    static bool init(QString *errorMessage = nullptr);

    static Mode mode();
    static bool isPortable();

    static QString rootDir();            ///< база для data/logs/backups/settings.ini
    static QString dataDir();
    static QString logsDir();
    static QString backupsDir();
    static QString settingsFile();       ///< <root>/settings.ini
    static QString defaultDatabaseFile();///< <root>/data/veterans.db

    /// Файл БД, выбранный в настройках, либо файл по умолчанию (M9-04).
    static QString databaseFile();

    /// Запоминает выбранный файл БД в settings.ini.
    /// Пустой путь или путь по умолчанию убирают настройку.
    static void setDatabaseFile(const QString &path);

private:
    static bool ensureDir(const QString &path, QString *errorMessage);
};
