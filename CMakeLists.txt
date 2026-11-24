cmake_minimum_required(VERSION 3.21)

project(Veterans
    VERSION 0.1.0
    DESCRIPTION "Справочник ветеранов"
    LANGUAGES CXX
)

# --- Стандарт языка ---------------------------------------------------------
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# --- Qt ---------------------------------------------------------------------
# Минимальная версия — Qt 6.5 LTS (LGPLv3, динамическая линковка).
find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Widgets Sql PrintSupport)

qt_standard_project_setup()   # AUTOMOC / AUTOUIC
set(CMAKE_AUTORCC ON)         # .qrc: миграции, иконки, темы

# Версия приложения: единственный источник — project(Veterans VERSION ...).
configure_file(src/core/version.h.in
               ${CMAKE_BINARY_DIR}/generated/core/version.h
               @ONLY)

# --- QXlsx (MIT) — запись .xlsx ---------------------------------------------
# Вендоренная копия 1.5.1.1 (см. third_party/QXlsx/README.md).
# Собирается статически и не требует отдельной DLL при поставке.
add_subdirectory(third_party/QXlsx/QXlsx)

# --- Конфигурация по умолчанию ---------------------------------------------
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Тип сборки" FORCE)
endif()

# --- Библиотека приложения --------------------------------------------------
# Весь код, кроме main.cpp, вынесен в статическую библиотеку: её же линкуют
# автотесты (M12-01), поэтому тесты проверяют настоящие классы приложения,
# а не их копии.
add_library(veterans_app STATIC
    src/core/AppPaths.h
    src/core/AppPaths.cpp
    src/core/Logging.h
    src/core/Logging.cpp
    src/core/PhoneNumber.h
    src/core/PhoneNumber.cpp
    src/core/Theme.h
    src/core/Theme.cpp
    src/core/version.h.in
    src/data/Migrations.h
    src/data/Migrations.cpp
    src/data/Database.h
    src/data/Database.cpp
    src/data/PersonRepository.h
    src/data/PersonRepository.cpp
    src/data/BackupService.h
    src/data/BackupService.cpp
    src/data/Transaction.h
    src/data/Transaction.cpp
    src/ui/MainWindow.h
    src/ui/MainWindow.cpp
    src/ui/AboutDialog.h
    src/ui/AboutDialog.cpp
    src/ui/PersonTableModel.h
    src/ui/PersonTableModel.cpp
    src/ui/PersonProxyModel.h
    src/ui/PersonProxyModel.cpp
    src/ui/CheckHeader.h
    src/ui/CheckHeader.cpp
    src/ui/SearchDialog.h
    src/ui/SearchDialog.cpp
    src/ui/SettingsDialog.h
    src/ui/SettingsDialog.cpp
    src/ui/PersonDialog.h
    src/ui/PersonDialog.cpp
    src/report/ExportScope.h
    src/report/ExportFileGuard.h
    src/report/ExportFileGuard.cpp
    src/report/ExcelExporter.h
    src/report/ExcelExporter.cpp
    src/report/CardRenderer.h
    src/report/CardRenderer.cpp
    src/report/ListRenderer.h
    src/report/ListRenderer.cpp
    src/report/ListPdfExporter.h
    src/report/ListPdfExporter.cpp
    src/report/PrintService.h
    src/report/PrintService.cpp
    src/import/IImportService.h
    src/import/NullImportService.h
    src/import/NullImportService.cpp
    resources/app.qrc
    # Далее: import/ (v2)
)

set_target_properties(veterans_app PROPERTIES
    AUTOMOC ON
    AUTORCC ON
)

target_include_directories(veterans_app PUBLIC
    src
    ${CMAKE_BINARY_DIR}/generated
)

target_compile_definitions(veterans_app PUBLIC
    QT_DISABLE_DEPRECATED_BEFORE=0x060500
)

target_link_libraries(veterans_app PUBLIC
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Sql
    Qt6::PrintSupport
    QXlsx::QXlsx
)

# --- Приложение -------------------------------------------------------------
add_executable(Veterans WIN32 src/main.cpp)
target_link_libraries(Veterans PRIVATE veterans_app)

set_target_properties(Veterans PROPERTIES
    OUTPUT_NAME "Veterans"
    WIN32_EXECUTABLE ON
)

# --- Ресурсы Windows: иконка, версия, манифест DPI (M11-03) -----------------
if(WIN32)
    enable_language(RC)

    set(VETERANS_ICON_PATH
        "${CMAKE_CURRENT_SOURCE_DIR}/resources/icons/veterans.ico")
    set(VETERANS_MANIFEST_PATH
        "${CMAKE_CURRENT_SOURCE_DIR}/resources/veterans.manifest")

    # Версию и пути подставляет configure_file, чтобы ресурс не расходился
    # с project(Veterans VERSION ...).
    configure_file(resources/veterans.rc.in
                   ${CMAKE_BINARY_DIR}/generated/veterans.rc
                   @ONLY)

    target_sources(Veterans PRIVATE
        ${CMAKE_BINARY_DIR}/generated/veterans.rc)
endif()

# --- Автотесты Qt (необязательно) -------------------------------------------
option(VETERANS_BUILD_TESTS "Собрать автотесты Qt" OFF)
if(VETERANS_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# --- Предупреждения компилятора --------------------------------------------
if(MSVC)
    target_compile_options(veterans_app PRIVATE /W4 /permissive- /utf-8)
else()
    target_compile_options(veterans_app PRIVATE -Wall -Wextra -Wpedantic)
endif()

# --- Установка (Linux, задел на будущее) -----------------------------------
include(GNUInstallDirs)
install(TARGETS Veterans
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
