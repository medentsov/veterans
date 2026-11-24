#include "import/NullImportService.h"

#include <QObject>

QString NullImportService::name() const
{
    return QObject::tr("Импорт из Excel");
}

bool NullImportService::isAvailable() const
{
    return false;
}

ImportResult NullImportService::importFromFile(const QString &path)
{
    Q_UNUSED(path);

    ImportResult result;
    result.ok = false;
    result.error = QObject::tr("Импорт из Excel будет реализован позже.");
    return result;
}
