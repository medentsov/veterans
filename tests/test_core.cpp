// Автотесты без Windows: граничные случаи, сортировка/поиск/фильтр,
// кириллица в ФИО и PDF (M12-02, M12-03, M12-06).
//
// Запуск (нужен Qt 6.5+ и платформа offscreen):
//   cmake -S . -B build -DVETERANS_BUILD_TESTS=ON
//   cmake --build build
//   VETERANS_TEST_OUT=build/test-out ./build/tests/veterans_tests
//
// PDF и Excel остаются в VETERANS_TEST_OUT для внешней проверки
// (Ghostscript, просмотр страниц).

#include "core/PhoneNumber.h"
#include "data/Database.h"
#include "data/PersonRepository.h"
#include "data/Transaction.h"
#include "report/CardRenderer.h"
#include "report/ExcelExporter.h"
#include "report/ListRenderer.h"
#include "ui/PersonProxyModel.h"
#include "ui/PersonTableModel.h"

#include "ui/MainWindow.h"
#include "ui/PersonProxyModel.h"
#include "ui/PersonTableModel.h"

#include <QApplication>
#include <QAction>
#include <QCollator>
#include <QDate>
#include <QDebug>
#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFileInfo>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QPrinter>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QString>
#include <QTemporaryDir>
#include <QTableView>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>

namespace {

int g_failures = 0;
int g_checks = 0;

void report(bool ok, const char *text, int line)
{
    ++g_checks;
    if (!ok) {
        ++g_failures;
        qCritical().noquote() << QStringLiteral("FAIL (строка %1): %2")
                                     .arg(line)
                                     .arg(QString::fromLatin1(text));
    }
}

#define CHECK(cond) report((cond), #cond, __LINE__)
#define CHECK_EQ(actual, expected)                                            \
    do {                                                                      \
        const auto a_ = (actual);                                             \
        const auto e_ = (expected);                                           \
        report(a_ == e_, #actual " == " #expected, __LINE__);                 \
        if (!(a_ == e_))                                                      \
            qCritical().noquote() << "  получено:" << a_ << "ожидалось:" << e_; \
    } while (0)

QString outDir()
{
    const QString dir = qEnvironmentVariable(
        "VETERANS_TEST_OUT", QDir::tempPath() + QStringLiteral("/veterans-tests"));
    QDir().mkpath(dir);
    return dir;
}

bool openDatabase(const QString &path)
{
    const Database::OpenResult result = Database::open(path);
    if (!result.ok)
        qCritical().noquote() << "Не удалось открыть тестовую БД:" << result.error;
    return result.ok;
}

PersonRecord makeRecord(const QString &last, const QString &first,
                        const QString &middle, const QDate &birth, int micro,
                        int category, int visitMonth, int visitYear,
                        int subscription, int subscriptionYear)
{
    PersonRecord record;
    record.lastName = last;
    record.firstName = first;
    record.middleName = middle;
    record.birthDate = birth;
    record.microdistrict = micro;
    record.street = QStringLiteral("ул. Тестовая");
    record.house = QStringLiteral("1");
    record.categoryId = category;
    record.visitMonth = visitMonth;
    record.visitYear = visitYear;
    record.subscriptionId = subscription;
    record.subscriptionYear = subscriptionYear;
    return record;
}

QString insertOrEmpty(PersonRecord record)
{
    QString error;
    if (!PersonRepository::insert(&record, &error)) {
        qCritical().noquote() << "INSERT не удался:" << error;
        return {};
    }
    return QString::number(record.id);
}

// --- M12-02: пустая БД, длинные строки, пустые поля, 10 000 записей --------

void testEmptyDatabase(const QString &dbPath)
{
    CHECK(openDatabase(dbPath));

    QString error;
    CHECK(PersonRepository::list(&error).isEmpty());
    CHECK(error.isEmpty());

    // Печать пустой базы: список без записей не печатается, а даёт понятную
    // ошибку (кнопка «Печать» и так недоступна без отмеченных строк).
    const QString emptyPdf = QDir(outDir()).filePath(QStringLiteral("empty-list.pdf"));
    QPrinter emptyPrinter(QPrinter::HighResolution);
    emptyPrinter.setOutputFormat(QPrinter::PdfFormat);
    emptyPrinter.setOutputFileName(emptyPdf);
    CHECK(!ListRenderer::render(emptyPrinter, {}, &error));
    CHECK_EQ(error, QStringLiteral("Нет записей для экспорта."));

    Database::close();
}

void testEdgeRecords(const QString &dbPath)
{
    CHECK(openDatabase(dbPath));

    PersonRecord longRecord = makeRecord(
        QString(400, QChar(0x0416)), QString(200, QChar(0x0418)),
        QString(), QDate(1940, 1, 1), 1, 0, 0, 0, 0, 0);
    longRecord.street = QString(1500, QChar(0x0423));
    longRecord.house.clear();
    longRecord.building.clear();
    longRecord.apartment.clear();
    longRecord.phoneType = 0;
    longRecord.phone.clear();

    QString error;
    CHECK(PersonRepository::insert(&longRecord, &error));
    CHECK(longRecord.id > 0);

    PersonRecord fetched;
    CHECK(PersonRepository::fetch(longRecord.id, &fetched, &error));
    CHECK_EQ(fetched.lastName.size(), 400);
    CHECK_EQ(fetched.street.size(), 1500);
    CHECK(fetched.middleName.isEmpty());
    CHECK(fetched.categoryName.isEmpty());
    CHECK(fetched.phone.isEmpty());

    // Невалидная дата рождения отклоняется репозиторием.
    PersonRecord bad = makeRecord(QStringLiteral("Без даты"),
                                  QStringLiteral("Иван"), QString(), QDate(),
                                  1, 0, 0, 0, 0, 0);
    CHECK(!PersonRepository::insert(&bad, &error));

    // deleted_at скрывает запись из списка, но не удаляет её.
    QSqlQuery query(Database::connection());
    CHECK(query.exec(QStringLiteral("UPDATE person SET deleted_at = '2026-11-23'"
                                    " WHERE id = %1").arg(longRecord.id)));
    const QVector<PersonRecord> visible = PersonRepository::list(&error);
    CHECK(visible.isEmpty());

    PersonRecord hidden;
    CHECK(PersonRepository::fetch(longRecord.id, &hidden, &error));

    // Длинные строки (400 символов ФИО, 1500 — улица) не ломают вёрстку:
    // карточка и список собираются в PDF без отказа и без пустого файла.
    const QString dir = outDir();
    const QString longCard = QDir(dir).filePath(QStringLiteral("long-card.pdf"));
    QPrinter cardPrinter(QPrinter::HighResolution);
    cardPrinter.setOutputFormat(QPrinter::PdfFormat);
    cardPrinter.setOutputFileName(longCard);
    CHECK(CardRenderer::render(cardPrinter, longRecord, longRecord.id,
                               QDate(2026, 11, 23),
                               QStringLiteral("Госпиталь ветеранов"), &error));
    CHECK(QFileInfo(longCard).size() > 0);

    const QString longList = QDir(dir).filePath(QStringLiteral("long-list.pdf"));
    QPrinter listPrinter(QPrinter::HighResolution);
    listPrinter.setOutputFormat(QPrinter::PdfFormat);
    listPrinter.setOutputFileName(longList);
    CHECK(ListRenderer::render(listPrinter, {longRecord}, &error));
    CHECK(QFileInfo(longList).size() > 0);

    // Запись видна в выборке до установки deleted_at, иначе рендер выше
    // проверял бы только скрытую строку.
    CHECK(hidden.lastName.size() == 400);

    Database::close();
}

void testBulkLoad(const QString &dbPath)
{
    CHECK(openDatabase(dbPath));

    const int total = 10000;
    QElapsedTimer timer;
    timer.start();

    QString error;
    {
        Transaction transaction(Database::connection());
        CHECK(transaction.begin(&error));
        for (int i = 0; i < total; ++i) {
            PersonRecord record = makeRecord(
                QStringLiteral("Ветеран%1").arg(i, 5, 10, QLatin1Char('0')),
                QStringLiteral("Имя%1").arg(i % 500),
                i % 3 == 0 ? QStringLiteral("Отчество") : QString(),
                QDate(1900 + i % 100, (i % 12) + 1, (i % 28) + 1),
                (i % 999) + 1, (i % 9) + 1,
                (i % 12) + 1, 2000 + i % 26,
                (i % 2) + 1, 2000 + i % 26);
            if (!PersonRepository::insert(&record, &error)) {
                qCritical().noquote() << "Массовая вставка:" << error;
                break;
            }
        }
        CHECK(transaction.commit(&error));
    }

    const qint64 ms = timer.elapsed();
    qInfo().noquote() << QStringLiteral("10 000 записей вставлено за %1 мс")
                             .arg(ms);

    const QVector<PersonRecord> all = PersonRepository::list(&error);
    CHECK_EQ(all.size(), total);
    if (!all.isEmpty())
        CHECK(all.first().lastName.startsWith(QStringLiteral("Ветеран")));

    // Массовое удаление одной транзакцией.
    QVector<int> ids;
    ids.reserve(500);
    for (int i = 0; i < qMin(500, all.size()); ++i)
        ids.append(all.at(i).id);
    CHECK(PersonRepository::removeMany(ids, &error));
    CHECK_EQ(PersonRepository::list(&error).size(), total - 500);

    Database::close();
}

// --- M12-03: сортировка по колонкам, быстрый поиск и фильтр вместе ---------

struct ModelFixture {
    PersonTableModel model;
    PersonProxyModel proxy;

    explicit ModelFixture(const QVector<PersonRecord> &records)
    {
        model.setRecords(records);
        proxy.setSourceModel(&model);
    }
};

QVector<PersonRecord> insertSearchRecords()
{
    QVector<PersonRecord> records;

    PersonRecord r1 = makeRecord(QStringLiteral("Иванов"),
                                 QStringLiteral("Иван"),
                                 QStringLiteral("Иванович"),
                                 QDate(1948, 3, 12), 7, 1, 3, 2026, 1, 2026);
    r1.phoneType = 1;
    r1.phone = QStringLiteral("79161234567");
    records << r1;

    PersonRecord r2 = makeRecord(QStringLiteral("Фёдоров"),
                                 QStringLiteral("Пётр"), QString(),
                                 QDate(1950, 1, 5), 3, 2, 0, 0, 0, 0);
    r2.phoneType = 0;
    r2.phone = QStringLiteral("12345");
    records << r2;

    records << makeRecord(QStringLiteral("Смирнов-Петров"),
                          QStringLiteral("Алексей"), QString(),
                          QDate(1939, 12, 31), 10, 3, 12, 2025, 2, 2024);

    records << makeRecord(QStringLiteral("О'Брайен"), QStringLiteral("Шон"),
                          QString(), QDate(1980, 6, 15), 1, 9, 0, 0, 0, 0);

    records << makeRecord(QStringLiteral("Ёлкин"), QStringLiteral("Егор"),
                          QString(), QDate(2000, 2, 29), 5, 4, 7, 2026, 0, 0);

    for (PersonRecord &record : records) {
        QString error;
        if (!PersonRepository::insert(&record, &error))
            qCritical().noquote() << "Поисковый набор:" << error;
    }
    return records;
}

void testSortSearchFilter(const QString &dbPath)
{
    CHECK(openDatabase(dbPath));

    insertSearchRecords();

    QString error;
    const QVector<PersonRecord> stored = PersonRepository::list(&error);
    CHECK_EQ(stored.size(), 5);

    ModelFixture fixture(stored);
    CHECK_EQ(fixture.proxy.rowCount(), 5);

    // Сортировка по каждой колонке с данными.
    QCollator collator(QLocale(QLocale::Russian, QLocale::Russia));
    collator.setCaseSensitivity(Qt::CaseInsensitive);
    collator.setNumericMode(true);

    const int columns[] = {
        PersonTableModel::FullName,
        PersonTableModel::Microdistrict,
        PersonTableModel::BirthYear,
        PersonTableModel::Category,
        PersonTableModel::VisitYear,
        PersonTableModel::Subscription,
    };

    for (int column : columns) {
        fixture.proxy.sort(column, Qt::AscendingOrder);
        bool ordered = true;
        for (int row = 1; row < fixture.proxy.rowCount(); ++row) {
            const QVariant a = fixture.proxy.index(row - 1, column).data();
            const QVariant b = fixture.proxy.index(row, column).data();
            const bool numeric = column == PersonTableModel::Microdistrict
                || column == PersonTableModel::BirthYear
                || column == PersonTableModel::VisitYear;
            const bool ok = numeric
                ? a.toInt() <= b.toInt()
                : collator.compare(a.toString(), b.toString()) <= 0;
            if (!ok) {
                ordered = false;
                qCritical().noquote()
                    << "Сортировка колонки" << column << "нарушена:"
                    << a.toString() << ">" << b.toString();
            }
        }
        CHECK(ordered);
    }

    // Быстрый поиск: «ё» приравнивается к «е».
    fixture.proxy.setQuickFilter(QStringLiteral("федоров"));
    CHECK_EQ(fixture.proxy.rowCount(), 1);

    fixture.proxy.setQuickFilter(QStringLiteral("иванов 1948"));
    CHECK_EQ(fixture.proxy.rowCount(), 1);

    fixture.proxy.setQuickFilter(QStringLiteral("ленина"));
    CHECK_EQ(fixture.proxy.rowCount(), 0);   // улица в тестовых записях другая
    fixture.proxy.setQuickFilter(QStringLiteral("тестовая"));
    CHECK_EQ(fixture.proxy.rowCount(), 5);

    fixture.proxy.setQuickFilter(QStringLiteral("9161234567"));
    CHECK_EQ(fixture.proxy.rowCount(), 1);

    fixture.proxy.setQuickFilter(QStringLiteral("ветеран труда"));
    CHECK_EQ(fixture.proxy.rowCount(), 1);

    fixture.proxy.setQuickFilter(QStringLiteral("смирнов петров"));
    CHECK_EQ(fixture.proxy.rowCount(), 1);

    fixture.proxy.setQuickFilter(QString());

    // Расширенный фильтр.
    PersonFilter filter;
    filter.lastName = QStringLiteral("федор");
    fixture.proxy.setAdvancedFilter(filter);
    CHECK_EQ(fixture.proxy.rowCount(), 1);

    filter = PersonFilter{};
    filter.microdistrict = 10;
    fixture.proxy.setAdvancedFilter(filter);
    CHECK_EQ(fixture.proxy.rowCount(), 1);

    filter = PersonFilter{};
    filter.birthYear = 2000;
    fixture.proxy.setAdvancedFilter(filter);
    CHECK_EQ(fixture.proxy.rowCount(), 1);

    filter = PersonFilter{};
    filter.categoryId = 9;
    fixture.proxy.setAdvancedFilter(filter);
    CHECK_EQ(fixture.proxy.rowCount(), 1);

    filter = PersonFilter{};
    filter.visitYear = 2025;
    fixture.proxy.setAdvancedFilter(filter);
    CHECK_EQ(fixture.proxy.rowCount(), 1);

    filter = PersonFilter{};
    filter.subscriptionId = 2;
    filter.subscriptionYear = 2024;
    fixture.proxy.setAdvancedFilter(filter);
    CHECK_EQ(fixture.proxy.rowCount(), 1);

    // Комбинация: расширенный фильтр + быстрый поиск (логическое «И»).
    filter = PersonFilter{};
    filter.microdistrict = 5;
    fixture.proxy.setAdvancedFilter(filter);
    fixture.proxy.setQuickFilter(QStringLiteral("ёлкин"));
    CHECK_EQ(fixture.proxy.rowCount(), 1);

    fixture.proxy.setQuickFilter(QStringLiteral("иванов"));
    CHECK_EQ(fixture.proxy.rowCount(), 0);

    fixture.proxy.clearAdvancedFilter();
    fixture.proxy.setQuickFilter(QString());
    CHECK_EQ(fixture.proxy.rowCount(), 5);

    Database::close();
}

// --- M12-06: кириллица, «ё», дефисы и апострофы в ФИО и в PDF ---------------

void testCyrillicAndDocuments(const QString &dbPath, const QString &dir)
{
    CHECK(openDatabase(dbPath));

    PersonRecord record = makeRecord(
        QStringLiteral("Фёдоров-О'Брайен"), QStringLiteral("Пётр"),
        QStringLiteral("Ильич"), QDate(1941, 6, 22), 4, 1, 5, 2026, 2, 2026);
    record.street = QStringLiteral("ул. Съезжинская");
    record.house = QStringLiteral("12А");
    record.phoneType = 1;
    record.phone = QStringLiteral("79161234567");

    QString error;
    CHECK(PersonRepository::insert(&record, &error));
    CHECK_EQ(record.fullName(),
             QStringLiteral("Фёдоров-О'Брайен Пётр Ильич"));

    // В карточке ФИО печатается заглавными; «ё», дефис и апостроф
    // должны дойти до PDF без потерь и без «сырых» сущностей.
    const QString html = CardRenderer::toHtml(record, record.id,
                                              QDate(2026, 11, 23),
                                              QStringLiteral("Госпиталь"));
    CHECK(html.contains(QStringLiteral("ФЁДОРОВ-О'БРАЙЕН")));
    CHECK(html.contains(QStringLiteral("ПЁТР")));

    const QString cardPdf = QDir(dir).filePath(QStringLiteral("card.pdf"));
    QPrinter cardPrinter(QPrinter::HighResolution);
    cardPrinter.setOutputFormat(QPrinter::PdfFormat);
    cardPrinter.setOutputFileName(cardPdf);
    CHECK(CardRenderer::render(cardPrinter, record, record.id,
                               QDate(2026, 11, 23),
                               QStringLiteral("Госпиталь ветеранов"), &error));

    const QVector<PersonRecord> all = PersonRepository::list(&error);
    const QString listPdf = QDir(dir).filePath(QStringLiteral("list.pdf"));
    QPrinter listPrinter(QPrinter::HighResolution);
    listPrinter.setOutputFormat(QPrinter::PdfFormat);
    listPrinter.setOutputFileName(listPdf);
    CHECK(ListRenderer::render(listPrinter, all, &error));

    const QString xlsx = QDir(dir).filePath(QStringLiteral("list.xlsx"));
    CHECK(ExcelExporter::exportList(all, xlsx, &error));

    Q_UNUSED(dbPath);
    Database::close();
}

// --- M12-01: функциональный прогон правил главного окна ---------------------

void waitForUi(int milliseconds)
{
    QEventLoop loop;
    QTimer::singleShot(milliseconds, &loop, &QEventLoop::quit);
    loop.exec();
}

QAction *actionByText(QWidget *window, const QString &text)
{
    // Сначала панель инструментов: у меню есть пункты с теми же подписями.
    if (auto *bar = window->findChild<QToolBar *>(QStringLiteral("mainToolBar"))) {
        const auto actions = bar->actions();
        for (QAction *action : actions) {
            if (action->text() == text)
                return action;
        }
    }
    const auto actions = window->findChildren<QAction *>();
    for (QAction *action : actions) {
        if (action->text() == text)
            return action;
    }
    return nullptr;
}

QLabel *labelContaining(QWidget *window, const QString &part)
{
    const auto labels = window->findChildren<QLabel *>();
    for (QLabel *label : labels) {
        if (label->text().contains(part))
            return label;
    }
    return nullptr;
}

void testMainWindowRules(const QString &dbPath)
{
    CHECK(openDatabase(dbPath));
    insertSearchRecords();

    MainWindow window;
    window.show();
    waitForUi(50);

    QAction *create = actionByText(&window, QStringLiteral("Создать"));
    QAction *edit = actionByText(&window, QStringLiteral("Редактировать"));
    QAction *remove = actionByText(&window, QStringLiteral("Удалить"));
    QAction *print = actionByText(&window, QStringLiteral("Печать"));
    QAction *search = actionByText(&window, QStringLiteral("Поиск"));
    QAction *import = actionByText(&window, QStringLiteral("Импорт из Excel"));
    CHECK(create && edit && remove && print && search && import);

    // Ctrl+P закреплён за пунктом «Записи в списке (N)» меню «Печать».
    QAction *printList = nullptr;
    const auto allActions = window.findChildren<QAction *>();
    for (QAction *candidate : allActions) {
        if (candidate->text().startsWith(QStringLiteral("Записи в списке")))
            printList = candidate;
    }
    CHECK(printList != nullptr);
    if (printList)
        CHECK_EQ(printList->shortcut(), QKeySequence(QKeySequence::Print));

    // Заглушка импорта: пункт неактивен и объясняет почему (M10-01).
    CHECK(!import->isEnabled());
    CHECK(import->toolTip().contains(QStringLiteral("позже")));

    // Горячие клавиши ТЗ (M3-07).
    CHECK_EQ(create->shortcut(), QKeySequence(QKeySequence::New));
    CHECK_EQ(search->shortcut(), QKeySequence(QKeySequence::Find));
    CHECK_EQ(edit->shortcut(), QKeySequence(QStringLiteral("Ctrl+E")));

    auto *model = window.findChild<PersonTableModel *>();
    auto *proxy = window.findChild<PersonProxyModel *>();
    CHECK(model && proxy);
    if (!model || !proxy) {
        Database::close();
        return;
    }
    CHECK_EQ(proxy->rowCount(), 5);

    // Без отмеченных записей печать и удаление недоступны (критерий 3).
    CHECK(!print->isEnabled());
    CHECK(!remove->isEnabled());
    CHECK(!edit->isEnabled());   // нет выбранной строки

    auto *view = window.findChild<QTableView *>();
    CHECK(view != nullptr);
    if (view) {
        view->selectRow(0);
        waitForUi(30);
        CHECK(edit->isEnabled());    // выбрана одна строка
        view->clearSelection();
        waitForUi(30);
        CHECK(!edit->isEnabled());
    }

    const auto checkRow = [&](int row, Qt::CheckState state) {
        const QModelIndex source = proxy->mapToSource(
            proxy->index(row, PersonTableModel::Check));
        model->setData(model->index(source.row(), PersonTableModel::Check),
                       state, Qt::CheckStateRole);
    };

    checkRow(0, Qt::Checked);
    waitForUi(30);
    CHECK_EQ(model->checkedIds().size(), 1);
    CHECK(print->isEnabled());
    CHECK(remove->isEnabled());

    checkRow(1, Qt::Checked);
    waitForUi(30);
    CHECK_EQ(model->checkedIds().size(), 2);
    CHECK(print->isEnabled());
    CHECK(remove->isEnabled());

    checkRow(0, Qt::Unchecked);
    checkRow(1, Qt::Unchecked);
    waitForUi(30);
    CHECK_EQ(model->checkedIds().size(), 0);
    CHECK(!print->isEnabled());
    CHECK(!remove->isEnabled());

    // Быстрый поиск с debounce 250 мс и счётчик (M3-03).
    QLineEdit *quickSearch = nullptr;
    const auto edits = window.findChildren<QLineEdit *>();
    for (QLineEdit *edit : edits) {
        if (edit->placeholderText().contains(QStringLiteral("Быстрый поиск")))
            quickSearch = edit;
    }
    CHECK(quickSearch != nullptr);
    if (quickSearch) {
        quickSearch->setText(QStringLiteral("федоров"));
        waitForUi(400);
        CHECK_EQ(proxy->rowCount(), 1);
        QLabel *counter = labelContaining(&window, QStringLiteral("Найдено"));
        CHECK(counter != nullptr);
        if (counter)
            CHECK(counter->text().contains(QStringLiteral("из 5")));

        quickSearch->setText(QStringLiteral("нет такого ветерана"));
        waitForUi(400);
        CHECK_EQ(proxy->rowCount(), 0);
        CHECK(labelContaining(&window, QStringLiteral("не найдено")) != nullptr);

        auto *reset = window.findChild<QToolButton *>();
        Q_UNUSED(reset);
        quickSearch->clear();
        waitForUi(400);
        CHECK_EQ(proxy->rowCount(), 5);
    }

    window.close();
    waitForUi(30);
    Database::close();
}

} // namespace

// Ресурсы (миграции, темы) — в статической библиотеке veterans_app.
static void initResources()
{
    Q_INIT_RESOURCE(app);
}

int main(int argc, char *argv[])
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    initResources();
    QApplication app(argc, argv);

    const QString dir = outDir();
    qInfo().noquote() << "Каталог результатов:" << dir;

    QTemporaryDir temp;
    CHECK(temp.isValid());

    const QString emptyDb = QDir(temp.path()).filePath(QStringLiteral("empty.db"));
    const QString edgeDb = QDir(temp.path()).filePath(QStringLiteral("edge.db"));
    const QString bulkDb = QDir(temp.path()).filePath(QStringLiteral("bulk.db"));
    const QString modelDb = QDir(temp.path()).filePath(QStringLiteral("model.db"));
    const QString docDb = QDir(temp.path()).filePath(QStringLiteral("doc.db"));

    testEmptyDatabase(emptyDb);
    testEdgeRecords(edgeDb);
    testBulkLoad(bulkDb);
    testSortSearchFilter(modelDb);
    testCyrillicAndDocuments(docDb, dir);
    testMainWindowRules(QDir(temp.path()).filePath(QStringLiteral("window.db")));

    qInfo().noquote() << QStringLiteral("Проверок: %1, провалов: %2")
                             .arg(g_checks)
                             .arg(g_failures);
    return g_failures == 0 ? 0 : 1;
}
