#pragma once

#include <QString>
#include <QStringList>

/// Резервные копии файла БД.
///
/// База работает в режиме WAL, поэтому копировать .db «как файл» при открытом
/// соединении нельзя. Снимок делается через `VACUUM INTO` — SQLite сам
/// собирает согласованную копию с учётом журнала.
class BackupService
{
public:
    static constexpr int kKeepFiles = 10;

    /// Создаёт копию, если за сегодня её ещё нет. Ошибка не критична для
    /// работы приложения — вызывающий сам решает, показывать её или писать в лог.
    static bool createDaily(QString *error = nullptr);

    /// Принудительная копия (кнопка в настройках).
    static bool createNow(QString *error = nullptr);

    /// Список копий, новые сверху.
    static QStringList list();

    /// Восстанавливает базу из копии. Перед вызовом база должна быть закрыта
    /// (Database::close()). Текущий файл сохраняется как
    /// `veterans-before-restore-*.db`, чтобы откат можно было отменить.
    static bool restore(const QString &backupPath,
                        const QString &targetDatabasePath,
                        QString *error = nullptr);

private:
    static QString makeFileName(const QString &stamp);
    static void prune();
};
