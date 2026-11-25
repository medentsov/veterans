#pragma once

#include <QSqlDatabase>
#include <QString>

/// RAII-обёртка над транзакцией QSqlDatabase.
///
/// Если объект уничтожен без commit(), изменения откатываются — это защищает
/// от «полуприменённых» массовых операций (удаление нескольких записей,
/// будущий импорт из Excel).
class Transaction
{
public:
    explicit Transaction(const QSqlDatabase &db);
    ~Transaction();

    Transaction(const Transaction &) = delete;
    Transaction &operator=(const Transaction &) = delete;

    bool begin(QString *error = nullptr);
    bool commit(QString *error = nullptr);
    void rollback();

    bool isActive() const { return m_active; }

private:
    QSqlDatabase m_db;
    bool m_active = false;
};
