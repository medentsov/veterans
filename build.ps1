<#
    Сборка «Ветеранов» (M1-07).

    Настраивает CMake, собирает Release и кладёт библиотеки Qt рядом с exe
    через windeployqt, чтобы приложение запускалось из build\release на машине
    без установленного Qt. Упаковка в zip и установщик — отдельные шаги
    (tools\package-portable.ps1, installer\veterans.iss).

    Запуск (из корня репозитория, в PowerShell):
        powershell -ExecutionPolicy Bypass -File build.ps1

    Параметры:
        -Preset    пресет CMake из CMakePresets.json (по умолчанию release)
        -QtBin     каталог bin Qt, если windeployqt не в PATH
        -NoDeploy  собрать без windeployqt (например, для CI без GUI)
        -Tests     собрать и прогнать автотесты (VETERANS_BUILD_TESTS=ON)
#>
[CmdletBinding()]
param(
    [ValidateSet("release", "debug")]
    [string]$Preset = "release",
    [string]$QtBin = "",
    [switch]$NoDeploy,
    [switch]$Tests
)

$ErrorActionPreference = "Stop"
$root = $PSScriptRoot
Push-Location $root

try {
    $cmake = Get-Command cmake -ErrorAction SilentlyContinue
    if (-not $cmake) {
        throw "cmake не найден в PATH. Установите CMake 3.21+ и повторите запуск."
    }

    $configureArgs = @("--preset", $Preset)
    if ($Tests) {
        $configureArgs += "-DVETERANS_BUILD_TESTS=ON"
    }

    Write-Host "Настройка CMake (пресет $Preset)..."
    & cmake @configureArgs
    if ($LASTEXITCODE -ne 0) {
        throw "cmake --preset $Preset завершился с кодом $LASTEXITCODE"
    }

    Write-Host "Сборка..."
    & cmake --build --preset $Preset
    if ($LASTEXITCODE -ne 0) {
        throw "cmake --build --preset $Preset завершился с кодом $LASTEXITCODE"
    }

    # Одноконфигурационные генераторы (MinGW/Ninja) кладут exe прямо в
    # build\<preset>, многоконфигурационные (Visual Studio) — в подкаталог.
    $buildDir = Join-Path $root (Join-Path "build" $Preset)
    $exe = Join-Path $buildDir "Veterans.exe"
    if (-not (Test-Path $exe)) {
        $exe = Join-Path (Join-Path $buildDir "Release") "Veterans.exe"
    }
    if (-not (Test-Path $exe)) {
        throw "Не найден Veterans.exe в $buildDir. Проверьте вывод сборки."
    }

    if (-not $NoDeploy) {
        $windeployqt = "windeployqt.exe"
        if ($QtBin) {
            $windeployqt = Join-Path $QtBin "windeployqt.exe"
            if (-not (Test-Path $windeployqt)) {
                throw "Не найден windeployqt: $windeployqt"
            }
        }

        # Флаги --release/--debug не передаём: для MinGW windeployqt определяет
        # debug по флагу IMAGE_FILE_DEBUG_STRIPPED, который GNU ld не ставит даже
        # в release-сборке. С --release фильтр плагинов оставляет только
        # «release», не находит ни одного и падает с «Unable to find the
        # platform plugin». Автоопределение для MinGW использует режим
        # MatchDebugOrRelease и находит нужные плагины.
        Write-Host "Копирование библиотек Qt..."
        & $windeployqt --compiler-runtime --no-translations $exe
        if ($LASTEXITCODE -ne 0) {
            throw "windeployqt завершился с кодом $LASTEXITCODE"
        }
    }

    if ($Tests) {
        $exeDir = Split-Path -Parent $exe

        # Автотесты идут с QT_QPA_PLATFORM=offscreen, а windeployqt кладёт
        # только qwindows.dll. Копируем плагин offscreen рядом с exe, чтобы
        # тестам не требовался установленный Qt.
        if (-not $NoDeploy) {
            $windeployqtExe = (Get-Command $windeployqt -ErrorAction SilentlyContinue).Source
            if ($windeployqtExe) {
                $qtPluginsDir = Join-Path (Split-Path (Split-Path $windeployqtExe -Parent) -Parent) "plugins"
                $offscreenPlugin = Join-Path $qtPluginsDir "platforms\qoffscreen.dll"
                if (Test-Path $offscreenPlugin) {
                    $platformsDir = Join-Path $exeDir "platforms"
                    New-Item -ItemType Directory -Force -Path $platformsDir | Out-Null
                    Copy-Item $offscreenPlugin (Join-Path $platformsDir "qoffscreen.dll") -Force
                } else {
                    Write-Warning "Не найден плагин offscreen: $offscreenPlugin"
                }
            }
        }

        $testCandidates = @(
            (Join-Path $exeDir "veterans_tests.exe"),
            (Join-Path (Join-Path $exeDir "tests") "veterans_tests.exe"),
            (Join-Path (Join-Path (Join-Path $buildDir "tests") "Release") "veterans_tests.exe")
        )
        # Имя переменной не должно совпадать с параметром [switch]$Tests:
        # PowerShell не различает регистр, и присваивание строки в $tests
        # падало с «не удалось преобразовать String в SwitchParameter».
        $testsExe = $testCandidates | Where-Object { Test-Path $_ } | Select-Object -First 1
        if ($testsExe) {
            Write-Host "Автотесты..."
            $env:QT_QPA_PLATFORM = "offscreen"
            # На Windows Qt пишет qInfo/qWarning в stderr, только если видит
            # консоль; иначе сообщения уходят в OutputDebugString и теряются
            # при перенаправлении вывода. Форсируем stderr, чтобы итог
            # «Проверок: N, провалов: M» попадал в лог сборки.
            $env:QT_ASSUME_STDERR_HAS_CONSOLE = "1"
            & $testsExe
            if ($LASTEXITCODE -ne 0) {
                throw "автотесты завершились с кодом $LASTEXITCODE"
            }
        } else {
            Write-Warning "Автотесты не собраны: проверьте $($testCandidates -join ', ')"
        }
    }

    Write-Host "Готово: $exe"
} finally {
    Pop-Location
}
