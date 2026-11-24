#pragma once

#include "import/IImportService.h"
#include "report/ExportScope.h"

#include <QMainWindow>
#include <QPoint>

#include <memory>

class QAction;
class QCloseEvent;
class QEvent;
class QLabel;
class QLineEdit;
class QTableView;
class QTimer;
class QToolBar;
class QToolButton;
class CheckHeader;
class PersonTableModel;
class PersonProxyModel;

/// Главное окно: панель инструментов, быстрый поиск, таблица записей, меню.
/// Доступность кнопок по правилам ТЗ — в M3-05, формы — в M4…M6.
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void closeEvent(QCloseEvent *event) override;

private slots:
    void onHeaderClicked(int section);
    void applyQuickFilter();
    void updateCounter();
    void updateActionStates();
    void updateCheckHeader();
    void onToggleAllChecked();
    void onTableContextMenu(const QPoint &pos);
    void onResetFilter();

    void onCreateRecord();
    void onViewRecord();
    void onEditRecord();
    void onDeleteRecords();
    void onPrintRecords();
    void onPrintCards();
    void onExtendedSearch();
    void onImportExcel();
    void onSettings();

private:
    void setupUi();
    void setupMenu();
    void setupToolBar();
    void setupTable();
    void restoreWindowState();
    void saveWindowState();
    void reloadData();
    void selectRecordById(int id);
    void openSelectedRecord(bool editMode);
    void exportExcel(ExportScope scope);
    void exportPdf(ExportScope scope);
    void exportRecords(ExportScope scope, bool pdf);
    void updateExportActions();
    void printChecked(bool cards);
    void restartApplication();

    PersonTableModel *m_model = nullptr;
    PersonProxyModel *m_proxy = nullptr;
    QTableView *m_view = nullptr;
    QToolBar *m_toolBar = nullptr;
    CheckHeader *m_header = nullptr;
    QLabel *m_emptyLabel = nullptr;

    QLineEdit *m_searchEdit = nullptr;
    QLabel *m_counterLabel = nullptr;
    QTimer *m_searchTimer = nullptr;
    QToolButton *m_resetFilterButton = nullptr;

    std::unique_ptr<IImportService> m_importService;

    QAction *m_createAction = nullptr;
    QAction *m_importAction = nullptr;
    QAction *m_editAction = nullptr;
    QAction *m_deleteAction = nullptr;
    QAction *m_printAction = nullptr;
    QAction *m_printListAction = nullptr;
    QAction *m_printCardsAction = nullptr;
    QAction *m_searchAction = nullptr;

    QAction *m_exportCheckedAction = nullptr;
    QAction *m_exportAllAction = nullptr;
    QAction *m_exportFilteredAction = nullptr;

    QAction *m_pdfCheckedAction = nullptr;
    QAction *m_pdfAllAction = nullptr;
    QAction *m_pdfFilteredAction = nullptr;
};
