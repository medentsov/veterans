#include "data/PersonRepository.h"

#include "data/Database.h"
#include "data/Transaction.h"

#include <QDateTime>
#include <QDebug>
#include <QSqlError>
#include <QSqlQuery>
#include <QStringList>
#include <QVariant>

namespace {

bool fail(QString *error, const QString &text)
{
    if (error)
        *error = text;
    qWarning().noquote() << "[PersonRepository]" << text;
    return false;
}

QString personSelect()
{
    return QStringLiteral(
        "SELECT p.id, p.last_name, p.first_name, p.middle_name, p.birth_date,"
        "       p.microdistrict, p.street, p.house, p.building, p.apartment,"
        "       p.category_id, p.visit_month, p.visit_year,"
        "       p.subscription_id, p.subscription_year,"
        "       p.created_at, p.updated_at,"
        "       c.name AS category_name, s.name AS subscription_name,"
        "       p.phone_type, p.phone"
        " FROM person p"
        " LEFT JOIN category c ON c.id = p.category_id"
        " LEFT JOIN subscription s ON s.id = p.subscription_id");
}

PersonRecord readRow(const QSqlQuery &query)
{
    PersonRecord r;
    r.id             = query.value(0).toInt();
    r.lastName       = query.value(1).toString();
    r.firstName      = query.value(2).toString();
    r.middleName     = query.value(3).toString();
    r.birthDate      = QDate::fromString(query.value(4).toString(), Qt::ISODate);
    r.microdistrict  = query.value(5).toInt();
    r.street         = query.value(6).toString();
    r.house          = query.value(7).toString();
    r.building       = query.value(8).toString();
    r.apartment      = query.value(9).toString();
    r.categoryId     = query.value(10).toInt();
    r.visitMonth     = query.value(11).toInt();
    r.visitYear      = query.value(12).toInt();
    r.subscriptionId = query.value(13).toInt();
    r.subscriptionYear = query.value(14).toInt();
    r.createdAt      = query.value(15).toString();
    r.updatedAt      = query.value(16).toString();
    r.categoryName     = query.value(17).toString();
    r.subscriptionName = query.value(18).toString();
    r.phoneType        = query.value(19).toInt();
    r.phone            = query.value(20).toString();
    return r;
}

QString nowIso()
{
    return QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
}

/// 0 в модели означает «не задано» → NULL в базе.
QVariant optionalId(int value)
{
    return value > 0 ? QVariant(value) : QVariant();
}

/// Null QString драйвер SQLite отправляет как NULL, а текстовые колонки
/// объявлены NOT NULL. Пустая (не null) строка сохраняется как ''.
QString textOrEmpty(const QString &value)
{
    return value.isNull() ? QStringLiteral("") : value;
}

void bindPerson(QSqlQuery &query, const PersonRecord &record)
{
    query.addBindValue(textOrEmpty(record.lastName));
    query.addBindValue(textOrEmpty(record.firstName));
    query.addBindValue(textOrEmpty(record.middleName));
    query.addBindValue(record.birthDate.toString(Qt::ISODate));
    query.addBindValue(record.microdistrict);
    query.addBindValue(textOrEmpty(record.street));
    query.addBindValue(textOrEmpty(record.house));
    query.addBindValue(textOrEmpty(record.building));
    query.addBindValue(textOrEmpty(record.apartment));
    query.addBindValue(optionalId(record.categoryId));
    query.addBindValue(optionalId(record.visitMonth));
    query.addBindValue(optionalId(record.visitYear));
    query.addBindValue(optionalId(record.subscriptionId));
    query.addBindValue(optionalId(record.subscriptionYear));
    query.addBindValue(record.phoneType);
    query.addBindValue(textOrEmpty(record.phone));
}

} // namespace

QString PersonRecord::fullName() const
{
    QStringList parts;
    if (!lastName.isEmpty())
        parts << lastName;
    if (!firstName.isEmpty())
        parts << firstName;
    if (!middleName.isEmpty())
        parts << middleName;
    return parts.join(QLatin1Char(' '));
}

QVector<PersonRecord> PersonRepository::list(QString *error)
{
    QVector<PersonRecord> result;

    QSqlDatabase db = Database::connection();
    if (!db.isOpen()) {
        fail(error, QStringLiteral("База данных не открыта."));
        return result;
    }

    QSqlQuery query(db);
    const QString sql = personSelect()
        + QStringLiteral(" WHERE p.deleted_at IS NULL"
                         " ORDER BY p.last_name, p.first_name, p.middle_name");
    if (!query.exec(sql)) {
        fail(error, QStringLiteral("Не удалось прочитать список: %1")
                        .arg(query.lastError().text().trimmed()));
        return result;
    }

    while (query.next())
        result.append(readRow(query));
    return result;
}

bool PersonRepository::fetch(int id, PersonRecord *record, QString *error)
{
    if (!record)
        return fail(error, QStringLiteral("Не передан получатель записи."));

    QSqlDatabase db = Database::connection();
    if (!db.isOpen())
        return fail(error, QStringLiteral("База данных не открыта."));

    QSqlQuery query(db);
    query.prepare(personSelect() + QStringLiteral(" WHERE p.id = ?"));
    query.addBindValue(id);

    if (!query.exec())
        return fail(error, QStringLiteral("Не удалось прочитать запись: %1")
                               .arg(query.lastError().text().trimmed()));

    if (!query.next())
        return fail(error, QStringLiteral("Запись не найдена (id %1).").arg(id));

    *record = readRow(query);
    return true;
}

bool PersonRepository::insert(PersonRecord *record, QString *error)
{
    if (!record)
        return fail(error, QStringLiteral("Не передана запись."));
    if (!record->birthDate.isValid())
        return fail(error, QStringLiteral("Не указана дата рождения."));

    QSqlDatabase db = Database::connection();
    if (!db.isOpen())
        return fail(error, QStringLiteral("База данных не открыта."));

    const QString timestamp = nowIso();

    QSqlQuery query(db);
    query.prepare(QStringLiteral(
        "INSERT INTO person (last_name, first_name, middle_name, birth_date,"
        "                    microdistrict, street, house, building, apartment,"
        "                    category_id, visit_month, visit_year,"
        "                    subscription_id, subscription_year,"
        "                    phone_type, phone,"
        "                    created_at, updated_at)"
        " VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"));
    bindPerson(query, *record);
    query.addBindValue(timestamp);
    query.addBindValue(timestamp);

    if (!query.exec())
        return fail(error, QStringLiteral("Не удалось сохранить запись: %1")
                               .arg(query.lastError().text().trimmed()));

    record->id = query.lastInsertId().toInt();
    record->createdAt = timestamp;
    record->updatedAt = timestamp;
    return true;
}

bool PersonRepository::update(const PersonRecord &record, QString *error)
{
    if (record.id <= 0)
        return fail(error, QStringLiteral("Не выбрана запись для изменения."));
    if (!record.birthDate.isValid())
        return fail(error, QStringLiteral("Не указана дата рождения."));

    QSqlDatabase db = Database::connection();
    if (!db.isOpen())
        return fail(error, QStringLiteral("База данных не открыта."));

    QSqlQuery query(db);
    query.prepare(QStringLiteral(
        "UPDATE person SET"
        "  last_name = ?, first_name = ?, middle_name = ?, birth_date = ?,"
        "  microdistrict = ?, street = ?, house = ?, building = ?, apartment = ?,"
        "  category_id = ?, visit_month = ?, visit_year = ?,"
        "  subscription_id = ?, subscription_year = ?,"
        "  phone_type = ?, phone = ?,"
        "  updated_at = ?"
        " WHERE id = ?"));
    bindPerson(query, record);
    query.addBindValue(nowIso());
    query.addBindValue(record.id);

    if (!query.exec())
        return fail(error, QStringLiteral("Не удалось изменить запись: %1")
                               .arg(query.lastError().text().trimmed()));
    if (query.numRowsAffected() == 0)
        return fail(error, QStringLiteral("Запись не найдена (id %1).").arg(record.id));
    return true;
}

bool PersonRepository::remove(int id, QString *error)
{
    QSqlDatabase db = Database::connection();
    if (!db.isOpen())
        return fail(error, QStringLiteral("База данных не открыта."));

    QSqlQuery query(db);
    query.prepare(QStringLiteral("DELETE FROM person WHERE id = ?"));
    query.addBindValue(id);

    if (!query.exec())
        return fail(error, QStringLiteral("Не удалось удалить запись: %1")
                               .arg(query.lastError().text().trimmed()));
    if (query.numRowsAffected() == 0)
        return fail(error, QStringLiteral("Запись не найдена (id %1).").arg(id));
    return true;
}

bool PersonRepository::removeMany(const QVector<int> &ids, QString *error)
{
    if (ids.isEmpty())
        return true;

    QSqlDatabase db = Database::connection();
    if (!db.isOpen())
        return fail(error, QStringLiteral("База данных не открыта."));

    QStringList placeholders;
    placeholders.reserve(ids.size());
    for (int i = 0; i < ids.size(); ++i)
        placeholders.append(QStringLiteral("?"));

    Transaction transaction(db);
    if (!transaction.begin(error))
        return false;

    QSqlQuery query(db);
    query.prepare(QStringLiteral("DELETE FROM person WHERE id IN (%1)")
                      .arg(placeholders.join(QStringLiteral(", "))));
    for (int id : ids)
        query.addBindValue(id);

    if (!query.exec()) {
        transaction.rollback();
        return fail(error, QStringLiteral("Не удалось удалить записи: %1")
                               .arg(query.lastError().text().trimmed()));
    }

    return transaction.commit(error);
}

QVector<Category> PersonRepository::categories(QString *error)
{
    QVector<Category> result;

    QSqlDatabase db = Database::connection();
    if (!db.isOpen()) {
        fail(error, QStringLiteral("База данных не открыта."));
        return result;
    }

    QSqlQuery query(db);
    if (!query.exec(QStringLiteral(
            "SELECT id, name FROM category ORDER BY sort_order, name"))) {
        fail(error, QStringLiteral("Не удалось прочитать категории: %1")
                        .arg(query.lastError().text().trimmed()));
        return result;
    }

    while (query.next())
        result.append({query.value(0).toInt(), query.value(1).toString()});
    return result;
}

QVector<Subscription> PersonRepository::subscriptions(QString *error)
{
    QVector<Subscription> result;

    QSqlDatabase db = Database::connection();
    if (!db.isOpen()) {
        fail(error, QStringLiteral("База данных не открыта."));
        return result;
    }

    QSqlQuery query(db);
    if (!query.exec(QStringLiteral(
            "SELECT id, name FROM subscription ORDER BY sort_order, name"))) {
        fail(error, QStringLiteral("Не удалось прочитать подписки: %1")
                        .arg(query.lastError().text().trimmed()));
        return result;
    }

    while (query.next())
        result.append({query.value(0).toInt(), query.value(1).toString()});
    return result;
}
