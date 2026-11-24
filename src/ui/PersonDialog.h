#pragma once

#include "data/PersonRepository.h"

#include <QDialog>

class QComboBox;
class QDateEdit;
class QLabel;
class QLineEdit;
class QPushButton;
class QRadioButton;
class QSpinBox;
class QToolButton;
class QWidget;

/// Карточка записи: создание, просмотр и редактирование.
///
/// Режим задаётся при открытии. В просмотре поля только читаются и доступны
/// «Выгрузить», «Печать» и «Редактировать»; в создании и правке —
/// «Сохранить» и «Отмена». «Выгрузить»/«Печать» включаются только после
/// сохранения записи в БД (M4-08).
class PersonDialog : public QDialog
{
    Q_OBJECT

public:
    enum class Mode {
        Create,
        View,
        Edit,
    };

    explicit PersonDialog(Mode mode, QWidget *parent = nullptr);

    /// Заполняет поля существующей записью (для просмотра и правки).
    void setRecord(const PersonRecord &record);

    /// Идентификатор записи после сохранения (0 — ещё не сохранена).
    int savedId() const { return m_recordId; }

    /// true, если запись существует в БД (открыта или только что сохранена).
    bool recordSaved() const { return m_saved && m_recordId > 0; }

    /// Закрытие с подтверждением, если есть несохранённые изменения.
    void reject() override;

private slots:
    void onPhoneTypeChanged();
    void formatPhone();
    void onSubscriptionChanged();
    void onPickVisitDate();
    void onSave();
    void onCancel();
    void onSwitchToEdit();
    void onExportCard();
    void onPrintCard();
    void onFieldEdited();

private:
    QWidget *createTopRow();
    QWidget *createAddressRow();
    QWidget *createDetailsRow();
    QWidget *createButtonRow();
    void loadDictionaries();
    void applyMode();
    void setFieldsReadOnly(bool readOnly);
    bool validate(bool markFields, bool focusFirstInvalid);
    void clearValidation();
    void markInvalid(QWidget *widget, bool invalid);
    void showFormMessage(const QString &text, bool error);
    bool confirmDiscard();
    PersonRecord recordFromFields() const;
    int defaultCategoryId() const;

    Mode m_mode;
    int m_recordId = 0;
    bool m_updating = false;
    bool m_validationShown = false;
    bool m_saved = false;
    bool m_dirty = false;
    bool m_editFromView = false;
    PersonRecord m_original;

    QLineEdit *m_lastName = nullptr;
    QLineEdit *m_firstName = nullptr;
    QLineEdit *m_middleName = nullptr;
    QDateEdit *m_birthDate = nullptr;
    QLineEdit *m_phone = nullptr;
    QRadioButton *m_homePhone = nullptr;
    QRadioButton *m_mobilePhone = nullptr;

    QSpinBox *m_microdistrict = nullptr;
    QLineEdit *m_street = nullptr;
    QLineEdit *m_house = nullptr;
    QLineEdit *m_building = nullptr;
    QLineEdit *m_apartment = nullptr;

    QComboBox *m_category = nullptr;
    QComboBox *m_visitMonth = nullptr;
    QSpinBox *m_visitYear = nullptr;
    QToolButton *m_visitCalendarButton = nullptr;
    QComboBox *m_subscription = nullptr;
    QSpinBox *m_subscriptionYear = nullptr;

    QLabel *m_errorLabel = nullptr;
    QPushButton *m_saveButton = nullptr;
    QPushButton *m_cancelButton = nullptr;
    QPushButton *m_editButton = nullptr;
    QPushButton *m_closeButton = nullptr;
    QPushButton *m_exportButton = nullptr;
    QPushButton *m_printButton = nullptr;
};
