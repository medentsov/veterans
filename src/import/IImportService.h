#pragma once

#include <QString>
#include <QStringList>

/// Результат импорта записей из внешнего файла.
struct ImportResult
{
    bool ok = false;
    int imported = 0;      ///< сколько записей добавлено
    int skipped = 0;       ///< сколько строк пропущено
    QStringList warnings;  ///< замечания по строкам
    QString error;         ///< причина отказа, если ok == false
};

/// Интерфейс импорта из Excel (M10-02).
///
/// Готовая реализация появится в следующей версии. Сейчас UI работает
/// с пустой `NullImportService`, чтобы точка вызова и сборка были готовы
/// заранее и не требовали переделки интерфейса.
class IImportService
{
public:
    virtual ~IImportService() = default;

    /// Отображаемое название источника.
    virtual QString name() const = 0;

    /// Готов ли импорт к работе (выбор файла и разбор строк подключены).
    virtual bool isAvailable() const = 0;

    /// Импортирует записи из файла. При `isAvailable() == false`
    /// возвращает результат с пояснением в `error`.
    virtual ImportResult importFromFile(const QString &path) = 0;
};
