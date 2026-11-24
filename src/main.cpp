// Точка входа приложения «Ветераны».
// M1-03: Fusion-стиль, политика High-DPI, метаданные приложения,
// запуск пустого главного окна. M1-04/M1-05: пути и логирование.
// Содержимое окна — M3.

#include <QApplication>
#include <QDebug>
#include <QDir>
#include <QGuiApplication>
#include <QLocale>
#include <QMessageBox>

#include "core/AppPaths.h"
#include "core/Logging.h"
#include "core/Theme.h"
#include "core/version.h"
#include "data/BackupService.h"
#include "data/Database.h"
#include "ui/MainWindow.h"

// Ресурсы (миграции, темы, иконки) лежат в статической библиотеке
// veterans_app, поэтому статический инициализатор нужно вызвать явно.
static void initResources()
{
    Q_INIT_RESOURCE(app);
}

int main(int argc, char *argv[])
{
    initResources();

    // Дробное масштабирование без округления: 125 %, 150 %, 175 % и т. п.
    // В Qt 6 Hi-DPI включён всегда, управлять нужно только политикой округления.
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(
        Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);

    QApplication app(argc, argv);

    // Fusion — нейтральная база; светлая тема из настроек применяется ниже.
    QApplication::setStyle(QStringLiteral("Fusion"));

    // Имя в файловой системе — латиницей (пути QSettings), отображаемое — по-русски.
    QApplication::setApplicationName(QStringLiteral("Veterans"));
    QApplication::setApplicationDisplayName(QStringLiteral("Ветераны"));
    QApplication::setApplicationVersion(QStringLiteral(APP_VERSION_STRING));
    QApplication::setOrganizationName(QStringLiteral("Veterans"));
    QApplication::setOrganizationDomain(QStringLiteral("veterans.local"));

    QLocale::setDefault(QLocale(QLocale::Russian, QLocale::Russia));

    // Portable-режим: data/, logs/, backups/ и settings.ini рядом с exe,
    // либо в профиле пользователя, если каталог программы только для чтения.
    QString pathError;
    if (!AppPaths::init(&pathError)) {
        QMessageBox::critical(nullptr, QStringLiteral("Ветераны"), pathError);
        return 1;
    }

    Logging::init();
    Theme::apply(Theme::currentId());
    qInfo() << "Запуск" << QCoreApplication::applicationVersion()
            << "режим:" << (AppPaths::isPortable() ? "portable" : "user-profile")
            << "каталог:" << AppPaths::rootDir()
            << "тема:" << Theme::currentId();

    const Database::OpenResult dbResult =
        Database::open(AppPaths::databaseFile());
    if (!dbResult.ok) {
        qCritical() << "Не удалось открыть базу данных:" << dbResult.error;
        QMessageBox::critical(nullptr, QStringLiteral("Ветераны"), dbResult.error);
        return 1;
    }
    qInfo() << "База данных:"
            << QDir::toNativeSeparators(AppPaths::databaseFile())
            << (dbResult.created ? "(создана)" : "(открыта)");

    // Автобэкап не должен мешать запуску: сбой только пишется в лог.
    QString backupError;
    if (!BackupService::createDaily(&backupError))
        qWarning().noquote() << "Автобэкап не создан:" << backupError;

    QObject::connect(qApp, &QCoreApplication::aboutToQuit, [] {
        Database::close();
    });

    MainWindow window;
    window.show();

    return app.exec();
}
