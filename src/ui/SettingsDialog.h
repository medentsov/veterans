#pragma once

#include <QDialog>
#include <QString>
#include <QStringList>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QWidget;

/// Настройки приложения (M9-01): тема, реквизиты для печати, файл БД, бэкапы.
///
/// Тема применяется сразу при выборе и откатывается при отмене (M9-02).
/// Смена файла БД требует перезапуска (M9-04), сброс — возвращает умолчания
/// (M9-05).
class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SettingsDialog(QWidget *parent = nullptr);

    /// true, если пользователь сменил файл БД и нужен перезапуск.
    bool restartRequested() const { return m_restartRequested; }

public slots:
    void accept() override;
    void reject() override;

private:
    QWidget *createGeneralTab();
    QWidget *createDatabaseTab();
    QWidget *createBackupTab();
    void refreshBackupInfo();
    void refreshBackupList(const QStringList &backups);
    void createBackupNow();
    void restoreSelectedBackup();
    void openBackupsFolder();
    void changeDatabaseFile();
    void resetSettings();

    QString m_initialTheme;
    QComboBox *m_theme = nullptr;
    QLineEdit *m_organization = nullptr;
    QCheckBox *m_streetClassifier = nullptr;
    QLineEdit *m_databasePath = nullptr;
    QLabel *m_backupInfo = nullptr;
    QLabel *m_backupStatus = nullptr;
    QListWidget *m_backupList = nullptr;
    QPushButton *m_restoreButton = nullptr;
    bool m_restartRequested = false;
};
