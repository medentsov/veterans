#include "data/Transaction.h"

#include <QDebug>
#include <QSqlError>

namespace {

bool fail(QString *error, const QString &text)
{
    if (error)
        *error = text;
    qWarning().noquote() << "[Transaction]" << text;
    return false;
}

} // namespace

Transaction::Transaction(const QSqlDatabase &db)
    : m_db(db)
{
}

Transaction::~Transaction()
{
    if (m_active)
        rollback();
}

bool Transaction::begin(QString *error)
{
    if (m_active)
        return fail(error, QStringLiteral("Транзакция уже начата."));
    if (!m_db.isOpen())
        return fail(error, QStringLiteral("База данных не открыта."));

    if (!m_db.transaction())
        return fail(error, QStringLiteral("Не удалось начать транзакцию: %1")
                               .arg(m_db.lastError().text().trimmed()));

    m_active = true;
    return true;
}

bool Transaction::commit(QString *error)
{
    if (!m_active)
        return fail(error, QStringLiteral("Транзакция не начата."));

    if (!m_db.commit()) {
        const QString text = m_db.lastError().text().trimmed();
        rollback();
        return fail(error, QStringLiteral("Не удалось зафиксировать изменения: %1")
                               .arg(text));
    }

    m_active = false;
    return true;
}

void Transaction::rollback()
{
    if (!m_active)
        return;

    if (!m_db.rollback()) {
        qWarning().noquote() << "[Transaction] откат не удался:"
                             << m_db.lastError().text().trimmed();
    }
    m_active = false;
}
