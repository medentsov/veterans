#include "core/Theme.h"

#include "core/AppPaths.h"

#include <QApplication>
#include <QFile>
#include <QSettings>

namespace {

const QVector<Theme::Info> &themes()
{
    static const QVector<Theme::Info> list{
        {QStringLiteral("light-warm"), QStringLiteral("Тёплая светлая")},
        {QStringLiteral("light-cool"), QStringLiteral("Холодная светлая")},
        {QStringLiteral("light-green"), QStringLiteral("Зелёная светлая")},
        {QStringLiteral("light-contrast"), QStringLiteral("Контрастная светлая")},
    };
    return list;
}

bool isKnown(const QString &id)
{
    for (const Theme::Info &info : themes()) {
        if (info.id == id)
            return true;
    }
    return false;
}

} // namespace

QVector<Theme::Info> Theme::all()
{
    return themes();
}

QString Theme::defaultId()
{
    return QStringLiteral("light-warm");
}

QString Theme::currentId()
{
    QSettings settings(AppPaths::settingsFile(), QSettings::IniFormat);
    const QString saved =
        settings.value(QStringLiteral("Appearance/theme")).toString();
    return isKnown(saved) ? saved : defaultId();
}

void Theme::setCurrentId(const QString &id)
{
    QSettings settings(AppPaths::settingsFile(), QSettings::IniFormat);
    settings.setValue(QStringLiteral("Appearance/theme"),
                      isKnown(id) ? id : defaultId());
    settings.sync();
}

QString Theme::styleSheet(const QString &id)
{
    const QString themeId = isKnown(id) ? id : defaultId();
    QFile file(QStringLiteral(":/themes/%1.qss").arg(themeId));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    return QString::fromUtf8(file.readAll());
}

void Theme::apply(const QString &id)
{
    if (qApp)
        qApp->setStyleSheet(styleSheet(id));
}
