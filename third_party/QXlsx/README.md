# QXlsx (вендоренная копия)

- **Версия:** 1.5.1.1
- **Источник:** https://github.com/QtExcel/QXlsx/tree/v1.5.1.1
- **Лицензия:** MIT, полный текст — `LICENSE` в этом каталоге
- **Что оставлено:** только библиотека `QXlsx/` (`CMakeLists.txt`, `cmake/`,
  `header/`, `source/`); примеры, тесты и скрипты upstream не копировались.

Подключение — в корневом `CMakeLists.txt`:

```cmake
add_subdirectory(third_party/QXlsx/QXlsx)
target_link_libraries(Veterans PRIVATE QXlsx::QXlsx)
```

По умолчанию QXlsx собирается статически (`BUILD_SHARED_LIBS=OFF`), поэтому
отдельная DLL в поставку не попадает. Для сборки нужен компонент Qt
`Qt6::GuiPrivate` — он входит в официальный Qt SDK (MinGW/MSVC).

## Локальные особенности

Библиотека **не патчится**. В QXlsx 1.5.1.1 нет публичного API для
автофильтра и закрепления первой строки, поэтому `src/report/ExcelExporter.cpp`
после `saveAs()` аккуратно добавляет `<autoFilter>` и `<pane>` в XML листа
(через штатные `ZipReader`/`ZipWriter` самой QXlsx).

## Обновление

1. Скачать новый тег с GitHub.
2. Заменить каталог `QXlsx/` и `LICENSE`.
3. Обновить версию в этом файле.
4. Пересобрать и проверить `tools/check_excel_export.py` (структура .xlsx).
