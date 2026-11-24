#include "ui/PersonTableModel.h"

#include <QApplication>
#include <QBrush>
#include <QColor>
#include <QPalette>
#include <QStringList>

#include <utility>

namespace {

QString placeholders()
{
    return QStringLiteral("—");
}

QString subscriptionText(const PersonRecord &record)
{
    if (record.subscriptionId == 0 || record.subscriptionName.isEmpty())
        return placeholders();
    if (record.subscriptionYear > 0) {
        return QStringLiteral("%1, %2")
            .arg(record.subscriptionName)
            .arg(record.subscriptionYear);
    }
    return record.subscriptionName;
}

QString addressText(const PersonRecord &record)
{
    QStringList parts;
    if (!record.street.isEmpty())
        parts << record.street;
    if (!record.house.isEmpty())
        parts << QStringLiteral("д. %1").arg(record.house);
    if (!record.building.isEmpty())
        parts << QStringLiteral("корп. %1").arg(record.building);
    if (!record.apartment.isEmpty())
        parts << QStringLiteral("кв. %1").arg(record.apartment);
    return parts.join(QStringLiteral(", "));
}

} // namespace

PersonTableModel::PersonTableModel(QObject *parent)
    : QAbstractTableModel(parent)
{
}

int PersonTableModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return m_rows.size();
}

int PersonTableModel::columnCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return ColumnCount;
}

QVariant PersonTableModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.size())
        return {};

    const PersonRecord &record = m_rows.at(index.row());

    switch (role) {
    case Qt::CheckStateRole:
        if (index.column() == Check) {
            return m_checkedIds.contains(record.id) ? Qt::Checked : Qt::Unchecked;
        }
        return {};

    case Qt::BackgroundRole:
        if (m_checkedIds.contains(record.id)) {
            // Лёгкая подсветка цветом темы: отмеченную строку видно,
            // но текст остаётся читаемым.
            QColor color = QApplication::palette().color(QPalette::Highlight);
            color.setAlpha(40);
            return QBrush(color);
        }
        return {};

    case Qt::TextAlignmentRole:
        switch (index.column()) {
        case Microdistrict:
        case BirthYear:
        case VisitYear:
            return int(Qt::AlignCenter);
        default:
            return int(Qt::AlignLeft | Qt::AlignVCenter);
        }

    case Qt::ToolTipRole:
        if (index.column() == FullName) {
            const QString address = addressText(record);
            if (!address.isEmpty())
                return address;
        }
        return {};

    case Qt::DisplayRole:
        switch (index.column()) {
        case Check:
            return {};
        case FullName:
            return record.fullName();
        case Microdistrict:
            return record.microdistrict;   // int — корректная сортировка
        case BirthYear:
            return record.birthDate.isValid() ? record.birthDate.year()
                                              : QVariant(placeholders());
        case Category:
            return record.categoryName.isEmpty() ? placeholders()
                                                 : record.categoryName;
        case VisitYear:
            return record.visitYear > 0 ? QVariant(record.visitYear)
                                        : QVariant(placeholders());
        case Subscription:
            return subscriptionText(record);
        default:
            return {};
        }

    default:
        return {};
    }
}

QVariant PersonTableModel::headerData(int section, Qt::Orientation orientation,
                                      int role) const
{
    if (role != Qt::DisplayRole)
        return {};

    if (orientation == Qt::Vertical)
        return section + 1;

    switch (section) {
    case Check:         return QString();
    case FullName:      return tr("ФИО");
    case Microdistrict: return tr("Микрорайон");
    case BirthYear:     return tr("Год рождения");
    case Category:      return tr("Льготная категория");
    case VisitYear:     return tr("Год посещения");
    case Subscription:  return tr("Подписка на газету");
    default:            return {};
    }
}

Qt::ItemFlags PersonTableModel::flags(const QModelIndex &index) const
{
    if (!index.isValid())
        return Qt::NoItemFlags;

    Qt::ItemFlags itemFlags = Qt::ItemIsEnabled | Qt::ItemIsSelectable;
    if (index.column() == Check)
        itemFlags |= Qt::ItemIsUserCheckable;
    return itemFlags;
}

bool PersonTableModel::setData(const QModelIndex &modelIndex,
                               const QVariant &value, int role)
{
    if (!modelIndex.isValid() || modelIndex.column() != Check
        || role != Qt::CheckStateRole) {
        return false;
    }

    const int id = m_rows.at(modelIndex.row()).id;
    const bool checked = value.toInt() == Qt::Checked;

    if (checked)
        m_checkedIds.insert(id);
    else
        m_checkedIds.remove(id);

    emit dataChanged(index(modelIndex.row(), 0),
                     index(modelIndex.row(), ColumnCount - 1),
                     {Qt::CheckStateRole, Qt::BackgroundRole});
    emit checkedChanged();
    return true;
}

void PersonTableModel::setRecords(QVector<PersonRecord> records)
{
    beginResetModel();
    m_rows = std::move(records);
    endResetModel();

    // Убираем отметки записей, которых больше нет в выборке.
    QSet<int> actual;
    actual.reserve(m_rows.size());
    for (const PersonRecord &record : std::as_const(m_rows))
        actual.insert(record.id);

    const QSet<int> before = m_checkedIds;
    m_checkedIds.intersect(actual);
    if (before != m_checkedIds)
        emit checkedChanged();
}

const PersonRecord *PersonTableModel::recordAt(int row) const
{
    if (row < 0 || row >= m_rows.size())
        return nullptr;
    return &m_rows.at(row);
}

void PersonTableModel::setCheckedIds(const QSet<int> &ids)
{
    QSet<int> actual;
    actual.reserve(m_rows.size());
    for (const PersonRecord &record : std::as_const(m_rows))
        actual.insert(record.id);

    QSet<int> next = ids;
    next.intersect(actual);

    if (next == m_checkedIds)
        return;

    m_checkedIds = next;

    if (!m_rows.isEmpty()) {
        emit dataChanged(index(0, 0), index(m_rows.size() - 1, ColumnCount - 1),
                         {Qt::CheckStateRole, Qt::BackgroundRole});
    }
    emit checkedChanged();
}

QVector<PersonRecord> PersonTableModel::checkedRecords() const
{
    QVector<PersonRecord> result;
    result.reserve(m_checkedIds.size());
    for (const PersonRecord &record : m_rows) {
        if (m_checkedIds.contains(record.id))
            result.append(record);
    }
    return result;
}

void PersonTableModel::clearChecked()
{
    if (m_checkedIds.isEmpty())
        return;

    m_checkedIds.clear();
    if (!m_rows.isEmpty()) {
        emit dataChanged(index(0, 0), index(m_rows.size() - 1, ColumnCount - 1),
                         {Qt::CheckStateRole, Qt::BackgroundRole});
    }
    emit checkedChanged();
}
