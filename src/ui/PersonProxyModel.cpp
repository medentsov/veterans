#include "ui/PersonProxyModel.h"

#include "core/PhoneNumber.h"
#include "ui/PersonTableModel.h"

#include <QLocale>
#include <QStringList>

namespace {

/// «ё» приравниваем к «е», чтобы «Фёдоров» находился по «федоров».
QString fold(const QString &text)
{
    QString result = text;
    result.replace(QChar(0x0451), QChar(0x0435));   // ё → е
    result.replace(QChar(0x0401), QChar(0x0415));   // Ё → Е
    return result;
}

} // namespace

PersonProxyModel::PersonProxyModel(QObject *parent)
    : QSortFilterProxyModel(parent)
{
    setDynamicSortFilter(true);
    setSortCaseSensitivity(Qt::CaseInsensitive);

    m_collator.setLocale(QLocale(QLocale::Russian, QLocale::Russia));
    m_collator.setCaseSensitivity(Qt::CaseInsensitive);
    m_collator.setNumericMode(true);
}

bool PersonProxyModel::lessThan(const QModelIndex &left,
                                const QModelIndex &right) const
{
    if (!left.isValid() || !right.isValid())
        return left.row() < right.row();

    switch (left.column()) {
    case PersonTableModel::Microdistrict:
    case PersonTableModel::BirthYear:
    case PersonTableModel::VisitYear:
        return left.data().toInt() < right.data().toInt();
    default:
        break;
    }

    return m_collator.compare(left.data().toString(),
                              right.data().toString()) < 0;
}

void PersonProxyModel::setQuickFilter(const QString &text)
{
    const QString trimmed = text.trimmed();
    if (trimmed == m_quickFilter)
        return;

    m_quickFilter = trimmed;
    m_tokens = fold(trimmed).toCaseFolded().split(QLatin1Char(' '),
                                                  Qt::SkipEmptyParts);
    invalidateFilter();
}

void PersonProxyModel::setAdvancedFilter(const PersonFilter &filter)
{
    PersonFilter normalized = filter;
    normalized.lastName = filter.lastName.trimmed();
    if (normalized == m_advancedFilter)
        return;

    m_advancedFilter = normalized;
    invalidateFilter();
}

bool PersonProxyModel::matchesAdvanced(const PersonRecord &record) const
{
    const PersonFilter &filter = m_advancedFilter;

    if (!filter.lastName.isEmpty()) {
        const QString haystack = fold(record.lastName).toCaseFolded();
        const QString needle = fold(filter.lastName).toCaseFolded();
        if (!haystack.contains(needle))
            return false;
    }

    if (filter.microdistrict > 0
        && record.microdistrict != filter.microdistrict)
        return false;

    if (filter.birthYear > 0
        && (!record.birthDate.isValid()
            || record.birthDate.year() != filter.birthYear))
        return false;

    if (filter.categoryId > 0 && record.categoryId != filter.categoryId)
        return false;

    if (filter.visitYear > 0 && record.visitYear != filter.visitYear)
        return false;

    if (filter.subscriptionId > 0
        && record.subscriptionId != filter.subscriptionId)
        return false;

    if (filter.subscriptionYear > 0
        && record.subscriptionYear != filter.subscriptionYear)
        return false;

    return true;
}

bool PersonProxyModel::filterAcceptsRow(int sourceRow,
                                        const QModelIndex &sourceParent) const
{
    if (sourceParent.isValid())
        return true;
    if (m_tokens.isEmpty() && m_advancedFilter.isEmpty())
        return true;

    auto *model = qobject_cast<PersonTableModel *>(sourceModel());
    if (!model)
        return true;

    const PersonRecord *record = model->recordAt(sourceRow);
    if (!record)
        return false;

    if (!matchesAdvanced(*record))
        return false;

    if (m_tokens.isEmpty())
        return true;

    QStringList parts;
    parts << record->fullName()
          << QString::number(record->microdistrict)
          << record->categoryName
          << record->subscriptionName
          << record->street
          << record->house
          << record->building
          << record->apartment;

    if (record->birthDate.isValid()) {
        parts << QString::number(record->birthDate.year())
              << record->birthDate.toString(QStringLiteral("dd.MM.yyyy"));
    }
    if (record->visitYear > 0)
        parts << QString::number(record->visitYear);
    if (record->visitMonth > 0 && record->visitMonth <= 12
        && record->visitYear > 0) {
        parts << QStringLiteral("%1.%2")
                     .arg(record->visitMonth, 2, 10, QLatin1Char('0'))
                     .arg(record->visitYear);
    }
    if (record->subscriptionYear > 0)
        parts << QString::number(record->subscriptionYear);
    if (!record->phone.isEmpty()) {
        const auto type = static_cast<PhoneType>(record->phoneType);
        parts << record->phone
              << PhoneNumber::format(type, record->phone)
              << PhoneNumber::typeName(type);
    }

    const QString haystack = fold(parts.join(QLatin1Char(' '))).toCaseFolded();
    for (const QString &token : m_tokens) {
        if (!haystack.contains(token))
            return false;
    }
    return true;
}
