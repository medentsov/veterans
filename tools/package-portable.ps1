<#
    Portable-сборка «Ветеранов» (M11-01).

    Копирует собранный Veterans.exe в dist\Veterans, добавляет DLL и плагины
    Qt через windeployqt, создаёт пустые каталоги данных и упаковывает всё
    в dist\Veterans-portable.zip.

    Запуск (из корня репозитория, в PowerShell):
        powershell -ExecutionPolicy Bypass -File tools\package-portable.ps1

    Параметры:
        -BuildDir  каталог сборки (по умолчанию build\release)
        -OutDir    каталог результата (по умолчанию dist)
        -QtBin     каталог bin Qt, если windeployqt не в PATH
#>
[CmdletBinding()]
param(
    [string]$BuildDir = "build/release",
    [string]$OutDir = "dist",
    [string]$QtBin = ""
)

$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
$exe = Join-Path $root (Join-Path $BuildDir "Veterans.exe")
if (-not (Test-Path $exe)) {
    throw "Не найден $exe. Сначала соберите проект: cmake --build --preset release"
}

$staging = Join-Path $root (Join-Path $OutDir "Veterans")
if (Test-Path $staging) {
    Remove-Item -Recurse -Force $staging
}
New-Item -ItemType Directory -Path $staging | Out-Null

Copy-Item $exe $staging
$stagedExe = Join-Path $staging "Veterans.exe"

$windeployqt = "windeployqt.exe"
if ($QtBin) {
    $windeployqt = Join-Path $QtBin "windeployqt.exe"
    if (-not (Test-Path $windeployqt)) {
        throw "Не найден windeployqt: $windeployqt"
    }
}

Write-Host "Копирование библиотек Qt..."
& $windeployqt --release --compiler-runtime --no-translations $stagedExe
if ($LASTEXITCODE -ne 0) {
    throw "windeployqt завершился с кодом $LASTEXITCODE"
}

# Приложение portable: каталоги данных создаются рядом с exe при первом
# запуске. Пустые папки в архиве показывают структуру и не мешают работе.
foreach ($dir in @("data", "logs", "backups")) {
    New-Item -ItemType Directory -Force `
        -Path (Join-Path $staging $dir) | Out-Null
}

$zip = Join-Path $root (Join-Path $OutDir "Veterans-portable.zip")
if (Test-Path $zip) {
    Remove-Item -Force $zip
}

Write-Host "Упаковка архива..."
Compress-Archive -Path (Join-Path $staging "*") -DestinationPath $zip

Write-Host "Готово: $zip"
