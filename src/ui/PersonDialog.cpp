#include "ui/PersonDialog.h"

#include "core/PhoneNumber.h"
#include "data/PersonRepository.h"
#include "report/PrintService.h"

#include <QAbstractSpinBox>
#include <QApplication>
#include <QCalendarWidget>
#include <QComboBox>
#include <QDate>
#include <QDateEdit>
#include <QDebug>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QList>
#include <QMessageBox>
#include <QPushButton>
#include <QRadioButton>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QSpinBox>
#include <QStringList>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWidget>

namespace {

QRegularExpression namePattern()
{
    return QRegularExpression(QStringLiteral("^[\\p{L} '\\-]*$"));
}

QRegularExpression shortAddressPattern()
{
    return QRegularExpression(QStringLiteral("^[\\p{L}\\p{Nd}]*$"));
}

QRegularExpression phonePattern()
{
    return QRegularExpression(QStringLiteral("^[0-9+()\\- ]*$"));
}

QString placeholder()
{
    return QStringLiteral("—");
}

} // namespace

PersonDialog::PersonDialog(Mode mode, QWidget *parent)
    : QDialog(parent)
    , m_mode(mode)
{
    m_updating = true;

    setWindowIcon(QIcon(QStringLiteral(":/icons/veterans-256.png")));
    setMinimumWidth(900);
    resize(1000, 360);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(14, 14, 14, 14);
    layout->setSpacing(12);

    layout->addWidget(createTopRow());
    layout->addWidget(createAddressRow());
    layout->addWidget(createDetailsRow());
    layout->addStretch(1);

    m_errorLabel = new QLabel(this);
    m_errorLabel->setWordWrap(true);
    m_errorLabel->setVisible(false);
    layout->addWidget(m_errorLabel);

    layout->addWidget(createButtonRow());

    connect(m_lastName, &QLineEdit::textEdited,
            this, &PersonDialog::onFieldEdited);
    connect(m_firstName, &QLineEdit::textEdited,
            this, &PersonDialog::onFieldEdited);
    connect(m_middleName, &QLineEdit::textEdited,
            this, &PersonDialog::onFieldEdited);
    connect(m_street, &QLineEdit::textEdited,
            this, &PersonDialog::onFieldEdited);
    connect(m_house, &QLineEdit::textEdited,
            this, &PersonDialog::onFieldEdited);
    connect(m_building, &QLineEdit::textEdited,
            this, &PersonDialog::onFieldEdited);
    connect(m_apartment, &QLineEdit::textEdited,
            this, &PersonDialog::onFieldEdited);
    connect(m_phone, &QLineEdit::textEdited,
            this, &PersonDialog::onFieldEdited);
    connect(m_birthDate, &QDateEdit::dateChanged,
            this, &PersonDialog::onFieldEdited);
    connect(m_microdistrict,
            QOverload<int>::of(&QSpinBox::valueChanged),
            this, &PersonDialog::onFieldEdited);
    connect(m_visitYear,
            QOverload<int>::of(&QSpinBox::valueChanged),
            this, &PersonDialog::onFieldEdited);
    connect(m_subscriptionYear,
            QOverload<int>::of(&QSpinBox::valueChanged),
            this, &PersonDialog::onFieldEdited);
    connect(m_category,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &PersonDialog::onFieldEdited);
    connect(m_visitMonth,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &PersonDialog::onFieldEdited);
    connect(m_subscription,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &PersonDialog::onFieldEdited);
    connect(m_homePhone, &QRadioButton::toggled,
            this, &PersonDialog::onFieldEdited);
    connect(m_mobilePhone, &QRadioButton::toggled,
            this, &PersonDialog::onFieldEdited);

    loadDictionaries();

    if (m_mode == Mode::Create) {
        const int index = m_category->findText(QStringLiteral("Без льгот"),
                                               Qt::MatchFixedString);
        if (index >= 0)
            m_category->setCurrentIndex(index);
    }

    applyMode();

    m_updating = false;
    m_dirty = false;
}

QWidget *PersonDialog::createTopRow()
{
    auto *row = new QWidget(this);
    auto *grid = new QGridLayout(row);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setHorizontalSpacing(8);
    grid->setVerticalSpacing(4);

    m_lastName = new QLineEdit(row);
    m_lastName->setMaxLength(100);
    m_lastName->setClearButtonEnabled(true);
    m_lastName->setValidator(
        new QRegularExpressionValidator(namePattern(), m_lastName));

    m_firstName = new QLineEdit(row);
    m_firstName->setMaxLength(100);
    m_firstName->setClearButtonEnabled(true);
    m_firstName->setValidator(
        new QRegularExpressionValidator(namePattern(), m_firstName));

    m_middleName = new QLineEdit(row);
    m_middleName->setMaxLength(100);
    m_middleName->setClearButtonEnabled(true);
    m_middleName->setPlaceholderText(tr("необязательно"));
    m_middleName->setValidator(
        new QRegularExpressionValidator(namePattern(), m_middleName));

    grid->addWidget(new QLabel(tr("Фамилия"), row), 0, 0);
    grid->addWidget(new QLabel(tr("Имя"), row), 0, 1);
    grid->addWidget(new QLabel(tr("Отчество"), row), 0, 2);
    grid->addWidget(m_lastName, 1, 0);
    grid->addWidget(m_firstName, 1, 1);
    grid->addWidget(m_middleName, 1, 2);

    grid->setColumnStretch(0, 1);
    grid->setColumnStretch(1, 1);
    grid->setColumnStretch(2, 1);

    m_birthDate = new QDateEdit(row);
    m_birthDate->setDisplayFormat(QStringLiteral("dd.MM.yyyy"));
    m_birthDate->setCalendarPopup(true);
    m_birthDate->setMinimumDate(QDate(1900, 1, 1));
    m_birthDate->setMaximumDate(QDate::currentDate());
    m_birthDate->setDate(QDate(1950, 1, 1));

    grid->addWidget(new QLabel(tr("Дата рождения"), row), 0, 3);
    grid->addWidget(m_birthDate, 1, 3);

    m_phone = new QLineEdit(row);
    m_phone->setClearButtonEnabled(true);
    m_phone->setInputMethodHints(Qt::ImhDialableCharactersOnly);
    m_phone->setValidator(
        new QRegularExpressionValidator(phonePattern(), m_phone));
    m_phone->setPlaceholderText(QStringLiteral("X-XX-XX"));
    m_phone->setToolTip(tr("5 цифр, на экране X-XX-XX"));
    m_phone->setMinimumWidth(130);
    connect(m_phone, &QLineEdit::editingFinished,
            this, &PersonDialog::formatPhone);

    grid->addWidget(new QLabel(tr("Телефон"), row), 0, 4);
    grid->addWidget(m_phone, 1, 4);

    auto *typeBox = new QWidget(row);
    auto *typeLayout = new QHBoxLayout(typeBox);
    typeLayout->setContentsMargins(0, 0, 0, 0);
    typeLayout->setSpacing(10);

    m_homePhone = new QRadioButton(tr("Домашний"), typeBox);
    m_mobilePhone = new QRadioButton(tr("Мобильный"), typeBox);
    m_homePhone->setChecked(true);
    typeLayout->addWidget(m_homePhone);
    typeLayout->addWidget(m_mobilePhone);

    connect(m_homePhone, &QRadioButton::toggled,
            this, &PersonDialog::onPhoneTypeChanged);
    connect(m_mobilePhone, &QRadioButton::toggled,
            this, &PersonDialog::onPhoneTypeChanged);

    grid->addWidget(new QLabel(tr("Тип"), row), 0, 5);
    grid->addWidget(typeBox, 1, 5);

    return row;
}

QWidget *PersonDialog::createAddressRow()
{
    auto *row = new QWidget(this);
    auto *grid = new QGridLayout(row);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setHorizontalSpacing(8);
    grid->setVerticalSpacing(4);

    m_microdistrict = new QSpinBox(row);
    m_microdistrict->setRange(0, 999);
    m_microdistrict->setSpecialValueText(placeholder());
    m_microdistrict->setAlignment(Qt::AlignCenter);
    m_microdistrict->setMaximumWidth(100);

    m_street = new QLineEdit(row);
    m_street->setMaxLength(120);
    m_street->setPlaceholderText(tr("ул. Ленина"));

    m_house = new QLineEdit(row);
    m_house->setMaxLength(10);
    m_house->setPlaceholderText(QStringLiteral("15"));
    m_house->setMaximumWidth(90);
    m_house->setValidator(
        new QRegularExpressionValidator(shortAddressPattern(), m_house));

    m_building = new QLineEdit(row);
    m_building->setMaxLength(10);
    m_building->setPlaceholderText(QStringLiteral("2"));
    m_building->setMaximumWidth(90);
    m_building->setValidator(
        new QRegularExpressionValidator(shortAddressPattern(), m_building));

    m_apartment = new QLineEdit(row);
    m_apartment->setMaxLength(10);
    m_apartment->setPlaceholderText(QStringLiteral("41"));
    m_apartment->setMaximumWidth(90);
    m_apartment->setValidator(
        new QRegularExpressionValidator(shortAddressPattern(), m_apartment));

    grid->addWidget(new QLabel(tr("Микрорайон"), row), 0, 0);
    grid->addWidget(new QLabel(tr("Улица"), row), 0, 1);
    grid->addWidget(new QLabel(tr("Дом"), row), 0, 2);
    grid->addWidget(new QLabel(tr("Корпус"), row), 0, 3);
    grid->addWidget(new QLabel(tr("Квартира"), row), 0, 4);

    grid->addWidget(m_microdistrict, 1, 0);
    grid->addWidget(m_street, 1, 1);
    grid->addWidget(m_house, 1, 2);
    grid->addWidget(m_building, 1, 3);
    grid->addWidget(m_apartment, 1, 4);

    grid->setColumnStretch(1, 1);

    return row;
}

QWidget *PersonDialog::createDetailsRow()
{
    auto *row = new QWidget(this);
    auto *grid = new QGridLayout(row);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setHorizontalSpacing(8);
    grid->setVerticalSpacing(4);

    m_category = new QComboBox(row);
    m_category->setSizeAdjustPolicy(
        QComboBox::AdjustToMinimumContentsLengthWithIcon);
    m_category->setMinimumContentsLength(18);
    m_category->addItem(tr("выберите категорию"), 0);

    auto *visitBox = new QWidget(row);
    auto *visitLayout = new QHBoxLayout(visitBox);
    visitLayout->setContentsMargins(0, 0, 0, 0);
    visitLayout->setSpacing(6);

    m_visitMonth = new QComboBox(visitBox);
    m_visitMonth->addItem(placeholder(), 0);
    for (int month = 1; month <= 12; ++month) {
        m_visitMonth->addItem(
            QStringLiteral("%1").arg(month, 2, 10, QLatin1Char('0')), month);
    }
    m_visitMonth->setMinimumWidth(60);

    m_visitYear = new QSpinBox(visitBox);
    m_visitYear->setRange(0, QDate::currentDate().year() + 1);
    m_visitYear->setSpecialValueText(placeholder());
    m_visitYear->setAlignment(Qt::AlignCenter);
    m_visitYear->setMinimumWidth(90);

    m_visitCalendarButton = new QToolButton(visitBox);
    m_visitCalendarButton->setText(QStringLiteral("…"));
    m_visitCalendarButton->setToolTip(tr("Выбрать месяц и год в календаре"));
    m_visitCalendarButton->setAccessibleName(tr("Календарь посещения"));
    m_visitCalendarButton->setAutoRaise(true);
    m_visitCalendarButton->setFixedSize(28, 28);
    connect(m_visitCalendarButton, &QToolButton::clicked,
            this, &PersonDialog::onPickVisitDate);

    visitLayout->addWidget(m_visitMonth);
    visitLayout->addWidget(m_visitYear);
    visitLayout->addWidget(m_visitCalendarButton);

    auto *subscriptionBox = new QWidget(row);
    auto *subscriptionLayout = new QHBoxLayout(subscriptionBox);
    subscriptionLayout->setContentsMargins(0, 0, 0, 0);
    subscriptionLayout->setSpacing(6);

    m_subscription = new QComboBox(subscriptionBox);
    m_subscription->setSizeAdjustPolicy(
        QComboBox::AdjustToMinimumContentsLengthWithIcon);
    m_subscription->setMinimumContentsLength(14);
    m_subscription->addItem(tr("нет подписки"), 0);

    m_subscriptionYear = new QSpinBox(subscriptionBox);
    m_subscriptionYear->setRange(0, QDate::currentDate().year() + 1);
    m_subscriptionYear->setSpecialValueText(placeholder());
    m_subscriptionYear->setAlignment(Qt::AlignCenter);
    m_subscriptionYear->setMinimumWidth(90);
    m_subscriptionYear->setEnabled(false);

    connect(m_subscription,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &PersonDialog::onSubscriptionChanged);

    subscriptionLayout->addWidget(m_subscription, 1);
    subscriptionLayout->addWidget(m_subscriptionYear);

    grid->addWidget(new QLabel(tr("Льготная категория"), row), 0, 0);
    grid->addWidget(new QLabel(tr("Посещение госпиталя"), row), 0, 1);
    grid->addWidget(new QLabel(tr("Подписка на газету"), row), 0, 2);

    grid->addWidget(m_category, 1, 0);
    grid->addWidget(visitBox, 1, 1);
    grid->addWidget(subscriptionBox, 1, 2);

    grid->setColumnStretch(0, 2);
    grid->setColumnStretch(1, 1);
    grid->setColumnStretch(2, 2);

    return row;
}

QWidget *PersonDialog::createButtonRow()
{
    auto *row = new QWidget(this);
    auto *layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);

    m_exportButton = new QPushButton(tr("Выгрузить"), row);
    m_exportButton->setToolTip(tr("Доступно после сохранения записи"));
    connect(m_exportButton, &QPushButton::clicked,
            this, &PersonDialog::onExportCard);

    m_printButton = new QPushButton(tr("Печать"), row);
    m_printButton->setToolTip(tr("Доступно после сохранения записи"));
    connect(m_printButton, &QPushButton::clicked,
            this, &PersonDialog::onPrintCard);

    m_editButton = new QPushButton(tr("Редактировать"), row);
    connect(m_editButton, &QPushButton::clicked,
            this, &PersonDialog::onSwitchToEdit);

    m_saveButton = new QPushButton(tr("Сохранить"), row);
    m_saveButton->setDefault(true);
    connect(m_saveButton, &QPushButton::clicked,
            this, &PersonDialog::onSave);

    m_cancelButton = new QPushButton(tr("Отмена"), row);
    connect(m_cancelButton, &QPushButton::clicked,
            this, &PersonDialog::onCancel);

    m_closeButton = new QPushButton(tr("Закрыть"), row);
    connect(m_closeButton, &QPushButton::clicked,
            this, &QDialog::reject);

    layout->addWidget(m_exportButton);
    layout->addWidget(m_printButton);
    layout->addWidget(m_editButton);
    layout->addStretch(1);
    layout->addWidget(m_saveButton);
    layout->addWidget(m_cancelButton);
    layout->addWidget(m_closeButton);

    return row;
}

void PersonDialog::loadDictionaries()
{
    QString error;
    const QVector<Category> categories = PersonRepository::categories(&error);
    if (!error.isEmpty()) {
        qWarning().noquote()
            << "[PersonDialog] не удалось загрузить категории:" << error;
    } else {
        for (const Category &category : categories)
            m_category->addItem(category.name, category.id);
    }

    error.clear();
    const QVector<Subscription> subscriptions =
        PersonRepository::subscriptions(&error);
    if (!error.isEmpty()) {
        qWarning().noquote()
            << "[PersonDialog] не удалось загрузить подписки:" << error;
    } else {
        for (const Subscription &subscription : subscriptions)
            m_subscription->addItem(subscription.name, subscription.id);
    }
}

void PersonDialog::applyMode()
{
    const bool view = m_mode == Mode::View;
    const bool hasSavedRecord = m_saved && m_recordId > 0;

    setWindowTitle(view ? tr("Просмотр записи")
                        : (m_mode == Mode::Edit ? tr("Редактирование записи")
                                                : tr("Новая запись")));

    setFieldsReadOnly(view);

    m_saveButton->setVisible(!view);
    m_cancelButton->setVisible(!view);
    m_editButton->setVisible(view);
    m_closeButton->setVisible(view);

    m_saveButton->setDefault(!view);
    m_closeButton->setDefault(view);

    m_exportButton->setEnabled(view && hasSavedRecord);
    m_printButton->setEnabled(view && hasSavedRecord);
    m_exportButton->setToolTip(
        view && hasSavedRecord
            ? tr("Выгрузить карточку записи")
            : tr("Доступно после сохранения записи"));
    m_printButton->setToolTip(
        view && hasSavedRecord
            ? tr("Напечатать карточку записи")
            : tr("Доступно после сохранения записи"));

    onSubscriptionChanged();
}

void PersonDialog::setFieldsReadOnly(bool readOnly)
{
    const QList<QLineEdit *> lineEdits = {
        m_lastName, m_firstName, m_middleName, m_street,
        m_house, m_building, m_apartment, m_phone,
    };
    for (QLineEdit *edit : lineEdits) {
        edit->setReadOnly(readOnly);
        edit->setClearButtonEnabled(!readOnly);
    }

    m_birthDate->setReadOnly(readOnly);
    m_birthDate->setCalendarPopup(!readOnly);

    const QList<QSpinBox *> spinBoxes = {
        m_microdistrict, m_visitYear, m_subscriptionYear,
    };
    for (QSpinBox *box : spinBoxes) {
        box->setReadOnly(readOnly);
        box->setButtonSymbols(readOnly ? QAbstractSpinBox::NoButtons
                                       : QAbstractSpinBox::UpDownArrows);
    }

    m_category->setEnabled(!readOnly);
    m_visitMonth->setEnabled(!readOnly);
    m_subscription->setEnabled(!readOnly);

    m_homePhone->setEnabled(!readOnly);
    m_mobilePhone->setEnabled(!readOnly);
    m_visitCalendarButton->setEnabled(!readOnly);
}

bool PersonDialog::validate(bool markFields, bool focusFirstInvalid)
{
    if (markFields)
        clearValidation();

    QStringList errors;
    QWidget *firstInvalid = nullptr;

    const auto report = [&](QWidget *widget, const QString &message) {
        errors << message;
        if (!markFields)
            return;
        markInvalid(widget, true);
        if (!firstInvalid)
            firstInvalid = widget;
    };

    const QString lastName = m_lastName->text().trimmed();
    if (lastName.isEmpty())
        report(m_lastName, tr("Укажите фамилию."));
    else if (!namePattern().match(lastName).hasMatch())
        report(m_lastName,
               tr("Фамилия: буквы, дефис, пробел и апостроф, до 100 символов."));

    const QString firstName = m_firstName->text().trimmed();
    if (firstName.isEmpty())
        report(m_firstName, tr("Укажите имя."));
    else if (!namePattern().match(firstName).hasMatch())
        report(m_firstName,
               tr("Имя: буквы, дефис, пробел и апостроф, до 100 символов."));

    const QString middleName = m_middleName->text().trimmed();
    if (!middleName.isEmpty() && !namePattern().match(middleName).hasMatch())
        report(m_middleName,
               tr("Отчество: буквы, дефис, пробел и апостроф, до 100 символов."));

    const QDate birthDate = m_birthDate->date();
    if (!birthDate.isValid() || birthDate < QDate(1900, 1, 1)
        || birthDate > QDate::currentDate()) {
        report(m_birthDate,
               tr("Дата рождения должна быть в диапазоне 01.01.1900 … сегодня."));
    }

    const int microdistrict = m_microdistrict->value();
    if (microdistrict < 1 || microdistrict > 999)
        report(m_microdistrict, tr("Укажите микрорайон от 1 до 999."));

    const QString street = m_street->text().trimmed();
    if (street.size() > 120)
        report(m_street, tr("Улица: не более 120 символов."));

    const auto checkShortAddress = [&](QLineEdit *edit, const QString &label) {
        const QString value = edit->text().trimmed();
        if (value.size() > 10 || !shortAddressPattern().match(value).hasMatch())
            report(edit, tr("%1: до 10 символов, только буквы и цифры.")
                             .arg(label));
    };
    checkShortAddress(m_house, tr("Дом"));
    checkShortAddress(m_building, tr("Корпус"));
    checkShortAddress(m_apartment, tr("Квартира"));

    const PhoneType phoneType = m_mobilePhone->isChecked()
        ? PhoneType::Mobile
        : PhoneType::Home;
    const QString phoneText = m_phone->text().trimmed();
    if (!phoneText.isEmpty()) {
        const QString digits = PhoneNumber::normalize(phoneType, phoneText);
        if (!PhoneNumber::isValid(phoneType, digits)) {
            report(m_phone,
                   phoneType == PhoneType::Home
                       ? tr("Домашний телефон: ровно 5 цифр.")
                       : tr("Мобильный телефон: 11 цифр, начиная с 7."));
        }
    }

    if (m_category->currentData().toInt() <= 0 && defaultCategoryId() <= 0)
        report(m_category, tr("Выберите льготную категорию."));

    const int visitMonth = m_visitMonth->currentData().toInt();
    const int visitYear = m_visitYear->value();
    if ((visitMonth == 0) != (visitYear == 0)) {
        errors << tr("Посещение: укажите и месяц, и год или оставьте оба поля "
                     "пустыми.");
        if (markFields) {
            markInvalid(m_visitMonth, true);
            markInvalid(m_visitYear, true);
            if (!firstInvalid)
                firstInvalid = m_visitMonth;
        }
    } else if (visitMonth != 0) {
        if (visitMonth < 1 || visitMonth > 12)
            report(m_visitMonth, tr("Месяц посещения: от 1 до 12."));
        if (visitYear < 1990 || visitYear > QDate::currentDate().year() + 1)
            report(m_visitYear,
                   tr("Год посещения: от 1990 до %1.")
                       .arg(QDate::currentDate().year() + 1));
    }

    const int subscriptionId = m_subscription->currentData().toInt();
    const int subscriptionYear = m_subscriptionYear->value();
    if (subscriptionId > 0) {
        if (subscriptionYear < 2000
            || subscriptionYear > QDate::currentDate().year() + 1) {
            report(m_subscriptionYear,
                   tr("Год подписки: от 2000 до %1.")
                       .arg(QDate::currentDate().year() + 1));
        }
    } else if (subscriptionYear != 0) {
        report(m_subscriptionYear,
               tr("Выберите газету или очистите год подписки."));
    }

    if (markFields && !errors.isEmpty()) {
        m_errorLabel->setStyleSheet(QStringLiteral("color: #c62828;"));
        m_errorLabel->setText(tr("Проверьте поля. %1").arg(errors.join(' ')));
        m_errorLabel->show();
        m_validationShown = true;

        if (focusFirstInvalid && firstInvalid) {
            firstInvalid->setFocus(Qt::OtherFocusReason);
            if (auto *edit = qobject_cast<QLineEdit *>(firstInvalid))
                edit->selectAll();
        }
    }

    return errors.isEmpty();
}

void PersonDialog::clearValidation()
{
    const QList<QWidget *> widgets = {
        m_lastName, m_firstName, m_middleName, m_birthDate,
        m_phone, m_microdistrict, m_street, m_house,
        m_building, m_apartment, m_category, m_visitMonth,
        m_visitYear, m_subscription, m_subscriptionYear,
    };
    for (QWidget *widget : widgets)
        markInvalid(widget, false);

    m_errorLabel->clear();
    m_errorLabel->hide();
    m_validationShown = false;
}

void PersonDialog::markInvalid(QWidget *widget, bool invalid)
{
    if (!widget)
        return;

    if (invalid) {
        widget->setStyleSheet(QStringLiteral(
            "border: 1px solid #c62828; border-radius: 3px;"
            " background-color: #fdecea;"));
    } else {
        widget->setStyleSheet(QString());
    }
}

void PersonDialog::showFormMessage(const QString &text, bool error)
{
    m_errorLabel->setStyleSheet(
        error ? QStringLiteral("color: #c62828;")
              : QStringLiteral("color: #2e7d32;"));
    m_errorLabel->setText(text);
    m_errorLabel->setVisible(!text.isEmpty());
}

PersonRecord PersonDialog::recordFromFields() const
{
    PersonRecord record;
    record.id = m_recordId;
    record.lastName = m_lastName->text().trimmed();
    record.firstName = m_firstName->text().trimmed();
    record.middleName = m_middleName->text().trimmed();
    record.birthDate = m_birthDate->date();
    record.microdistrict = m_microdistrict->value();
    record.street = m_street->text().trimmed();
    record.house = m_house->text().trimmed();
    record.building = m_building->text().trimmed();
    record.apartment = m_apartment->text().trimmed();

    record.phoneType = m_mobilePhone->isChecked() ? 1 : 0;
    record.phone = PhoneNumber::normalize(
        record.phoneType == 1 ? PhoneType::Mobile : PhoneType::Home,
        m_phone->text());

    record.categoryId = m_category->currentData().toInt();
    if (record.categoryId <= 0)
        record.categoryId = defaultCategoryId();

    record.visitMonth = m_visitMonth->currentData().toInt();
    record.visitYear = record.visitMonth > 0 ? m_visitYear->value() : 0;

    record.subscriptionId = m_subscription->currentData().toInt();
    record.subscriptionYear =
        record.subscriptionId > 0 ? m_subscriptionYear->value() : 0;

    return record;
}

int PersonDialog::defaultCategoryId() const
{
    for (int i = 0; i < m_category->count(); ++i) {
        if (m_category->itemText(i).compare(QStringLiteral("Без льгот"),
                                            Qt::CaseInsensitive)
            == 0) {
            return m_category->itemData(i).toInt();
        }
    }
    return 0;
}

void PersonDialog::setRecord(const PersonRecord &record)
{
    m_updating = true;

    m_original = record;
    m_recordId = record.id;
    m_saved = record.id > 0;
    m_dirty = false;

    m_lastName->setText(record.lastName);
    m_firstName->setText(record.firstName);
    m_middleName->setText(record.middleName);
    m_birthDate->setDate(record.birthDate.isValid() ? record.birthDate
                                                    : QDate(1950, 1, 1));
    m_microdistrict->setValue(record.microdistrict);
    m_street->setText(record.street);
    m_house->setText(record.house);
    m_building->setText(record.building);
    m_apartment->setText(record.apartment);

    const bool mobile = record.phoneType == 1;
    m_mobilePhone->setChecked(mobile);
    m_homePhone->setChecked(!mobile);

    const PhoneType type = mobile ? PhoneType::Mobile : PhoneType::Home;
    const QString digits = PhoneNumber::normalize(type, record.phone);
    m_phone->setText(PhoneNumber::isValid(type, digits)
                         ? PhoneNumber::format(type, digits)
                         : QString());

    int categoryIndex = m_category->findData(record.categoryId);
    if (categoryIndex < 0)
        categoryIndex = 0;   // «выберите категорию»
    m_category->setCurrentIndex(categoryIndex);

    int monthIndex = m_visitMonth->findData(record.visitMonth);
    if (monthIndex < 0)
        monthIndex = 0;
    m_visitMonth->setCurrentIndex(monthIndex);
    m_visitYear->setValue(record.visitMonth > 0 ? record.visitYear : 0);

    int subscriptionIndex = m_subscription->findData(record.subscriptionId);
    if (subscriptionIndex < 0)
        subscriptionIndex = 0;
    m_subscription->setCurrentIndex(subscriptionIndex);
    m_subscriptionYear->setValue(record.subscriptionId > 0
                                     ? record.subscriptionYear
                                     : 0);

    clearValidation();
    m_updating = false;

    applyMode();
}

void PersonDialog::onSubscriptionChanged()
{
    const bool hasSubscription = m_subscription->currentData().toInt() > 0;
    const bool editable = m_mode != Mode::View;

    m_subscriptionYear->setEnabled(hasSubscription && editable);

    if (!hasSubscription)
        m_subscriptionYear->setValue(0);
    else if (m_subscriptionYear->value() == 0)
        m_subscriptionYear->setValue(QDate::currentDate().year());
}

void PersonDialog::onPickVisitDate()
{
    QDialog dialog(this);
    dialog.setWindowTitle(tr("Месяц и год посещения"));
    dialog.resize(360, 320);

    auto *layout = new QVBoxLayout(&dialog);

    auto *calendar = new QCalendarWidget(&dialog);
    calendar->setGridVisible(true);
    calendar->setVerticalHeaderFormat(QCalendarWidget::NoVerticalHeader);
    calendar->setDateRange(QDate(1990, 1, 1),
                           QDate(QDate::currentDate().year() + 1, 12, 31));

    const int month = m_visitMonth->currentData().toInt();
    const int year = m_visitYear->value();
    if (month > 0 && year > 0)
        calendar->setSelectedDate(QDate(year, month, 1));
    else
        calendar->setSelectedDate(QDate::currentDate());

    layout->addWidget(calendar);

    auto *buttons =
        new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
                             &dialog);
    buttons->button(QDialogButtonBox::Ok)->setText(tr("Выбрать"));
    buttons->button(QDialogButtonBox::Cancel)->setText(tr("Отмена"));
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);

    if (dialog.exec() != QDialog::Accepted)
        return;

    const QDate selected = calendar->selectedDate();
    const int monthIndex = m_visitMonth->findData(selected.month());
    if (monthIndex >= 0)
        m_visitMonth->setCurrentIndex(monthIndex);
    m_visitYear->setValue(selected.year());
}

void PersonDialog::onPhoneTypeChanged()
{
    if (!m_phone)
        return;

    if (!m_homePhone->isChecked() && !m_mobilePhone->isChecked())
        return;

    const bool mobile = m_mobilePhone->isChecked();
    m_phone->setPlaceholderText(mobile ? QStringLiteral("+7XXXXXXXXXX")
                                       : QStringLiteral("X-XX-XX"));
    m_phone->setToolTip(mobile ? tr("11 цифр, начиная с 7")
                               : tr("5 цифр, на экране X-XX-XX"));

    formatPhone();
}

void PersonDialog::formatPhone()
{
    if (!m_phone)
        return;

    const PhoneType type = m_mobilePhone->isChecked() ? PhoneType::Mobile
                                                      : PhoneType::Home;
    const QString digits = PhoneNumber::normalize(type, m_phone->text());
    if (digits.isEmpty()) {
        m_phone->clear();
        return;
    }

    if (PhoneNumber::isValid(type, digits))
        m_phone->setText(PhoneNumber::format(type, digits));
}

void PersonDialog::onSave()
{
    if (!validate(true, true)) {
        QApplication::beep();
        return;
    }

    PersonRecord record = recordFromFields();
    QString error;
    const bool ok = m_recordId > 0
        ? PersonRepository::update(record, &error)
        : PersonRepository::insert(&record, &error);

    if (!ok) {
        showFormMessage(error, true);
        QMessageBox::critical(this, tr("Не удалось сохранить"), error);
        return;
    }

    m_recordId = record.id;
    m_saved = true;
    m_mode = Mode::View;
    m_editFromView = false;

    setRecord(record);
    showFormMessage(tr("Запись сохранена."), false);
}

void PersonDialog::onCancel()
{
    if (m_mode == Mode::Edit && m_editFromView) {
        const PersonRecord original = m_original;
        m_mode = Mode::View;
        m_editFromView = false;
        setRecord(original);
        showFormMessage(tr("Изменения отменены."), false);
        return;
    }

    reject();
}

void PersonDialog::reject()
{
    if (m_dirty && !confirmDiscard())
        return;

    QDialog::reject();
}

bool PersonDialog::confirmDiscard()
{
    QMessageBox box(this);
    box.setIcon(QMessageBox::Warning);
    box.setWindowTitle(tr("Несохранённые изменения"));
    box.setText(tr("В форме есть несохранённые изменения."));
    box.setInformativeText(tr("Закрыть форму без сохранения?"));

    auto *discardButton = box.addButton(tr("Закрыть без сохранения"),
                                        QMessageBox::DestructiveRole);
    auto *stayButton = box.addButton(tr("Продолжить редактирование"),
                                     QMessageBox::RejectRole);
    box.setDefaultButton(stayButton);
    box.setEscapeButton(stayButton);
    box.exec();

    return box.clickedButton() == discardButton;
}

void PersonDialog::onSwitchToEdit()
{
    if (m_mode != Mode::View)
        return;

    m_original = recordFromFields();
    m_editFromView = true;
    m_dirty = false;
    m_mode = Mode::Edit;

    clearValidation();
    applyMode();

    m_lastName->setFocus(Qt::OtherFocusReason);
    m_lastName->selectAll();
}

void PersonDialog::onExportCard()
{
    if (!recordSaved())
        return;

    QMessageBox::information(
        this, tr("Выгрузить карточку"),
        tr("Экспорт карточки записи появится на этапе M7."));
}

void PersonDialog::onPrintCard()
{
    if (!recordSaved())
        return;

    PersonRecord record;
    QString error;
    if (!PersonRepository::fetch(m_recordId, &record, &error)) {
        QMessageBox::critical(this, tr("Не удалось напечатать"), error);
        return;
    }

    PrintService::printCards(this, QVector<PersonRecord>{record});
}

void PersonDialog::onFieldEdited()
{
    if (m_updating)
        return;

    m_dirty = true;

    if (m_validationShown)
        clearValidation();
}
