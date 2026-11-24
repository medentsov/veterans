#!/usr/bin/env python3
"""Проверка печати списка и карточек (M8-02…M8-04) без Qt.

Проверяет общий `PrintService`: предпросмотр QPrintPreviewDialog (кнопка
«Печать» открывает QPrintDialog), альбомный список, карточки с новой
страницы, меню «Печать» и доступность пунктов по отмеченным записям.

    python3 tools/check_print_ui.py
"""

from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SERVICE_H = ROOT / "src" / "report" / "PrintService.h"
SERVICE_CPP = ROOT / "src" / "report" / "PrintService.cpp"
MAIN_CPP = ROOT / "src" / "ui" / "MainWindow.cpp"
MAIN_H = ROOT / "src" / "ui" / "MainWindow.h"
CMAKE = ROOT / "CMakeLists.txt"

checks = 0
failures = []


def check(name, fn):
    global checks
    checks += 1
    try:
        fn()
        print(f"  ok   {name}")
    except Exception as exc:  # noqa: BLE001
        failures.append(f"{name}: {exc}")
        print(f"  FAIL {name}: {exc}")


def read(path):
    return path.read_text(encoding="utf-8")


def require(source, needle, what):
    assert needle in source, f"не найдено: {what} ({needle!r})"


def between(source, start_marker, end_marker):
    start = source.index(start_marker)
    end = source.index(end_marker, start)
    return source[start:end]


def service_list():
    return between(read(SERVICE_CPP), "void PrintService::printList",
                   "void PrintService::printCards")


def service_cards():
    text = read(SERVICE_CPP)
    return text[text.index("void PrintService::printCards"):]


def check_files():
    def exists():
        for path in (SERVICE_H, SERVICE_CPP):
            assert path.exists(), f"нет {path.relative_to(ROOT)}"

    check("файлы PrintService существуют", exists)

    def cmake():
        text = read(CMAKE)
        require(text, "src/report/PrintService.h", "заголовок в CMake")
        require(text, "src/report/PrintService.cpp", "исходник в CMake")
        require(text, "Qt6::PrintSupport", "Qt PrintSupport в CMake")

    check("CMake: PrintService и QtPrintSupport", cmake)


def check_preview():
    def list_preview():
        body = service_list()
        require(body, "QPrintPreviewDialog preview(&printer, parent);",
                "предпросмотр списка")
        require(body, "QPrintPreviewDialog::paintRequested",
                "отрисовка страниц списка")
        require(body, "ListRenderer::render(*p, records, &error)",
                "общий рендерер списка")
        require(body, "preview.exec();", "запуск предпросмотра")

    check("список: QPrintPreviewDialog + ListRenderer", list_preview)

    def cards_preview():
        body = service_cards()
        require(body, "QPrintPreviewDialog preview(&printer, parent);",
                "предпросмотр карточек")
        require(body, "QPrintPreviewDialog::paintRequested",
                "отрисовка страниц карточек")
        require(body, "CardRenderer::render(*p, record, record.id, formedDate,",
                "рендерер карточки и номер = id")
        require(body, "p->newPage()", "каждая карточка с новой страницы")
        require(body, "preview.exec();", "запуск предпросмотра")

    check("карточки: QPrintPreviewDialog + CardRenderer + newPage",
          cards_preview)

    def print_dialog():
        text = read(SERVICE_CPP)
        assert text.count("QPrintDialog") >= 2, \
            "в обоих режимах должен быть описан QPrintDialog"
        assert text.count("Кнопка «Печать» в QPrintPreviewDialog") >= 2, \
            "нет пояснения про встроенную кнопку печати"

    check("QPrintDialog: принтер, копии, ориентация, поля", print_dialog)


def check_layout():
    def list_layout():
        body = service_list()
        require(body, "QPageLayout::Landscape", "альбомная ориентация")
        require(body, "QMarginsF(15.0, 15.0, 15.0, 15.0)", "поля 15 мм")

    check("список: A4 альбомный, поля 15 мм", list_layout)

    def cards_layout():
        body = service_cards()
        require(body, "QPageLayout::Portrait", "книжная ориентация")
        require(body, "QMarginsF(20.0, 15.0, 15.0, 15.0)",
                "поля 20/15/15/15 мм")

    check("карточки: A4 книжная, поля 20/15/15/15 мм", cards_layout)


def check_menu():
    main = read(MAIN_CPP)

    def menu():
        require(main, 'menuBar()->addMenu(tr("Печать"))', "меню «Печать»")
        require(main, 'm_printListAction = printMenu->addAction(tr("Записи в списке"))',
                "пункт «Записи в списке»")
        require(main, 'm_printCardsAction = printMenu->addAction(tr("Каждую отдельно"))',
                "пункт «Каждую отдельно»")

    check("меню «Печать»: список и каждую отдельно", menu)

    def triggers():
        require(main, "connect(m_printListAction, &QAction::triggered,\n"
                      "            this, &MainWindow::onPrintRecords);",
                "пункт списка")
        require(main, "connect(m_printCardsAction, &QAction::triggered,\n"
                      "            this, &MainWindow::onPrintCards);",
                "пункт карточек")
        require(main, "connect(m_printAction, &QAction::triggered,\n"
                      "            this, &MainWindow::onPrintRecords);",
                "кнопка панели инструментов")

    check("обработчики меню и панели инструментов", triggers)

    def shortcut():
        require(main, "m_printListAction->setShortcut(QKeySequence::Print);",
                "Ctrl+P на пункте списка")
        toolbar = between(main, "m_printAction = m_toolBar->addAction",
                          "m_searchAction = m_toolBar->addAction")
        assert "setShortcut" not in toolbar, \
            "у кнопки панели не должно быть второго Ctrl+P"

    check("Ctrl+P назначен одному действию", shortcut)


def check_state():
    main = read(MAIN_CPP)

    def enabled():
        body = between(main, "void MainWindow::updateActionStates",
                       "void MainWindow::updateExportActions")
        require(body, "m_printAction->setEnabled(checked > 0);",
                "кнопка панели")
        require(body, "m_printListAction->setEnabled(checked > 0);",
                "пункт списка")
        require(body, "m_printCardsAction->setEnabled(checked > 0);",
                "пункт карточек")

    check("печать доступна только при отмеченных записях", enabled)

    def records():
        body = between(main, "void MainWindow::printChecked",
                       "void MainWindow::onExtendedSearch")
        require(body, "m_model->checkedRecords()", "выборка отмеченных")
        require(body, "records.isEmpty()", "защита от пустой выборки")
        require(body, "PrintService::printCards(this, records);", "карточки")
        require(body, "PrintService::printList(this, records);", "список")

    check("printChecked: отмеченные → PrintService", records)

    def wiring():
        require(read(MAIN_CPP), '#include "report/PrintService.h"',
                "подключение PrintService")
        header = read(MAIN_H)
        require(header, "void onPrintCards();", "слот карточек")
        require(header, "void printChecked(bool cards);", "общий метод")
        require(header, "QAction *m_printListAction", "пункт списка")
        require(header, "QAction *m_printCardsAction", "пункт карточек")

    check("MainWindow: подключение, слоты и пункты меню", wiring)


def check_form_card():
    dialog_cpp = ROOT / "src" / "ui" / "PersonDialog.cpp"
    text = read(dialog_cpp)

    def wiring():
        require(text, '#include "report/PrintService.h"',
                "подключение PrintService")
        body = between(text, "void PersonDialog::onPrintCard()",
                       "void PersonDialog::onFieldEdited()")
        require(body, "if (!recordSaved())", "печать только сохранённой записи")
        require(body, "PersonRepository::fetch(m_recordId, &record, &error)",
                "перечитывание записи с названиями справочников")
        require(body, "PrintService::printCards(this, QVector<PersonRecord>{record});",
                "печать одной карточки")

    check("форма: печать одной карточки через PrintService", wiring)

    def button():
        body = between(text, "m_printButton = new QPushButton",
                       "m_editButton = new QPushButton")
        require(body, "PersonDialog::onPrintCard", "обработчик кнопки «Печать»")

    check("форма: кнопка «Печать» подключена", button)


def main():
    print("Проверка печати списка и карточек (M8-02…M8-05) без Qt")
    check_files()
    check_preview()
    check_layout()
    check_menu()
    check_state()
    check_form_card()

    print(f"\nПроверок: {checks}, провалов: {len(failures)}")
    if failures:
        for item in failures:
            print(" -", item)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
