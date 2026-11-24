#pragma once

#include <QString>

/// Проверки и безопасная замена файлов экспорта (M7-05).
///
/// Общие правила для Excel и PDF: понятные ошибки для занятого файла, отсутствия
/// прав и слишком длинного пути. Сами экспортёры пишут во временный файл и
/// подменяют цель только после успешной записи (см. replaceFile).
class ExportFileGuard
{
public:
    /// Проверяет, что в файл можно писать. Не создаёт и не изменяет файл.
    static bool validateTarget(const QString &filePath, QString *error = nullptr);

    /// Атомарно заменяет target файлом temp: цель уходит в .bak, затем
    /// возвращается при неудаче. Используется после успешной записи.
    static bool replaceFile(const QString &tempPath, const QString &targetPath,
                            QString *error = nullptr);
};
