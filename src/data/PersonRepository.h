#pragma once

#include <QDate>
#include <QString>
#include <QVector>

/// Запись справочника. Названия категорий и подписки заполняются при чтении
/// (JOIN), чтобы таблица на главном экране не ходила в БД за каждой строкой.
struct PersonRecord
{
    int id = 0;
    QString lastName;
    QString firstName;
    QString middleName;
    QDate birthDate;
    int microdistrict = 0;
    QString street;
    QString house;
    QString building;
    QString apartment;
    int phoneType = 0;        ///< 0 — домашний, 1 — мобильный
    QString phone;            ///< только цифры, см. core/PhoneNumber
    int categoryId = 0;
    QString categoryName;
    int visitMonth = 0;   ///< 0 — не задано
    int visitYear = 0;    ///< 0 — не задано
    int subscriptionId = 0;
    QString subscriptionName;
    int subscriptionYear = 0;
    QString createdAt;
    QString updatedAt;

    QString fullName() const;
};

struct Category
{
    int id = 0;
    QString name;
};

struct Subscription
{
    int id = 0;
    QString name;
};

/// Доступ к таблице person и справочникам.
/// Все методы возвращают false и заполняют error при ошибке БД.
class PersonRepository
{
public:
    static QVector<PersonRecord> list(QString *error = nullptr);
    static bool fetch(int id, PersonRecord *record, QString *error = nullptr);

    /// Вставляет запись; в record->id возвращается новый идентификатор.
    static bool insert(PersonRecord *record, QString *error = nullptr);
    static bool update(const PersonRecord &record, QString *error = nullptr);
    static bool remove(int id, QString *error = nullptr);

    /// Удаляет несколько записей одной транзакцией (все или ни одной).
    static bool removeMany(const QVector<int> &ids, QString *error = nullptr);

    static QVector<Category> categories(QString *error = nullptr);
    static QVector<Subscription> subscriptions(QString *error = nullptr);
};
