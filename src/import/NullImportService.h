#pragma once

#include "import/IImportService.h"

/// Пустая реализация импорта (M10-02).
///
/// Всегда сообщает, что функция ещё не реализована, и не трогает базу:
/// пункт меню остаётся неактивным, а прямой вызов безопасен.
class NullImportService : public IImportService
{
public:
    QString name() const override;
    bool isAvailable() const override;
    ImportResult importFromFile(const QString &path) override;
};
