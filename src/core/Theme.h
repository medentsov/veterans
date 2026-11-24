#pragma once

#include <QString>
#include <QVector>

/// Светлые темы оформления (M9-02).
///
/// Идентификаторы тем совпадают с именами файлов `resources/themes/*.qss`.
/// Выбор хранится в `settings.ini` (`Appearance/theme`) и применяется
/// без перезапуска через `QApplication::setStyleSheet`.
namespace Theme {

struct Info
{
    QString id;
    QString name;
};

/// Четыре светлые темы в порядке показа в настройках.
QVector<Info> all();

QString defaultId();

/// Сохранённая тема или тема по умолчанию, если сохранённой нет.
QString currentId();

void setCurrentId(const QString &id);

/// Содержимое QSS из ресурсов; пустая строка, если файл недоступен.
QString styleSheet(const QString &id);

/// Применяет тему ко всему приложению. Пустой id — тема по умолчанию.
void apply(const QString &id);

} // namespace Theme
