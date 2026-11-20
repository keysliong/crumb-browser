# Скачивает зависимости Crumb: CEF (в third_party/cef) и Qt 6 для MSVC (в C:\Qt).
# Запуск из корня проекта:  powershell -ExecutionPolicy Bypass -File scripts\setup-deps.ps1

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot

# ------------------------------------------------------------------------ CEF
$cefVersion = "cef_binary_156.0.31+ge603d97+chromium-156.0.8078.100_windows64_minimal"
$cefSha1    = "d179fb251141d6c30992a1775db80a6a879852ba"
$cefDir     = Join-Path $root "third_party\cef"

if (Test-Path (Join-Path $cefDir "cmake\FindCEF.cmake")) {
    Write-Host "CEF уже на месте: $cefDir"
} else {
    $dl = Join-Path $root "third_party\_download"
    New-Item -ItemType Directory -Force $dl | Out-Null
    $archive = Join-Path $dl "cef.tar.bz2"
    $url = "https://cef-builds.spotifycdn.com/" + [Uri]::EscapeDataString("$cefVersion.tar.bz2")
    Write-Host "Скачиваю CEF (~170 МБ)…"
    Invoke-WebRequest -Uri $url -OutFile $archive -UseBasicParsing
    $hash = (Get-FileHash $archive -Algorithm SHA1).Hash.ToLower()
    if ($hash -ne $cefSha1) { throw "Контрольная сумма CEF не совпала: $hash" }
    Write-Host "Распаковываю…"
    tar -xjf $archive -C (Join-Path $root "third_party")
    Move-Item (Join-Path $root "third_party\$cefVersion") $cefDir
    Remove-Item -Recurse -Force $dl
    Write-Host "CEF готов: $cefDir"
}

# ------------------------------------------------------------------------- Qt
$qtVersion = "6.10.3"
$qtDir = "C:\Qt\$qtVersion\msvc2022_64"
if (Test-Path "$qtDir\bin\windeployqt.exe") {
    Write-Host "Qt уже на месте: $qtDir"
} else {
    Write-Host "Ставлю Qt $qtVersion через aqtinstall (нужен Python; запускается под Python 3.12 через uv)…"
    python -m pip install --quiet uv
    python -m uv tool run --python 3.12 --from aqtinstall aqt install-qt windows desktop $qtVersion `
        win64_msvc2022_64 -m qtmultimedia qtshadertools qtimageformats --outputdir C:\Qt
    if (-not (Test-Path "$qtDir\bin\windeployqt.exe")) { throw "Qt не установился" }
    Write-Host "Qt готов: $qtDir"
}
