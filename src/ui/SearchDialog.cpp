#include "ui/SearchDialog.h"

#include "data/PersonRepository.h"

#include <QComboBox>
#include <QDate>
#include <QDebug>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QWidget>

SearchDialog::SearchDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Расширенный поиск"));
    setWindowIcon(QIcon(QStringLiteral(":/icons/veterans-256.png")));
    setMinimumWidth(480);
    resize(500, 390);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(14, 14, 14, 14);
    layout->setSpacing(12);

    auto *hint = new QLabel(
        tr("Пустое поле или «любой» не ограничивает выборку. "
           "Все заполненные критерии действуют одновременно."),
        this);
    hint->setWordWrap(true);
    layout->addWidget(hint);

    auto *form = new QFormLayout;
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    form->setHorizontalSpacing(12);
    form->setVerticalSpacing(8);

    m_lastName = new QLineEdit(this);
    m_lastName->setMaxLength(100);
    m_lastName->setClearButtonEnabled(true);
    m_lastName->setPlaceholderText(tr("часть фамилии"));
    form->addRow(tr("Фамилия"), m_lastName);

    m_microdistrict = new QSpinBox(this);
    m_microdistrict->setRange(0, 999);
    m_microdistrict->setSpecialValueText(tr("любой"));
    m_microdistrict->setAlignment(Qt::AlignCenter);
    form->addRow(tr("Микрорайон"), m_microdistrict);

    m_birthYear = new QSpinBox(this);
    m_birthYear->setRange(0, QDate::currentDate().year());
    m_birthYear->setSpecialValueText(tr("любой"));
    m_birthYear->setAlignment(Qt::AlignCenter);
    form->addRow(tr("Год рождения"), m_birthYear);

    m_category = new QComboBox(this);
    m_category->addItem(tr("все категории"), 0);
    form->addRow(tr("Льготная категория"), m_category);

    m_visitYear = new QSpinBox(this);
    m_visitYear->setRange(0, QDate::currentDate().year() + 1);
    m_visitYear->setSpecialValueText(tr("любой"));
    m_visitYear->setAlignment(Qt::AlignCenter);
    form->addRow(tr("Год посещения"), m_visitYear);

    m_subscription = new QComboBox(this);
    m_subscription->addItem(tr("все подписки"), 0);
    form->addRow(tr("Подписка"), m_subscription);

    m_subscriptionYear = new QSpinBox(this);
    m_subscriptionYear->setRange(0, QDate::currentDate().year() + 1);
    m_subscriptionYear->setSpecialValueText(tr("любой"));
    m_subscriptionYear->setAlignment(Qt::AlignCenter);
    form->addRow(tr("Год подписки"), m_subscriptionYear);

    layout->addLayout(form);
    layout->addStretch(1);

    auto *buttons = new QDialogButtonBox(this);
    auto *findButton = buttons->addButton(tr("Найти"),
                                          QDialogButtonBox::AcceptRole);
    auto *resetButton = buttons->addButton(tr("Сбросить"),
                                           QDialogButtonBox::ResetRole);
    auto *cancelButton = buttons->addButton(tr("Отмена"),
                                            QDialogButtonBox::RejectRole);
    findButton->setDefault(true);

    connect(findButton, &QPushButton::clicked, this, &QDialog::accept);
    connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);
    connect(resetButton, &QPushButton::clicked, this, [this] {
        m_lastName->clear();
        m_microdistrict->setValue(0);
        m_birthYear->setValue(0);
        m_category->setCurrentIndex(0);
        m_visitYear->setValue(0);
        m_subscription->setCurrentIndex(0);
        m_subscriptionYear->setValue(0);
        accept();
    });

    layout->addWidget(buttons);

    loadDictionaries();
}

PersonFilter SearchDialog::filter() const
{
    PersonFilter result;
    result.lastName = m_lastName->text().trimmed();
    result.microdistrict = m_microdistrict->value();
    result.birthYear = m_birthYear->value();
    result.categoryId = m_category->currentData().toInt();
    result.visitYear = m_visitYear->value();
    result.subscriptionId = m_subscription->currentData().toInt();
    result.subscriptionYear = m_subscriptionYear->value();
    return result;
}

void SearchDialog::setFilter(const PersonFilter &filter)
{
    m_lastName->setText(filter.lastName);
    m_microdistrict->setValue(filter.microdistrict);
    m_birthYear->setValue(filter.birthYear);
    m_visitYear->setValue(filter.visitYear);

    int categoryIndex = m_category->findData(filter.categoryId);
    if (categoryIndex < 0)
        categoryIndex = 0;
    m_category->setCurrentIndex(categoryIndex);

    int subscriptionIndex = m_subscription->findData(filter.subscriptionId);
    if (subscriptionIndex < 0)
        subscriptionIndex = 0;
    m_subscription->setCurrentIndex(subscriptionIndex);

    m_subscriptionYear->setValue(filter.subscriptionYear);
}

void SearchDialog::loadDictionaries()
{
    QString error;
    const QVector<Category> categories = PersonRepository::categories(&error);
    if (!error.isEmpty()) {
        qWarning().noquote()
            << "[SearchDialog] не удалось загрузить категории:" << error;
    } else {
        for (const Category &category : categories)
            m_category->addItem(category.name, category.id);
    }

    error.clear();
    const QVector<Subscription> subscriptions =
        PersonRepository::subscriptions(&error);
    if (!error.isEmpty()) {
        qWarning().noquote()
            << "[SearchDialog] не удалось загрузить подписки:" << error;
    } else {
        for (const Subscription &subscription : subscriptions)
            m_subscription->addItem(subscription.name, subscription.id);
    }
}
