#!/usr/bin/env python3
"""Проверка шаблона карточки ветерана (M8-01) без Qt.

Сверяет HTML-шаблон `CardRenderer` с макетом `docs/PRINT-LAYOUT.md`, раздел 2:
A4 книжная, поля 20/15/15/15 мм, порядок полей, кегли, формат телефона,
пустые значения «—» и защита от разрыва страницы.

    python3 tools/check_card_template.py
"""

from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
HEADER = ROOT / "src" / "report" / "CardRenderer.h"
CPP = ROOT / "src" / "report" / "CardRenderer.cpp"
PHONE = ROOT / "src" / "core" / "PhoneNumber.cpp"
CMAKE = ROOT / "CMakeLists.txt"
LAYOUT = ROOT / "docs" / "PRINT-LAYOUT.md"

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


def cpp():
    return CPP.read_text(encoding="utf-8")


def require(source, needle, what):
    assert needle in source, f"не найдено: {what} ({needle!r})"


def check_files():
    def exists():
        assert HEADER.exists(), "нет CardRenderer.h"
        assert CPP.exists(), "нет CardRenderer.cpp"

    check("файлы CardRenderer существуют", exists)

    def cmake():
        text = CMAKE.read_text(encoding="utf-8")
        require(text, "src/report/CardRenderer.h", "заголовок в CMake")
        require(text, "src/report/CardRenderer.cpp", "исходник в CMake")
        require(text, "Qt6::PrintSupport", "Qt PrintSupport в CMake")

    check("CMake: CardRenderer и QtPrintSupport", cmake)


def check_page():
    src = cpp()

    def layout():
        require(src, "QPageSize(QPageSize::A4)", "формат A4")
        require(src, "QPageLayout::Portrait", "книжная ориентация")
        require(src, "QMarginsF(20.0, 15.0, 15.0, 15.0)",
                "поля 20/15/15/15 мм")
        require(src, "printer.setFullPage(true)", "полная страница без полей драйвера")
        require(src, "contentMm.left() * 96.0 / 25.4",
                "позиция контента считается от полей макета")

    check("A4 книжная, поля 20/15/15/15 мм", layout)

    def document():
        require(src, "#include <QTextDocument>", "подключение QTextDocument")
        require(src, "QTextDocument document;", "создание документа")
        require(src, "document.setHtml(toHtml(", "HTML в документ")
        require(src, "document.drawContents(", "ручная отрисовка страниц")
        require(src, "painter.begin(&printer)", "рисование в принтер")
        # document.print() добавляет собственную нумерацию страниц,
        # а макет требует «нумерация не нужна».
        assert "document.print(&printer)" not in src, \
            "нельзя document.print(): он нумерует страницы"

    check("рендер через QTextDocument и QPainter", document)

    def no_split():
        require(src, "page-break-inside: avoid", "запрет разрыва карточки")

    check("карточка не разрывается между страницами", no_split)


def check_header():
    src = cpp()

    def hospital():
        require(src, "ГОСПИТАЛЬ ВЕТЕРАНОВ", "шапка учреждения")
        require(src, ".hospital { font-size:14pt; font-weight:bold; "
                     "text-align:center; }", "14 pt, bold, по центру")
        require(src, ".org { font-size:9pt; text-align:center;",
                "наименование 9 pt по центру")
        require(src, "Госпиталь ветеранов", "значение по умолчанию")
        require(src, "organizationName.trimmed()", "подстановка из настроек")

    check("шапка: учреждение 14 pt и наименование 9 pt", hospital)

    def lines():
        # Макет задаёт линию 0.75 pt и рамку 0.5 pt, но QTextDocument рисует
        # рамки таблиц только в px (значения в pt молча отбрасываются).
        # 1 px при 96 dpi = 0.2646 мм = 0.75 pt, поэтому в коде везде px.
        require(src, "border-bottom:1px solid #000000;", "линия 0.75 pt в px")
        require(src, "border:1px solid #000000;", "рамка ФИО в px")
        require(src, "1 px при 96 dpi = 0.2646 мм = 0.75 pt",
                "пояснение к переводу pt в px")
        layout = LAYOUT.read_text(encoding="utf-8")
        require(layout, "линия 0.75 pt = 1 px", "макет: линия в px")
        require(layout, "рамка 0.5 pt = 1 px", "макет: рамка в px")

    check("линии и рамка по макету", lines)


def check_title():
    src = cpp()

    def title():
        require(src, "КАРТОЧКА ВЕТЕРАНА", "заголовок карточки")
        require(src, ".heading { font-size:12pt; font-weight:bold; }",
                "заголовок 12 pt bold")
        require(src, ".number { font-size:10pt; text-align:right; }",
                "номер 10 pt справа")
        require(src, "rightJustified(6, QLatin1Char('0'))",
                "номер с ведущими нулями до 6 знаков")

    check("заголовок 12 pt и номер 10 pt справа", title)

    def fio():
        require(src, ".fio { font-size:15pt; font-weight:bold;",
                "ФИО 15 pt bold")
        require(src, ".fio-label { font-size:7.5pt; }", "подпись ФИО 7.5 pt")
        require(src, "record.fullName().toUpper()", "ФИО заглавными")

    check("ФИО: 15 pt bold, заглавные, рамка", fio)


def check_field_order():
    src = cpp()
    order = [
        "Дата рождения:",
        "АДРЕС ПРОПИСКИ:",
        "Телефон:",
        "ЛЬГОТНАЯ КАТЕГОРИЯ:",
        "ДАТА ПОСЕЩЕНИЯ ГОСПИТАЛЯ:",
        "ПОДПИСКА НА ГАЗЕТУ:",
        "Дата формирования:",
    ]

    def order_ok():
        positions = [src.index(marker) for marker in order]
        assert positions == sorted(positions), \
            f"нарушен порядок полей: {list(zip(order, positions))}"

    check("порядок полей по ТЗ", order_ok)

    def two_columns():
        require(src, "Дата рождения:", "дата рождения")
        require(src, "Микрорайон:", "микрорайон в той же строке")
        require(src, ".row { font-size:10pt;", "строка 10 pt")

    check("дата рождения и микрорайон в одной строке", two_columns)


def check_values():
    src = cpp()

    def empty():
        require(src, 'QStringLiteral("—")', "подстановка «—»")
        require(src, "record.birthDate.isValid()", "проверка даты рождения")
        require(src, "record.categoryName.isEmpty()", "пустая категория")
        require(src, "record.visitYear <= 0", "пустой год посещения")
        require(src, "record.subscriptionId == 0", "пустая подписка")

    check("пустые значения заменяются на «—»", empty)

    def address():
        for part in ("record.street", "record.house", "record.building",
                     "record.apartment", '"д. %1"', '"корп. %1"', '"кв. %1"'):
            require(src, part, f"адрес: {part}")

    check("адрес: улица, дом, корпус, квартира", address)

    def phone():
        require(src, "PhoneNumber::format(type, record.phone)",
                "формат телефона из справочника")
        require(src, "PhoneNumber::typeName(type)", "подпись типа телефона")
        require(src, '"%1 (%2)"', "телефон с типом в скобках")
        require(src, "if (!phone.isEmpty())", "пустой телефон не печатается")
        phone_src = PHONE.read_text(encoding="utf-8")
        require(phone_src, 'QStringLiteral("%1-%2-%3")', "домашний X-XX-XX")
        require(phone_src, "QLatin1Char('+') + digits", "мобильный +7…")

    check("телефон: X-XX-XX / +7XXXXXXXXXX с типом", phone)

    def visit():
        for month in ("январь", "февраль", "март", "апрель", "май",
                      "июнь", "июль", "август", "сентябрь", "октябрь",
                      "ноябрь", "декабрь"):
            require(src, month, f"месяц «{month}»")
        require(src, "monthNameNominative(record.visitMonth)",
                "месяц в именительном падеже")
        require(src, '"%1 %2"', "формат «март 2026»")
        # Падеж зафиксирован одинаково в макете и в коде (M12-07).
        layout = LAYOUT.read_text(encoding="utf-8")
        require(layout, "словом в именительном падеже", "макет: именительный падеж")

    check("дата посещения: месяц словом и год", visit)

    def subscription():
        require(src, '"%1, %2"', "формат «Ветеран, 2026»")
        require(src, "record.subscriptionName", "название газеты")

    check("подписка: газета и год", subscription)

    def footer():
        require(src, "Дата формирования:", "дата формирования")
        require(src, "Подпись: ___________", "строка подписи")
        require(src, ".footer { font-size:8pt;", "подпись 8 pt")

    check("подвал: дата формирования и подпись 8 pt", footer)


def check_escaping():
    src = cpp()

    def escaped():
        for value in ("organization.toHtmlEscaped()",
                      "fullName.toHtmlEscaped()",
                      "birthDateText(record).toHtmlEscaped()",
                      "addressText(record).toHtmlEscaped()",
                      "phone.toHtmlEscaped()",
                      "visitDateText(record).toHtmlEscaped()",
                      "subscriptionText(record).toHtmlEscaped()"):
            require(src, value, f"экранирование {value}")

    check("данные экранируются через toHtmlEscaped", escaped)


def main():
    print("Проверка шаблона карточки (M8-01) без Qt")
    check_files()
    check_page()
    check_header()
    check_title()
    check_field_order()
    check_values()
    check_escaping()

    print(f"\nПроверок: {checks}, провалов: {len(failures)}")
    if failures:
        for item in failures:
            print(" -", item)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
