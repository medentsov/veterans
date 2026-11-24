#pragma once

#include "data/PersonRepository.h"

#include <QVector>

class QWidget;

/// Печать списка и карточек через QPrintPreviewDialog (M8-03, M8-04).
///
/// Предпросмотр показывает страницы, а кнопка «Печать» в нём открывает
/// QPrintDialog: выбор принтера, копий, ориентации и полей.
class PrintService
{
public:
    /// Печать отмеченных записей единым альбомным списком (M8-02).
    static void printList(QWidget *parent, const QVector<PersonRecord> &records);

    /// Печать отмеченных записей карточками, каждая с новой страницы (M8-01).
    static void printCards(QWidget *parent, const QVector<PersonRecord> &records);
};
