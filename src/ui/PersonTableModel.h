#pragma once

#include "data/PersonRepository.h"

#include <QAbstractTableModel>
#include <QSet>

/// Модель таблицы главного окна: 7 колонок по ТЗ.
///
/// Состояние галочек хранится по id записи, а не по номеру строки, поэтому
/// при сортировке и фильтрации отметки не «съезжают» на другие записи.
class PersonTableModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    enum Column {
        Check = 0,
        FullName,
        Microdistrict,
        BirthYear,
        Category,
        VisitYear,
        Subscription,
        ColumnCount
    };

    explicit PersonTableModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    int columnCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    bool setData(const QModelIndex &index, const QVariant &value,
                 int role = Qt::EditRole) override;

    void setRecords(QVector<PersonRecord> records);
    const PersonRecord *recordAt(int row) const;

    QSet<int> checkedIds() const { return m_checkedIds; }
    bool isChecked(int id) const { return m_checkedIds.contains(id); }
    QVector<PersonRecord> checkedRecords() const;
    void setCheckedIds(const QSet<int> &ids);
    void clearChecked();

signals:
    /// Изменился набор отмеченных записей (кнопки «Печать»/«Удалить»).
    void checkedChanged();

private:
    QVector<PersonRecord> m_rows;
    QSet<int> m_checkedIds;
};
