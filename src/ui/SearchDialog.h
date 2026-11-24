#pragma once

#include "ui/PersonProxyModel.h"

#include <QDialog>

class QComboBox;
class QLineEdit;
class QSpinBox;

/// Модальный расширенный поиск по шести критериям.
///
/// Пустое поле или «любой» не ограничивает выборку. «Сбросить» очищает все
/// критерии и сразу применяет пустой фильтр, «Найти» — применяет введённые.
class SearchDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SearchDialog(QWidget *parent = nullptr);

    /// Показывает в форме ранее применённые критерии (в рамках сессии).
    void setFilter(const PersonFilter &filter);

    /// Текущие значения формы в виде критериев фильтра.
    PersonFilter filter() const;

private:
    void loadDictionaries();

    QLineEdit *m_lastName = nullptr;
    QSpinBox *m_microdistrict = nullptr;
    QSpinBox *m_birthYear = nullptr;
    QComboBox *m_category = nullptr;
    QSpinBox *m_visitYear = nullptr;
    QComboBox *m_subscription = nullptr;
    QSpinBox *m_subscriptionYear = nullptr;
};
