#include "ui/SettingsDialog.h"

#include "core/AppPaths.h"
#include "core/Theme.h"
#include "data/BackupService.h"
#include "data/Database.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDesktopServices>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QStringList>
#include <QTabWidget>
#include <QUrl>
#include <QVBoxLayout>
#include <QWidget>

SettingsDialog::SettingsDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Настройки"));
    setWindowIcon(QIcon(QStringLiteral(":/icons/veterans-64.png")));
    setModal(true);
    resize(600, 460);

    m_initialTheme = Theme::currentId();

    auto *tabs = new QTabWidget(this);
    tabs->addTab(createGeneralTab(), tr("Общие"));
    tabs->addTab(createDatabaseTab(), tr("База данных"));
    tabs->addTab(createBackupTab(), tr("Резервные копии"));

    auto *saveButton = new QPushButton(tr("Сохранить"), this);
    saveButton->setDefault(true);
    connect(saveButton, &QPushButton::clicked, this, &SettingsDialog::accept);

    auto *cancelButton = new QPushButton(tr("Отмена"), this);
    connect(cancelButton, &QPushButton::clicked, this, &SettingsDialog::reject);

    auto *buttonRow = new QHBoxLayout;
    buttonRow->addStretch();
    buttonRow->addWidget(saveButton);
    buttonRow->addWidget(cancelButton);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(10);
    layout->addWidget(tabs);
    layout->addLayout(buttonRow);
}

QWidget *SettingsDialog::createGeneralTab()
{
    auto *tab = new QWidget(this);
    auto *layout = new QVBoxLayout(tab);

    auto *form = new QFormLayout;

    m_theme = new QComboBox(tab);
    for (const Theme::Info &info : Theme::all())
        m_theme->addItem(info.name, info.id);

    const int themeIndex = m_theme->findData(Theme::currentId());
    if (themeIndex >= 0)
        m_theme->setCurrentIndex(themeIndex);
    connect(m_theme, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int) {
                Theme::apply(m_theme->currentData().toString());
            });
    form->addRow(tr("Тема оформления:"), m_theme);

    QSettings settings(AppPaths::settingsFile(), QSettings::IniFormat);
    m_organization = new QLineEdit(
        settings.value(QStringLiteral("Print/organizationName")).toString(),
        tab);
    m_organization->setMaxLength(200);
    m_organization->setPlaceholderText(QStringLiteral("Госпиталь ветеранов"));
    m_organization->setToolTip(
        tr("Печатается в шапке карточки ветерана"));
    form->addRow(tr("Наименование учреждения:"), m_organization);

    // Заглушка будущей функции: чекбокс виден, но недоступен (M9-03).
    m_streetClassifier = new QCheckBox(tr("Классификатор улиц (TBD)"), tab);
    m_streetClassifier->setEnabled(false);
    m_streetClassifier->setToolTip(
        tr("Функция будет реализована позже"));
    form->addRow(QString(), m_streetClassifier);

    layout->addLayout(form);

    auto *hint = new QLabel(
        tr("Тема применяется сразу, без перезапуска. "
           "Пустое наименование заменяется значением по умолчанию."),
        tab);
    hint->setWordWrap(true);
    layout->addWidget(hint);

    auto *resetButton = new QPushButton(tr("Сбросить настройки"), tab);
    connect(resetButton, &QPushButton::clicked,
            this, &SettingsDialog::resetSettings);

    auto *resetRow = new QHBoxLayout;
    resetRow->addWidget(resetButton);
    resetRow->addStretch();
    layout->addLayout(resetRow);
    layout->addStretch();

    return tab;
}

QWidget *SettingsDialog::createDatabaseTab()
{
    auto *tab = new QWidget(this);
    auto *layout = new QVBoxLayout(tab);

    auto *form = new QFormLayout;
    m_databasePath = new QLineEdit(
        QDir::toNativeSeparators(AppPaths::databaseFile()), tab);
    m_databasePath->setReadOnly(true);
    m_databasePath->setCursorPosition(0);
    form->addRow(tr("Файл базы данных:"), m_databasePath);
    layout->addLayout(form);

    auto *changeButton = new QPushButton(tr("Сменить файл…"), tab);
    connect(changeButton, &QPushButton::clicked,
            this, &SettingsDialog::changeDatabaseFile);

    auto *buttonRow = new QHBoxLayout;
    buttonRow->addWidget(changeButton);
    buttonRow->addStretch();
    layout->addLayout(buttonRow);

    auto *hint = new QLabel(
        tr("Данные хранятся локально. Для переноса на другой компьютер "
           "скопируйте этот файл вместе с программой. "
           "Смена файла применяется после перезапуска."),
        tab);
    hint->setWordWrap(true);
    layout->addWidget(hint);
    layout->addStretch();

    return tab;
}

QWidget *SettingsDialog::createBackupTab()
{
    auto *tab = new QWidget(this);
    auto *layout = new QVBoxLayout(tab);

    m_backupInfo = new QLabel(tab);
    m_backupInfo->setWordWrap(true);
    layout->addWidget(m_backupInfo);

    m_backupList = new QListWidget(tab);
    m_backupList->setMinimumHeight(150);
    m_backupList->setAlternatingRowColors(true);
    m_backupList->setToolTip(
        tr("Копии хранятся в %1").arg(QDir::toNativeSeparators(
            AppPaths::backupsDir())));
    connect(m_backupList, &QListWidget::itemSelectionChanged, this, [this] {
        if (m_restoreButton)
            m_restoreButton->setEnabled(m_backupList->currentItem() != nullptr);
    });
    layout->addWidget(m_backupList, 1);

    auto *hint = new QLabel(
        tr("Восстановление заменяет текущую базу выбранной копией. "
           "Текущая база сохраняется рядом с копиями как "
           "veterans-before-restore-*.db, после восстановления приложение "
           "перезапускается."),
        tab);
    hint->setWordWrap(true);
    layout->addWidget(hint);

    auto *row = new QHBoxLayout;
    auto *createButton = new QPushButton(tr("Создать копию сейчас"), tab);
    connect(createButton, &QPushButton::clicked,
            this, &SettingsDialog::createBackupNow);
    row->addWidget(createButton);

    m_restoreButton = new QPushButton(tr("Восстановить…"), tab);
    m_restoreButton->setEnabled(false);
    connect(m_restoreButton, &QPushButton::clicked,
            this, &SettingsDialog::restoreSelectedBackup);
    row->addWidget(m_restoreButton);

    auto *openButton = new QPushButton(tr("Открыть папку"), tab);
    connect(openButton, &QPushButton::clicked,
            this, &SettingsDialog::openBackupsFolder);
    row->addWidget(openButton);
    row->addStretch();
    layout->addLayout(row);

    m_backupStatus = new QLabel(tab);
    m_backupStatus->setWordWrap(true);
    layout->addWidget(m_backupStatus);

    refreshBackupInfo();
    return tab;
}

void SettingsDialog::refreshBackupInfo()
{
    const QStringList backups = BackupService::list();

    if (m_backupInfo) {
        if (backups.isEmpty()) {
            m_backupInfo->setText(
                tr("Резервных копий пока нет. Автобэкап создаётся при запуске "
                   "раз в сутки, хранится до %1 копий.")
                    .arg(BackupService::kKeepFiles));
        } else {
            const QFileInfo latest(backups.first());
            m_backupInfo->setText(
                tr("Копий: %1 (хранится до %2). Последняя: %3, %4 КБ.")
                    .arg(backups.size())
                    .arg(BackupService::kKeepFiles)
                    .arg(latest.lastModified().toString(
                        QStringLiteral("dd.MM.yyyy HH:mm")))
                    .arg(qMax<qint64>(1, latest.size() / 1024)));
        }
    }

    refreshBackupList(backups);
}

void SettingsDialog::refreshBackupList(const QStringList &backups)
{
    if (!m_backupList)
        return;

    m_backupList->clear();
    for (const QString &path : backups) {
        const QFileInfo info(path);
        auto *item = new QListWidgetItem(
            tr("%1 — %2 КБ — %3")
                .arg(info.lastModified().toString(
                    QStringLiteral("dd.MM.yyyy HH:mm")))
                .arg(qMax<qint64>(1, info.size() / 1024))
                .arg(info.fileName()),
            m_backupList);
        item->setData(Qt::UserRole, path);
        item->setToolTip(QDir::toNativeSeparators(path));
    }

    if (m_backupList->count() > 0)
        m_backupList->setCurrentRow(0);
    if (m_restoreButton)
        m_restoreButton->setEnabled(m_backupList->currentItem() != nullptr);
}

void SettingsDialog::createBackupNow()
{
    QString error;
    if (!BackupService::createNow(&error)) {
        QMessageBox::warning(this, tr("Не удалось создать копию"), error);
        return;
    }

    refreshBackupInfo();
    if (m_backupStatus)
        m_backupStatus->setText(tr("Резервная копия создана."));
}

void SettingsDialog::restoreSelectedBackup()
{
    const QListWidgetItem *item =
        m_backupList ? m_backupList->currentItem() : nullptr;
    if (!item) {
        QMessageBox::information(
            this, tr("Восстановление из копии"),
            tr("Выберите резервную копию в списке."));
        return;
    }

    const QString backupPath = item->data(Qt::UserRole).toString();
    const auto answer = QMessageBox::warning(
        this, tr("Восстановление из копии"),
        tr("Текущая база данных будет заменена копией:\n%1\n\n"
           "Текущая база сохранится рядом с копиями как "
           "veterans-before-restore-*.db, поэтому откат возможен.\n\n"
           "После восстановления приложение перезапустится. Продолжить?")
            .arg(QDir::toNativeSeparators(backupPath)),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (answer != QMessageBox::Yes)
        return;

    const QString databaseFile = AppPaths::databaseFile();

    // restore() подменяет файл, поэтому SQLite должен отпустить базу вместе
    // с журналами WAL/SHM — иначе копия окажется поверх открытого соединения.
    Database::close();

    QString error;
    if (!BackupService::restore(backupPath, databaseFile, &error)) {
        // Возвращаем приложение в рабочее состояние: база открывается заново,
        // данные остаются теми же, что были до попытки восстановления.
        const Database::OpenResult reopened = Database::open(databaseFile);
        if (!reopened.ok) {
            QMessageBox::critical(
                this, tr("База данных не открывается"),
                tr("Восстановление не удалось, и базу не удалось открыть "
                   "заново:\n\n%1").arg(reopened.error));
            return;
        }
        QMessageBox::warning(this, tr("Восстановление не удалось"), error);
        refreshBackupInfo();
        return;
    }

    m_restartRequested = true;
    QMessageBox::information(
        this, tr("Восстановление из копии"),
        tr("База данных восстановлена из копии. Приложение будет "
           "перезапущено."));
    accept();   // MainWindow увидит restartRequested() и перезапустит процесс
}

void SettingsDialog::openBackupsFolder()
{
    QDesktopServices::openUrl(
        QUrl::fromLocalFile(AppPaths::backupsDir()));
}

void SettingsDialog::changeDatabaseFile()
{
    const QString current = AppPaths::databaseFile();
    const QString chosen = QFileDialog::getOpenFileName(
        this, tr("Выберите файл базы данных"),
        QFileInfo(current).absolutePath(),
        tr("База SQLite (*.db *.sqlite *.sqlite3);;Все файлы (*)"));
    if (chosen.isEmpty())
        return;

    const QString path = QDir::cleanPath(chosen);
    if (path == QDir::cleanPath(current)) {
        if (m_databasePath)
            m_databasePath->setText(QDir::toNativeSeparators(path));
        return;
    }

    if (!QFileInfo::exists(path)) {
        QMessageBox::warning(
            this, tr("Файл не найден"),
            tr("Выбранный файл базы данных не существует."));
        return;
    }

    const auto answer = QMessageBox::question(
        this, tr("Смена файла базы данных"),
        tr("Текущая база останется на диске, но приложение откроет "
           "выбранный файл.\n\n"
           "Для применения изменений приложение будет перезапущено. "
           "Продолжить?"),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (answer != QMessageBox::Yes)
        return;

    AppPaths::setDatabaseFile(path);
    if (m_databasePath)
        m_databasePath->setText(QDir::toNativeSeparators(path));
    m_restartRequested = true;
}

void SettingsDialog::resetSettings()
{
    const auto answer = QMessageBox::question(
        this, tr("Сброс настроек"),
        tr("Вернуть все настройки к значениям по умолчанию?\n\n"
           "Тема, реквизиты учреждения и выбранный файл базы данных "
           "будут сброшены."),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (answer != QMessageBox::Yes)
        return;

    QSettings settings(AppPaths::settingsFile(), QSettings::IniFormat);
    settings.clear();
    settings.sync();

    const int defaultIndex = m_theme->findData(Theme::defaultId());
    if (defaultIndex >= 0)
        m_theme->setCurrentIndex(defaultIndex);
    m_organization->clear();
    if (m_databasePath)
        m_databasePath->setText(
            QDir::toNativeSeparators(AppPaths::defaultDatabaseFile()));

    Theme::apply(Theme::defaultId());
    m_initialTheme = Theme::defaultId();
    m_restartRequested = false;

    if (m_backupStatus)
        m_backupStatus->setText(tr("Настройки сброшены."));
}

void SettingsDialog::accept()
{
    QSettings settings(AppPaths::settingsFile(), QSettings::IniFormat);
    settings.setValue(QStringLiteral("Appearance/theme"),
                      m_theme->currentData().toString());
    settings.setValue(QStringLiteral("Print/organizationName"),
                      m_organization->text().trimmed());
    settings.sync();

    Theme::apply(m_theme->currentData().toString());
    QDialog::accept();
}

void SettingsDialog::reject()
{
    Theme::apply(m_initialTheme);
    QDialog::reject();
}
