#pragma once

#include <QCollator>
#include <QSortFilterProxyModel>
#include <QString>
#include <QStringList>

struct PersonRecord;

/// Критерии расширенного поиска. Нулевые значения и пустая фамилия
/// означают «не ограничивать».
struct PersonFilter
{
    QString lastName;
    int microdistrict = 0;
    int birthYear = 0;
    int categoryId = 0;
    int visitYear = 0;
    int subscriptionId = 0;
    int subscriptionYear = 0;

    bool isEmpty() const
    {
        return lastName.isEmpty() && microdistrict == 0 && birthYear == 0
            && categoryId == 0 && visitYear == 0 && subscriptionId == 0
            && subscriptionYear == 0;
    }

    bool operator==(const PersonFilter &other) const
    {
        return lastName == other.lastName
            && microdistrict == other.microdistrict
            && birthYear == other.birthYear
            && categoryId == other.categoryId
            && visitYear == other.visitYear
            && subscriptionId == other.subscriptionId
            && subscriptionYear == other.subscriptionYear;
    }

    bool operator!=(const PersonFilter &other) const
    {
        return !(*this == other);
    }
};

/// Прокси таблицы: сортировка, быстрый поиск и расширенный фильтр.
///
/// Числовые колонки (микрорайон, годы) сравниваются как числа, текстовые —
/// через QCollator с русской локалью, чтобы «Ё» и регистр не ломали порядок.
/// Быстрый поиск ищет по ФИО, адресу, телефону, категории, посещению и
/// подписке; несколько слов через пробел ищутся как «И».
class PersonProxyModel : public QSortFilterProxyModel
{
    Q_OBJECT

public:
    explicit PersonProxyModel(QObject *parent = nullptr);

    /// Быстрый поиск: подстрока в любом поле, без учёта регистра и «ё».
    /// Несколько слов через пробел — должны найтись все.
    void setQuickFilter(const QString &text);
    QString quickFilter() const { return m_quickFilter; }

    /// Расширенный фильтр; комбинируется с быстрым поиском через «И».
    void setAdvancedFilter(const PersonFilter &filter);
    void clearAdvancedFilter() { setAdvancedFilter(PersonFilter{}); }
    const PersonFilter &advancedFilter() const { return m_advancedFilter; }
    bool hasAdvancedFilter() const { return !m_advancedFilter.isEmpty(); }

protected:
    bool lessThan(const QModelIndex &left, const QModelIndex &right) const override;
    bool filterAcceptsRow(int sourceRow,
                          const QModelIndex &sourceParent) const override;

private:
    bool matchesAdvanced(const PersonRecord &record) const;

    QCollator m_collator;
    QString m_quickFilter;
    QStringList m_tokens;
    PersonFilter m_advancedFilter;
};
