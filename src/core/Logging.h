#pragma once

#include <QString>

/// Логирование в <root>/logs/app.log с ротацией по размеру.
/// Пишется всё, что проходит через qDebug/qInfo/qWarning/qCritical/qFatal,
/// включая сообщения Qt. Файл ограничен 2 МБ, хранится 5 архивных копий.
class Logging
{
public:
    /// Устанавливает обработчик сообщений. Вызывать после AppPaths::init().
    static void init();

    static QString logFilePath();
};
