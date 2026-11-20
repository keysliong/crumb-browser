# Сборка CEF с проприетарными кодеками (AAC, H.264) — нужна для Яндекс Музыки,
# Spotify Web и других сайтов, которые отдают звук в AAC / видео в H.264.
#
# Готовые сборки CEF (cef-builds.spotifycdn.com) идут без этих кодеков из-за
# патентных лицензий, поэтому их приходится компилировать вместе с Chromium.
#
#   Место:  ~80 ГБ во время сборки (исходники без истории + объектные файлы),
#           после -Cleanup остаётся ~1 ГБ.
#   Время:  8–12 часов на 4 ядрах / 16 ГБ ОЗУ. Запускайте на ночь.
#   Лицензия: для личного использования; распространять сборку с H.264 без
#           патентной лицензии нельзя.
#
# Запуск (PowerShell из корня проекта):
#   powershell -ExecutionPolicy Bypass -File scripts\build-cef-codecs.ps1
# Затем подключить к Crumb:
#   cmake -DCEF_ROOT=third_party/cef-codecs build
#   cmake --build --preset release --parallel

param(
    [string]$Branch = "8078",              # ветка CEF = номер сборки Chromium (156.0.8078.x)
    [string]$WorkDir = "C:\cefbuild",
    [int]$Jobs = 6,                        # параллельных задач; при 16 ГБ ОЗУ больше не надо
    [switch]$Cleanup                       # удалить исходники после успешной сборки
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
Write-Host "=== Сборка CEF $Branch с кодеками AAC/H.264 в $WorkDir ===" -ForegroundColor Cyan

# 1. Debugging Tools for Windows (часть Windows SDK) — без них Chromium не собирается.
if (-not (Test-Path "C:\Program Files (x86)\Windows Kits\10\Debuggers\x64\dbghelp.dll")) {
    Write-Host "Ставлю Debugging Tools for Windows (подтвердите запрос прав администратора)…"
    winget install --id Microsoft.WindowsSDK.10.0.26100 --source winget --accept-package-agreements `
        --accept-source-agreements --override "/features OptionId.WindowsDesktopDebuggers /quiet /norestart"
    if (-not (Test-Path "C:\Program Files (x86)\Windows Kits\10\Debuggers\x64\dbghelp.dll")) {
        throw "Debugging Tools for Windows не установились"
    }
}

$free = (Get-PSDrive ($WorkDir.Substring(0, 1))).Free / 1GB
if ($free -lt 90) { throw ("Нужно ~90 ГБ свободного места, а есть {0:N0} ГБ" -f $free) }

New-Item -ItemType Directory -Force $WorkDir | Out-Null
$automate = Join-Path $WorkDir "automate-git.py"
Invoke-WebRequest "https://raw.githubusercontent.com/chromiumembedded/cef/master/tools/automate/automate-git.py" `
    -OutFile $automate -UseBasicParsing

# 2. Параметры сборки Chromium/CEF
$env:DEPOT_TOOLS_WIN_TOOLCHAIN = "0"            # используем установленную Visual Studio 2022
$env:GN_DEFINES = "is_official_build=false is_debug=false proprietary_codecs=true ffmpeg_branding=Chrome " +
                  "enable_widevine=true symbol_level=0 blink_symbol_level=0 v8_symbol_level=0 " +
                  "chrome_pgo_phase=0 dcheck_always_on=false"
$env:CEF_ARCHIVE_FORMAT = "tar.bz2"
$env:NINJA_CORE_LIMIT = "$Jobs"                 # autoninja: не больше N задач (памяти 16 ГБ)

# 3. Сборка с пониженным приоритетом — компьютером можно пользоваться
$log = Join-Path $WorkDir "build.log"
Write-Host "Лог: $log  (сборка идёт часами — это нормально)"
$pyArgs = @($automate, "--download-dir=$WorkDir", "--branch=$Branch", "--x64-build", "--no-debug-build",
          "--minimal-distrib", "--no-chromium-history", "--force-build")
$p = Start-Process python -ArgumentList $pyArgs -NoNewWindow -PassThru -RedirectStandardOutput $log `
        -RedirectStandardError "$log.err"
$p.PriorityClass = "BelowNormal"
$p.WaitForExit()
if ($p.ExitCode -ne 0) { throw "Сборка завершилась с ошибкой $($p.ExitCode) — см. $log и $log.err" }

# 4. Распаковать готовый minimal-дистрибутив в third_party/cef-codecs
$distrib = Get-ChildItem "$WorkDir\chromium\src\cef\binary_distrib" -Filter "*windows64_minimal.tar.bz2" |
           Sort-Object LastWriteTime | Select-Object -Last 1
if (-not $distrib) { throw "Не найден готовый архив в binary_distrib" }
$target = Join-Path $root "third_party\cef-codecs"
Remove-Item -Recurse -Force $target -ErrorAction SilentlyContinue
$tmp = Join-Path $root "third_party\_codecs_tmp"
New-Item -ItemType Directory -Force $tmp | Out-Null
tar -xjf $distrib.FullName -C $tmp
Move-Item (Get-ChildItem $tmp -Directory | Select-Object -First 1).FullName $target
Remove-Item -Recurse -Force $tmp
Write-Host "Готово: $target" -ForegroundColor Green
Write-Host "Подключить: cmake -DCEF_ROOT=third_party/cef-codecs build && cmake --build --preset release --parallel"

if ($Cleanup) {
    Write-Host "Удаляю исходники ($WorkDir\chromium)…"
    Remove-Item -Recurse -Force "$WorkDir\chromium"
}
