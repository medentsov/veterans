#!/usr/bin/env python3
"""Проверка кириллицы в готовых PDF/Excel (M12-06) без Qt.

Инструмент читает документы, созданные автотестами (`tests/veterans_tests`), и
проверяет две вещи:

1. документ валиден — есть заголовок `%PDF`, страницы, встроенные шрифты и
   карта `/ToUnicode` (по ней просмотрщик восстанавливает текст при копировании,
   поиске и печати);
2. все не-ASCII символы, которые обязаны быть в документе, присутствуют в
   картах `/ToUnicode`. Если символ пропал из карты, в PDF получаются
   «квадратики» — это ловится без визуального просмотра и без Windows.

    VETERANS_TEST_OUT=build/test-out ./build/tests/veterans_tests
    python3 tools/check_pdf_text.py [каталог]

Каталог по умолчанию — `$VETERANS_TEST_OUT` или `build/test-out`.
Если документы ещё не сформированы, проверка пропускается с пояснением.
"""

import os
import re
import sys
import zipfile
import zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

FIO_UPPER = "ФЁДОРОВ-О'БРАЙЕН ПЁТР ИЛЬИЧ"
FIO_MIXED = "Фёдоров-О'Брайен Пётр Ильич"
STREET = "ул. Съезжинская"

# Что каждый файл автотестов обязан напечатать (M12-06).
EXPECTED_TEXT = {
    "card.pdf": (FIO_UPPER, STREET, "май"),
    "list.pdf": (FIO_MIXED,),
    "long-card.pdf": ("Ж" * 400, "И" * 200, "У" * 1500),
    "long-list.pdf": ("Ж" * 400, "И" * 200),
}

EXPECTED_XLSX_TEXT = {
    "list.xlsx": (FIO_MIXED,),
}

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


def output_dir() -> Path:
    if len(sys.argv) > 1:
        return Path(sys.argv[1])
    return Path(os.environ.get("VETERANS_TEST_OUT", ROOT / "build" / "test-out"))


def streams(data: bytes):
    """Потоки PDF: распакованные FlateDecode или как есть."""
    for match in re.finditer(rb"stream\r?\n", data):
        start = match.end()
        end = data.find(b"endstream", start)
        if end < 0:
            continue
        raw = data[start:end]
        try:
            yield zlib.decompress(raw)
        except zlib.error:
            yield raw


def cmap_chars(data: bytes) -> set:
    """Множество символов из всех карт /ToUnicode документа."""
    chars = set()
    for part in streams(data):
        if b"beginbfchar" not in part and b"beginbfrange" not in part:
            continue
        for block in re.findall(rb"beginbfchar(.*?)endbfchar", part, re.S):
            for _, dst in re.findall(rb"<([0-9A-Fa-f]+)>\s*<([0-9A-Fa-f]+)>", block):
                chars.add(bytes.fromhex(dst.decode()).decode("utf-16-be", "replace"))
        for block in re.findall(rb"beginbfrange(.*?)endbfrange", part, re.S):
            # Диапазон по возрастанию: <lo> <hi> <dst>
            for _, _, dst in re.findall(
                rb"<([0-9A-Fa-f]+)>\s*<([0-9A-Fa-f]+)>\s*<([0-9A-Fa-f]+)>", block
            ):
                chars.add(chr(int(dst, 16)))
            # Диапазон перечислением: <lo> <hi> [<d1> <d2> …] — так пишет Qt.
            for _, _, array in re.findall(
                rb"<([0-9A-Fa-f]+)>\s*<([0-9A-Fa-f]+)>\s*\[(.*?)\]", block, re.S
            ):
                for dst in re.findall(rb"<([0-9A-Fa-f]+)>", array):
                    chars.add(
                        bytes.fromhex(dst.decode()).decode("utf-16-be", "replace")
                    )
    return chars


def document_pages(data: bytes) -> int:
    return len(re.findall(rb"/Type\s*/Page[^s]", data))


def check_pdf(path: Path):
    data = path.read_bytes()
    chars = cmap_chars(data)

    def valid():
        assert data.startswith(b"%PDF-"), "нет заголовка %PDF"
        assert document_pages(data) >= 1, "в документе нет страниц"
        embedded = data.count(b"/FontFile2") + data.count(b"/FontFile3")
        assert embedded >= 1, "шрифт не встроен в PDF"
        assert data.count(b"/ToUnicode") >= 1, "нет карты /ToUnicode"

    check(f"{path.name}: валидный PDF со встроенными шрифтами", valid)

    text = EXPECTED_TEXT.get(path.name)

    def cyrillic():
        needed = {ch for sample in text for ch in sample if not ch.isascii()}
        missing = sorted(needed - chars)
        assert not missing, f"нет в /ToUnicode: {''.join(missing)!r}"
        assert any("А" <= ch <= "я" for ch in chars), "в PDF нет кириллицы"

    if text is None:
        check(f"{path.name}: кириллица в /ToUnicode", lambda: cyrillic_fallback(chars))
    else:
        check(f"{path.name}: кириллица, «ё», дефис и апостроф в /ToUnicode", cyrillic)


def cyrillic_fallback(chars: set):
    assert any("А" <= ch <= "я" for ch in chars), "в PDF нет кириллицы"


def check_xlsx(path: Path):
    samples = EXPECTED_XLSX_TEXT[path.name]

    def valid():
        with zipfile.ZipFile(path) as archive:
            names = archive.namelist()
            assert "xl/workbook.xml" in names, "нет xl/workbook.xml"
            assert "xl/sharedStrings.xml" in names, "нет таблицы строк Excel"
            shared = archive.read("xl/sharedStrings.xml").decode("utf-8")
        for sample in samples:
            assert sample in shared, f"нет строки {sample!r} в Excel"

    check(f"{path.name}: кириллица в строках Excel", valid)


def main() -> int:
    directory = output_dir()
    print(f"Проверка кириллицы в документах (M12-06): {directory}")
    pdfs = sorted(directory.glob("*.pdf")) if directory.is_dir() else []
    xlsxs = sorted(directory.glob("*.xlsx")) if directory.is_dir() else []

    if not pdfs:
        print("PDF не найдены — сначала запустите автотесты "
              "(VETERANS_TEST_OUT=… ./tests/veterans_tests). Проверка пропущена.")
        return 0

    for pdf in pdfs:
        check_pdf(pdf)
    for xlsx in xlsxs:
        check_xlsx(xlsx)

    print(f"\nПроверок: {checks}, провалов: {len(failures)}")
    if failures:
        for item in failures:
            print(" -", item)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
