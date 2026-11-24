#include "ui/MainWindow.h"

#include "core/AppPaths.h"
#include "data/PersonRepository.h"
#include "import/NullImportService.h"
#include "report/ExcelExporter.h"
#include "report/ListPdfExporter.h"
#include "report/PrintService.h"
#include "ui/AboutDialog.h"
#include "ui/CheckHeader.h"
#include "ui/PersonDialog.h"
#include "ui/PersonProxyModel.h"
#include "ui/PersonTableModel.h"
#include "ui/SearchDialog.h"
#include "ui/SettingsDialog.h"

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QCloseEvent>
#include <QDate>
#include <QDebug>
#include <QDir>
#include <QEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QFont>
#include <QHeaderView>
#include <QIcon>
#include <QItemSelectionModel>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPalette>
#include <QProcess>
#include <QPushButton>
#include <QSettings>
#include <QShortcut>
#include <QSize>
#include <QSizePolicy>
#include <QStandardPaths>
#include <QStatusBar>
#include <QStringList>
#include <QTableView>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QWidget>

namespace {

/// Собирает записи выбранной области экспорта:
/// «отмеченные» — все галочки (даже скрытые фильтром), «все» — весь список
/// в исходном порядке, «результат фильтра» — только видимые строки
/// в порядке таблицы (с учётом сортировки).
QVector<PersonRecord> recordsForScope(const PersonTableModel *model,
                                      const PersonProxyModel *proxy,
                                      ExportScope scope)
{
    QVector<PersonRecord> records;
    if (!model || !proxy)
        return records;

    if (scope == ExportScope::Checked)
        return model->checkedRecords();

    if (scope == ExportScope::Filtered) {
        records.reserve(proxy->rowCount());
        for (int row = 0; row < proxy->rowCount(); ++row) {
            const QModelIndex sourceIndex = proxy->mapToSource(
                proxy->index(row, PersonTableModel::FullName));
            if (const PersonRecord *record = model->recordAt(sourceIndex.row()))
                records.append(*record);
        }
        return records;
    }

    records.reserve(model->rowCount());
    for (int row = 0; row < model->rowCount(); ++row) {
        if (const PersonRecord *record = model->recordAt(row))
            records.append(*record);
    }
    return records;
}

QString defaultExportFileName(ExportScope scope, const QString &extension)
{
    QString suffix;
    switch (scope) {
    case ExportScope::Checked:
        suffix = QStringLiteral("_отмеченные");
        break;
    case ExportScope::Filtered:
        suffix = QStringLiteral("_фильтр");
        break;
    case ExportScope::All:
        break;
    }

    return QStringLiteral("Ветераны%1_%2.%3")
        .arg(suffix,
             QDate::currentDate().toString(QStringLiteral("yyyy-MM-dd")),
             extension);
}

QString defaultExportDir()
{
    QString dir = QStandardPaths::writableLocation(
        QStandardPaths::DocumentsLocation);
    if (dir.isEmpty())
        dir = QDir::homePath();
    return dir;
}

/// Папка последнего экспорта запоминается между запусками (M7-04).
QString exportStartDir()
{
    QSettings settings(AppPaths::settingsFile(), QSettings::IniFormat);
    const QString saved =
        settings.value(QStringLiteral("Export/lastDir")).toString();
    if (!saved.isEmpty() && QDir(saved).exists())
        return saved;
    return defaultExportDir();
}

void rememberExportDir(const QString &filePath)
{
    const QDir dir = QFileInfo(filePath).absoluteDir();
    if (!dir.exists())
        return;

    QSettings settings(AppPaths::settingsFile(), QSettings::IniFormat);
    settings.setValue(QStringLiteral("Export/lastDir"), dir.absolutePath());
    settings.sync();
}

} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setupUi();
}

void MainWindow::setupUi()
{
    setWindowTitle(QStringLiteral("Ветераны"));
    setWindowIcon(QIcon(QStringLiteral(":/icons/veterans-256.png")));
    resize(1100, 700);
    setMinimumSize(1000, 520);

    // Точка вызова импорта (M10-02): пока пустая реализация.
    m_importService = std::make_unique<NullImportService>();

    setupMenu();
    setupTable();
    setupToolBar();
    restoreWindowState();

    reloadData();
}

void MainWindow::setupMenu()
{
    auto *fileMenu = menuBar()->addMenu(tr("Файл"));

    // Заглушка будущего импорта (M10-01): пункт виден, но неактивен.
    m_importAction = fileMenu->addAction(tr("Импорт из Excel"));
    m_importAction->setToolTip(
        tr("Импорт из Excel — будет реализовано позже"));
    m_importAction->setStatusTip(
        tr("Функция появится в следующих версиях"));
    m_importAction->setEnabled(false);
    connect(m_importAction, &QAction::triggered,
            this, &MainWindow::onImportExcel);
    fileMenu->addSeparator();

    auto *exportMenu = fileMenu->addMenu(tr("Экспорт в Excel"));
    exportMenu->setObjectName(QStringLiteral("exportExcelMenu"));

    m_exportCheckedAction =
        exportMenu->addAction(tr("Отмеченные записи"));
    m_exportCheckedAction->setToolTip(
        tr("Выгрузить записи, отмеченные галочками"));
    connect(m_exportCheckedAction, &QAction::triggered, this, [this] {
        exportExcel(ExportScope::Checked);
    });

    m_exportAllAction = exportMenu->addAction(tr("Все записи"));
    m_exportAllAction->setToolTip(
        tr("Выгрузить весь список независимо от фильтра"));
    connect(m_exportAllAction, &QAction::triggered, this, [this] {
        exportExcel(ExportScope::All);
    });

    m_exportFilteredAction =
        exportMenu->addAction(tr("Результат фильтра"));
    m_exportFilteredAction->setToolTip(
        tr("Выгрузить строки, видимые с учётом быстрого поиска "
           "и расширенного фильтра"));
    connect(m_exportFilteredAction, &QAction::triggered, this, [this] {
        exportExcel(ExportScope::Filtered);
    });

    auto *pdfMenu = fileMenu->addMenu(tr("Экспорт в PDF"));
    pdfMenu->setObjectName(QStringLiteral("exportPdfMenu"));

    m_pdfCheckedAction = pdfMenu->addAction(tr("Отмеченные записи"));
    m_pdfCheckedAction->setToolTip(
        tr("Выгрузить записи, отмеченные галочками, в PDF-список"));
    connect(m_pdfCheckedAction, &QAction::triggered, this, [this] {
        exportPdf(ExportScope::Checked);
    });

    m_pdfAllAction = pdfMenu->addAction(tr("Все записи"));
    m_pdfAllAction->setToolTip(
        tr("Выгрузить весь список независимо от фильтра в PDF"));
    connect(m_pdfAllAction, &QAction::triggered, this, [this] {
        exportPdf(ExportScope::All);
    });

    m_pdfFilteredAction = pdfMenu->addAction(tr("Результат фильтра"));
    m_pdfFilteredAction->setToolTip(
        tr("Выгрузить строки, видимые с учётом быстрого поиска "
           "и расширенного фильтра, в PDF"));
    connect(m_pdfFilteredAction, &QAction::triggered, this, [this] {
        exportPdf(ExportScope::Filtered);
    });

    auto *printMenu = menuBar()->addMenu(tr("Печать"));
    printMenu->setObjectName(QStringLiteral("printMenu"));

    m_printListAction = printMenu->addAction(tr("Записи в списке"));
    m_printListAction->setToolTip(
        tr("Напечатать отмеченные записи единым списком (Ctrl+P)"));
    m_printListAction->setShortcut(QKeySequence::Print);
    m_printListAction->setEnabled(false);
    connect(m_printListAction, &QAction::triggered,
            this, &MainWindow::onPrintRecords);

    m_printCardsAction = printMenu->addAction(tr("Каждую отдельно"));
    m_printCardsAction->setToolTip(
        tr("Напечатать отмеченные записи карточками, каждую с новой страницы"));
    m_printCardsAction->setEnabled(false);
    connect(m_printCardsAction, &QAction::triggered,
            this, &MainWindow::onPrintCards);

    updateExportActions();

    auto *helpMenu = menuBar()->addMenu(tr("Справка"));
    auto *aboutAction = helpMenu->addAction(tr("О программе"));
    connect(aboutAction, &QAction::triggered, this, [this] {
        AboutDialog dialog(this);
        dialog.exec();
    });
}

void MainWindow::setupToolBar()
{
    m_toolBar = addToolBar(tr("Основная"));
    m_toolBar->setObjectName(QStringLiteral("mainToolBar"));
    m_toolBar->setMovable(false);
    m_toolBar->setFloatable(false);
    m_toolBar->setIconSize(QSize(18, 18));
    m_toolBar->setToolButtonStyle(Qt::ToolButtonTextOnly);

    m_createAction = m_toolBar->addAction(tr("Создать"));
    m_createAction->setToolTip(tr("Создать новую запись (Ctrl+N)"));
    m_createAction->setShortcut(QKeySequence::New);
    connect(m_createAction, &QAction::triggered,
            this, &MainWindow::onCreateRecord);

    m_editAction = m_toolBar->addAction(tr("Редактировать"));
    m_editAction->setToolTip(tr("Открыть выбранную запись на правку (Ctrl+E)"));
    m_editAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+E")));
    m_editAction->setEnabled(false);
    connect(m_editAction, &QAction::triggered,
            this, &MainWindow::onEditRecord);

    m_deleteAction = m_toolBar->addAction(tr("Удалить"));
    m_deleteAction->setToolTip(tr("Удалить отмеченные записи"));
    m_deleteAction->setEnabled(false);
    connect(m_deleteAction, &QAction::triggered,
            this, &MainWindow::onDeleteRecords);

    m_printAction = m_toolBar->addAction(tr("Печать"));
    m_printAction->setToolTip(tr("Печать отмеченных записей списком (Ctrl+P)"));
    m_printAction->setEnabled(false);
    connect(m_printAction, &QAction::triggered,
            this, &MainWindow::onPrintRecords);

    m_searchAction = m_toolBar->addAction(tr("Поиск"));
    m_searchAction->setToolTip(tr("Расширенный поиск по всем полям (Ctrl+F)"));
    m_searchAction->setShortcut(QKeySequence::Find);
    connect(m_searchAction, &QAction::triggered,
            this, &MainWindow::onExtendedSearch);

    auto *spacer = new QWidget(this);
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_toolBar->addWidget(spacer);

    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText(tr("Быстрый поиск"));
    m_searchEdit->setClearButtonEnabled(true);
    m_searchEdit->setFixedWidth(280);
    m_searchEdit->setToolTip(tr("Поиск по ФИО, адресу, телефону, "
                                "категории, посещению и подписке"));
    m_toolBar->addWidget(m_searchEdit);

    m_resetFilterButton = new QToolButton(this);
    m_resetFilterButton->setText(tr("Сбросить"));
    m_resetFilterButton->setToolTip(tr("Фильтр активен. Сбросить поиск"));
    m_resetFilterButton->setAccessibleName(tr("Сбросить поиск"));
    m_resetFilterButton->setAutoRaise(true);
    connect(m_resetFilterButton, &QToolButton::clicked,
            this, &MainWindow::onResetFilter);
    m_toolBar->addWidget(m_resetFilterButton);
    m_resetFilterButton->setVisible(false);

    m_counterLabel = new QLabel(this);
    m_counterLabel->setMinimumWidth(140);
    m_counterLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_toolBar->addWidget(m_counterLabel);

    m_toolBar->addSeparator();

    auto *settingsButton = new QToolButton(this);
    settingsButton->setText(QStringLiteral("⚙"));
    settingsButton->setToolTip(tr("Настройки"));
    settingsButton->setAccessibleName(tr("Настройки"));
    settingsButton->setAutoRaise(true);
    settingsButton->setFixedSize(32, 32);
    QFont settingsFont = settingsButton->font();
    if (settingsFont.pointSize() > 0)
        settingsFont.setPointSize(settingsFont.pointSize() + 4);
    else
        settingsFont.setPixelSize(20);
    settingsButton->setFont(settingsFont);
    connect(settingsButton, &QToolButton::clicked,
            this, &MainWindow::onSettings);
    m_toolBar->addWidget(settingsButton);

    m_searchTimer = new QTimer(this);
    m_searchTimer->setSingleShot(true);
    m_searchTimer->setInterval(250);
    connect(m_searchTimer, &QTimer::timeout,
            this, &MainWindow::applyQuickFilter);

    connect(m_searchEdit, &QLineEdit::textChanged, this,
            [this](const QString &text) {
                if (text.trimmed().isEmpty()) {
                    m_searchTimer->stop();
                    applyQuickFilter();
                } else {
                    m_searchTimer->start();
                }
            });
}

void MainWindow::setupTable()
{
    m_model = new PersonTableModel(this);

    m_proxy = new PersonProxyModel(this);
    m_proxy->setSourceModel(m_model);

    m_view = new QTableView(this);
    m_view->setModel(m_proxy);
    m_view->setAlternatingRowColors(true);
    m_view->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_view->setSelectionMode(QAbstractItemView::SingleSelection);
    m_view->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_view->setWordWrap(false);
    m_view->setShowGrid(true);

    m_view->verticalHeader()->setVisible(false);
    m_view->verticalHeader()->setDefaultSectionSize(32);

    m_header = new CheckHeader(Qt::Horizontal, m_view);
    m_header->setToolTip(tr("Отметить все видимые записи"));
    m_view->setHorizontalHeader(m_header);

    m_header->setSectionResizeMode(PersonTableModel::FullName,
                                   QHeaderView::Stretch);
    m_header->setSectionResizeMode(PersonTableModel::Check,
                                   QHeaderView::Fixed);
    m_view->setColumnWidth(PersonTableModel::Check, 36);
    m_view->setColumnWidth(PersonTableModel::Microdistrict, 110);
    m_view->setColumnWidth(PersonTableModel::BirthYear, 120);
    m_view->setColumnWidth(PersonTableModel::Category, 210);
    m_view->setColumnWidth(PersonTableModel::VisitYear, 130);
    m_view->setColumnWidth(PersonTableModel::Subscription, 190);

    connect(m_header, &QHeaderView::sectionClicked,
            this, &MainWindow::onHeaderClicked);
    connect(m_header, &CheckHeader::toggleAllRequested,
            this, &MainWindow::onToggleAllChecked);

    connect(m_view->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, &MainWindow::updateActionStates);
    connect(m_model, &PersonTableModel::checkedChanged,
            this, &MainWindow::updateActionStates);
    connect(m_model, &PersonTableModel::checkedChanged,
            this, &MainWindow::updateCheckHeader);

    m_view->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_view, &QWidget::customContextMenuRequested,
            this, &MainWindow::onTableContextMenu);

    connect(m_view, &QTableView::doubleClicked,
            this, [this](const QModelIndex &index) {
                if (index.isValid())
                    onViewRecord();
            });

    auto *returnShortcut =
        new QShortcut(QKeySequence(QStringLiteral("Return")), m_view);
    returnShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(returnShortcut, &QShortcut::activated,
            this, &MainWindow::onViewRecord);

    auto *enterShortcut =
        new QShortcut(QKeySequence(QStringLiteral("Enter")), m_view);
    enterShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(enterShortcut, &QShortcut::activated,
            this, &MainWindow::onViewRecord);

    auto *deleteShortcut =
        new QShortcut(QKeySequence(QStringLiteral("Del")), m_view);
    deleteShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(deleteShortcut, &QShortcut::activated,
            this, &MainWindow::onDeleteRecords);

    setCentralWidget(m_view);

    m_emptyLabel = new QLabel(m_view->viewport());
    m_emptyLabel->setAlignment(Qt::AlignCenter);
    m_emptyLabel->setWordWrap(true);
    m_emptyLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_emptyLabel->hide();

    QPalette emptyPalette = m_emptyLabel->palette();
    emptyPalette.setColor(QPalette::WindowText,
                          emptyPalette.color(QPalette::PlaceholderText));
    m_emptyLabel->setPalette(emptyPalette);

    QFont emptyFont = m_emptyLabel->font();
    if (emptyFont.pointSize() > 0)
        emptyFont.setPointSize(emptyFont.pointSize() + 1);
    m_emptyLabel->setFont(emptyFont);

    m_view->viewport()->installEventFilter(this);
}

void MainWindow::restoreWindowState()
{
    QSettings settings(AppPaths::settingsFile(), QSettings::IniFormat);
    settings.beginGroup(QStringLiteral("MainWindow"));

    const QByteArray geometry =
        settings.value(QStringLiteral("geometry")).toByteArray();
    if (!geometry.isEmpty())
        restoreGeometry(geometry);

    const QByteArray headerState =
        settings.value(QStringLiteral("header")).toByteArray();
    if (!headerState.isEmpty() && m_header)
        m_header->restoreState(headerState);

    settings.endGroup();
}

void MainWindow::saveWindowState()
{
    QSettings settings(AppPaths::settingsFile(), QSettings::IniFormat);
    settings.beginGroup(QStringLiteral("MainWindow"));
    settings.setValue(QStringLiteral("geometry"), saveGeometry());
    if (m_header)
        settings.setValue(QStringLiteral("header"), m_header->saveState());
    settings.endGroup();
    settings.sync();
}

void MainWindow::reloadData()
{
    QString error;
    const QVector<PersonRecord> records = PersonRepository::list(&error);
    if (!error.isEmpty()) {
        qWarning().noquote() << "[MainWindow] не удалось загрузить список:"
                             << error;
        QMessageBox::warning(this, tr("Ветераны"), error);
        return;
    }

    m_model->setRecords(records);
    m_view->sortByColumn(PersonTableModel::FullName, Qt::AscendingOrder);
    applyQuickFilter();
}

void MainWindow::applyQuickFilter()
{
    m_proxy->setQuickFilter(m_searchEdit->text());
    updateCounter();
}

void MainWindow::onResetFilter()
{
    m_proxy->clearAdvancedFilter();
    m_searchEdit->clear();
    applyQuickFilter();
    m_searchEdit->setFocus(Qt::OtherFocusReason);
}

void MainWindow::updateCounter()
{
    const int total = m_model->rowCount();
    const int found = m_proxy->rowCount();
    const bool hasQuick = !m_proxy->quickFilter().isEmpty();
    const bool hasAdvanced = m_proxy->hasAdvancedFilter();
    const bool hasFilter = hasQuick || hasAdvanced;

    if (m_resetFilterButton) {
        m_resetFilterButton->setVisible(hasFilter);
        m_resetFilterButton->setToolTip(
            hasQuick && hasAdvanced
                ? tr("Активны быстрый поиск и расширенный фильтр. Сбросить оба")
                : (hasAdvanced
                       ? tr("Активен расширенный фильтр. Сбросить")
                       : tr("Активен быстрый поиск. Сбросить")));
    }

    if (!hasFilter) {
        const QString text = tr("Записей: %1").arg(total);
        m_counterLabel->setText(text);
        statusBar()->showMessage(text);
    } else {
        const QString text = tr("Найдено: %1 из %2").arg(found).arg(total);
        m_counterLabel->setText(text);
        statusBar()->showMessage(text);
    }

    if (m_emptyLabel) {
        const bool noRecords = total == 0;
        const bool noMatches = total > 0 && found == 0;

        if (noRecords) {
            m_emptyLabel->setText(
                tr("Записей пока нет\n\n"
                   "Нажмите «Создать», чтобы добавить первую запись."));
        } else if (noMatches) {
            m_emptyLabel->setText(
                tr("Ничего не найдено\n\n"
                   "Измените запрос или сбросьте фильтры."));
        }

        m_emptyLabel->setVisible(noRecords || noMatches);
        if (noRecords || noMatches) {
            m_emptyLabel->setGeometry(m_view->viewport()->rect());
            m_emptyLabel->raise();
        }
    }

    updateCheckHeader();
    updateActionStates();
}

void MainWindow::updateCheckHeader()
{
    if (!m_header || !m_model || !m_proxy)
        return;

    const int visible = m_proxy->rowCount();
    int checked = 0;

    for (int row = 0; row < visible; ++row) {
        const QModelIndex source = m_proxy->mapToSource(
            m_proxy->index(row, PersonTableModel::Check));
        const PersonRecord *record = m_model->recordAt(source.row());
        if (record && m_model->isChecked(record->id))
            ++checked;
    }

    Qt::CheckState state = Qt::Unchecked;
    if (visible > 0 && checked == visible)
        state = Qt::Checked;
    else if (checked > 0)
        state = Qt::PartiallyChecked;

    m_header->setCheckState(state);
}

void MainWindow::updateActionStates()
{
    if (!m_model || !m_view || !m_editAction)
        return;

    const bool hasSelection = m_view->selectionModel()
        && m_view->selectionModel()->hasSelection();
    const int checked = m_model->checkedIds().size();

    m_editAction->setEnabled(hasSelection);
    m_deleteAction->setEnabled(checked > 0);
    m_printAction->setEnabled(checked > 0);
    if (m_printListAction) {
        m_printListAction->setEnabled(checked > 0);
        m_printListAction->setText(tr("Записи в списке (%1)").arg(checked));
    }
    if (m_printCardsAction) {
        m_printCardsAction->setEnabled(checked > 0);
        m_printCardsAction->setText(tr("Каждую отдельно (%1)").arg(checked));
    }

    m_deleteAction->setToolTip(
        checked > 0 ? tr("Удалить отмеченные записи: %1").arg(checked)
                    : tr("Отметьте записи для удаления"));
    m_printAction->setToolTip(
        checked > 0 ? tr("Печать отмеченных записей: %1 (Ctrl+P)").arg(checked)
                    : tr("Отметьте записи для печати (Ctrl+P)"));

    updateExportActions();
}

void MainWindow::updateExportActions()
{
    const int total = m_model ? m_model->rowCount() : 0;
    const int checked = m_model ? m_model->checkedIds().size() : 0;
    const int found = m_proxy ? m_proxy->rowCount() : 0;
    const bool hasFilter = m_proxy
        && (m_proxy->hasAdvancedFilter() || !m_proxy->quickFilter().isEmpty());

    const QString checkedText =
        tr("Отмеченные записи (%1)").arg(checked);
    const QString allText = tr("Все записи (%1)").arg(total);
    const QString filteredText =
        tr("Результат фильтра (%1)").arg(found);
    const QString filteredTip =
        hasFilter ? tr("Выгрузить строки, видимые с учётом быстрого поиска "
                       "и расширенного фильтра: %1").arg(found)
                  : tr("Доступно при активном быстром поиске "
                       "или расширенном фильтре");

    const auto apply = [&](QAction *checkedAction, QAction *allAction,
                           QAction *filteredAction) {
        if (!checkedAction || !allAction || !filteredAction)
            return;

        checkedAction->setText(checkedText);
        checkedAction->setEnabled(checked > 0);

        allAction->setText(allText);
        allAction->setEnabled(total > 0);

        filteredAction->setText(filteredText);
        filteredAction->setEnabled(hasFilter && found > 0);
        filteredAction->setToolTip(filteredTip);
    };

    apply(m_exportCheckedAction, m_exportAllAction, m_exportFilteredAction);
    apply(m_pdfCheckedAction, m_pdfAllAction, m_pdfFilteredAction);
}

void MainWindow::exportExcel(ExportScope scope)
{
    exportRecords(scope, false);
}

void MainWindow::exportPdf(ExportScope scope)
{
    exportRecords(scope, true);
}

void MainWindow::exportRecords(ExportScope scope, bool pdf)
{
    const QVector<PersonRecord> records =
        recordsForScope(m_model, m_proxy, scope);
    if (records.isEmpty()) {
        QMessageBox::information(this, tr("Экспорт"),
                                 tr("Нет записей для экспорта."));
        return;
    }

    const QString title = pdf ? tr("Экспорт в PDF") : tr("Экспорт в Excel");
    const QString extension = pdf ? QStringLiteral("pdf")
                                  : QStringLiteral("xlsx");
    const QString filter = pdf ? tr("Файлы PDF (*.pdf)")
                               : tr("Файлы Excel (*.xlsx)");
    const QString suggestedPath = QDir(exportStartDir())
                                      .filePath(defaultExportFileName(
                                          scope, extension));

    QString filePath = QFileDialog::getSaveFileName(
        this, title, suggestedPath, filter);
    if (filePath.isEmpty())
        return;

    if (!filePath.endsWith(QStringLiteral(".") + extension,
                           Qt::CaseInsensitive)) {
        filePath += QStringLiteral(".") + extension;
    }
    rememberExportDir(filePath);

    QString error;
    QApplication::setOverrideCursor(Qt::WaitCursor);
    const bool ok = pdf
        ? ListPdfExporter::exportList(records, filePath, &error)
        : ExcelExporter::exportList(records, filePath, &error);
    QApplication::restoreOverrideCursor();

    if (!ok) {
        QMessageBox::warning(this, tr("Не удалось экспортировать"),
                             error.isEmpty()
                                 ? tr("Неизвестная ошибка экспорта.")
                                 : error);
        return;
    }

    statusBar()->showMessage(
        tr("Экспортировано записей: %1 → %2")
            .arg(records.size())
            .arg(QDir::toNativeSeparators(filePath)),
        7000);
}

void MainWindow::onToggleAllChecked()
{
    if (!m_model || !m_proxy || !m_header)
        return;

    const bool check = m_header->checkState() != Qt::Checked;
    QSet<int> ids = m_model->checkedIds();

    for (int row = 0; row < m_proxy->rowCount(); ++row) {
        const QModelIndex source = m_proxy->mapToSource(
            m_proxy->index(row, PersonTableModel::Check));
        const PersonRecord *record = m_model->recordAt(source.row());
        if (!record)
            continue;

        if (check)
            ids.insert(record->id);
        else
            ids.remove(record->id);
    }

    m_model->setCheckedIds(ids);
}

void MainWindow::onTableContextMenu(const QPoint &pos)
{
    const QModelIndex proxyIndex = m_view->indexAt(pos);
    const PersonRecord *record = nullptr;

    if (proxyIndex.isValid()) {
        m_view->setCurrentIndex(proxyIndex);
        m_view->selectRow(proxyIndex.row());

        const QModelIndex sourceIndex = m_proxy->mapToSource(proxyIndex);
        record = m_model->recordAt(sourceIndex.row());
    } else {
        m_view->clearSelection();
    }

    updateActionStates();

    QMenu menu(this);
    menu.addAction(m_createAction);
    menu.addAction(m_editAction);
    menu.addAction(m_deleteAction);
    menu.addSeparator();

    const QString fullName = record ? record->fullName() : QString();
    auto *copyAction = menu.addAction(tr("Копировать ФИО"));
    copyAction->setEnabled(!fullName.isEmpty());
    connect(copyAction, &QAction::triggered, this, [fullName] {
        if (!fullName.isEmpty())
            QApplication::clipboard()->setText(fullName);
    });

    menu.exec(m_view->viewport()->mapToGlobal(pos));
}

void MainWindow::onHeaderClicked(int section)
{
    if (section == PersonTableModel::Check)
        return;   // по галочкам не сортируем

    QHeaderView *header = m_view->horizontalHeader();
    const bool sameColumn = header->sortIndicatorSection() == section;
    const Qt::SortOrder order =
        sameColumn && header->sortIndicatorOrder() == Qt::AscendingOrder
            ? Qt::DescendingOrder
            : Qt::AscendingOrder;

    m_view->sortByColumn(section, order);
}

void MainWindow::onCreateRecord()
{
    PersonDialog dialog(PersonDialog::Mode::Create, this);
    dialog.exec();

    if (!dialog.recordSaved())
        return;

    const int savedId = dialog.savedId();
    reloadData();
    selectRecordById(savedId);
    statusBar()->showMessage(tr("Запись сохранена"), 4000);
}

void MainWindow::onViewRecord()
{
    openSelectedRecord(false);
}

void MainWindow::onEditRecord()
{
    openSelectedRecord(true);
}

void MainWindow::openSelectedRecord(bool editMode)
{
    if (!m_view->selectionModel() || !m_view->selectionModel()->hasSelection())
        return;

    const QModelIndex current = m_view->currentIndex();
    if (!current.isValid())
        return;

    const QModelIndex sourceIndex = m_proxy->mapToSource(current);
    const PersonRecord *row = m_model->recordAt(sourceIndex.row());
    if (!row)
        return;

    PersonRecord record;
    QString error;
    if (!PersonRepository::fetch(row->id, &record, &error)) {
        QMessageBox::warning(this, tr("Ветераны"), error);
        return;
    }

    PersonDialog dialog(editMode ? PersonDialog::Mode::Edit
                                 : PersonDialog::Mode::View,
                        this);
    dialog.setRecord(record);
    dialog.exec();

    if (dialog.recordSaved()) {
        const int savedId = dialog.savedId();
        reloadData();
        selectRecordById(savedId);
    }
}

void MainWindow::selectRecordById(int id)
{
    if (id <= 0 || !m_proxy || !m_model || !m_view)
        return;

    for (int row = 0; row < m_proxy->rowCount(); ++row) {
        const QModelIndex proxyIndex =
            m_proxy->index(row, PersonTableModel::FullName);
        const QModelIndex sourceIndex = m_proxy->mapToSource(proxyIndex);
        const PersonRecord *record = m_model->recordAt(sourceIndex.row());
        if (!record || record->id != id)
            continue;

        m_view->selectRow(row);
        m_view->setCurrentIndex(proxyIndex);
        m_view->scrollTo(proxyIndex, QAbstractItemView::PositionAtCenter);
        return;
    }
}

void MainWindow::onDeleteRecords()
{
    const QSet<int> checkedIds = m_model->checkedIds();
    if (checkedIds.isEmpty()) {
        updateActionStates();
        return;
    }

    QStringList names;
    for (int row = 0; row < m_model->rowCount(); ++row) {
        const PersonRecord *record = m_model->recordAt(row);
        if (record && checkedIds.contains(record->id))
            names << record->fullName();
    }

    if (names.isEmpty()) {
        reloadData();
        return;
    }

    names.sort(Qt::CaseInsensitive);

    QMessageBox box(this);
    box.setIcon(QMessageBox::Question);
    box.setWindowTitle(tr("Удаление записей"));

    QString details;
    if (names.size() == 1) {
        box.setText(tr("Удалить запись «%1»?").arg(names.first()));
    } else {
        box.setText(tr("Удалить выбранные записи: %1?").arg(names.size()));

        const int limit = 5;
        const QStringList shown = names.mid(0, limit);
        details = shown.join(QLatin1Char('\n'));
        if (names.size() > limit) {
            details += QLatin1Char('\n');
            details += tr("… и ещё %1").arg(names.size() - limit);
        }
        details += QStringLiteral("\n\n");
    }
    details += tr("Удаление необратимо.");
    box.setInformativeText(details);

    auto *deleteButton = box.addButton(tr("Удалить"),
                                       QMessageBox::DestructiveRole);
    auto *cancelButton = box.addButton(tr("Отмена"),
                                       QMessageBox::RejectRole);
    box.setDefaultButton(cancelButton);
    box.setEscapeButton(cancelButton);
    box.exec();

    if (box.clickedButton() != deleteButton)
        return;

    QVector<int> ids;
    ids.reserve(checkedIds.size());
    for (int id : checkedIds)
        ids.append(id);

    QString error;
    if (!PersonRepository::removeMany(ids, &error)) {
        QMessageBox::critical(this, tr("Не удалось удалить"), error);
        return;
    }

    reloadData();
    statusBar()->showMessage(
        tr("Удалено записей: %1").arg(ids.size()), 5000);
}

void MainWindow::onPrintRecords()
{
    printChecked(false);
}

void MainWindow::onPrintCards()
{
    printChecked(true);
}

void MainWindow::printChecked(bool cards)
{
    if (!m_model)
        return;

    const QVector<PersonRecord> records = m_model->checkedRecords();
    if (records.isEmpty()) {
        QMessageBox::information(this, tr("Печать"),
                                 tr("Отметьте записи для печати."));
        return;
    }

    if (cards)
        PrintService::printCards(this, records);
    else
        PrintService::printList(this, records);
}

void MainWindow::onExtendedSearch()
{
    SearchDialog dialog(this);
    dialog.setFilter(m_proxy->advancedFilter());

    if (dialog.exec() != QDialog::Accepted)
        return;

    const PersonFilter filter = dialog.filter();
    m_proxy->setAdvancedFilter(filter);
    updateCounter();
}

void MainWindow::onImportExcel()
{
    if (!m_importService)
        return;

    // Пока доступна только пустая реализация: пункт меню неактивен,
    // но прямой вызов остаётся безопасным (M10-02).
    if (!m_importService->isAvailable()) {
        statusBar()->showMessage(
            tr("Импорт из Excel будет реализован позже."), 5000);
        return;
    }

    const QString fileName = QFileDialog::getOpenFileName(
        this, tr("Импорт из Excel"), QString(),
        tr("Книга Excel (*.xlsx *.xls)"));
    if (fileName.isEmpty())
        return;

    const ImportResult result = m_importService->importFromFile(fileName);
    if (!result.ok) {
        QMessageBox::warning(this, tr("Импорт из Excel"), result.error);
        return;
    }

    reloadData();
    statusBar()->showMessage(
        tr("Импортировано записей: %1").arg(result.imported), 5000);
}

void MainWindow::onSettings()
{
    SettingsDialog dialog(this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    if (dialog.restartRequested()) {
        statusBar()->showMessage(tr("Перезапуск приложения…"), 5000);
        restartApplication();
        return;
    }

    statusBar()->showMessage(tr("Настройки сохранены."), 5000);
}

void MainWindow::restartApplication()
{
    // Новый экземпляр запускаем после закрытия базы: main.cpp подключает
    // Database::close() к aboutToQuit раньше, поэтому порядок обработчиков
    // гарантирует, что SQLite уже отпустил файл.
    QObject::connect(qApp, &QCoreApplication::aboutToQuit, [] {
        QProcess::startDetached(QCoreApplication::applicationFilePath(),
                                QCoreApplication::arguments().mid(1));
    });
    qApp->quit();
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (m_view && watched == m_view->viewport()
        && event->type() == QEvent::Resize && m_emptyLabel) {
        m_emptyLabel->setGeometry(m_view->viewport()->rect());
    }

    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    saveWindowState();
    QMainWindow::closeEvent(event);
}
