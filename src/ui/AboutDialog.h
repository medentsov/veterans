#pragma once

#include <QDialog>

/// Модальное окно «О программе»: название, версия, назначение, версия Qt.
class AboutDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AboutDialog(QWidget *parent = nullptr);
};
