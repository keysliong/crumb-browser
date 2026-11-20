# Скачивает CEF с проприетарными кодеками (AAC, H.264) в third_party/cef-codecs.
# Нужен для Яндекс Музыки, Twitter/X-видео и других сайтов с AAC/H.264.
#
# Источник — сторонняя сборка (github.com/codeforenode/enode, CEF 149.0.7827.156):
# официальные сборки CEF идут без этих кодеков из-за патентов. Архив проверяется
# по SHA-256. Альтернатива без чужих бинарников — scripts/build-cef-codecs.ps1
# (сборка из исходников, ~10 часов).
#
#   powershell -ExecutionPolicy Bypass -File scripts\setup-codecs.ps1
#   cmake -DCEF_ROOT=third_party/cef-codecs build   (или пересоздать build)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$url = "https://github.com/codeforenode/enode/releases/download/cef-149.0.7827.156/enode_cef_149.0.7827.156_chrome_win64.tar.bz2"
$sha = "e25a19a756aab54e0ab13ce0bbb9c342246d883fdbf39e82cca04f5f225a4f18"
$dl = Join-Path $root "third_party\_download\enode"
$archive = Join-Path $dl "enode.tar.bz2"
New-Item -ItemType Directory -Force $dl | Out-Null

if (-not (Test-Path $archive) -or (Get-FileHash $archive -Algorithm SHA256).Hash.ToLower() -ne $sha) {
    Write-Host "Скачиваю CEF с кодеками (~160 МБ)…"
    Invoke-WebRequest $url -OutFile $archive -UseBasicParsing
}
$got = (Get-FileHash $archive -Algorithm SHA256).Hash.ToLower()
if ($got -ne $sha) { Remove-Item $archive; throw "SHA-256 не совпал: $got" }

$target = Join-Path $root "third_party\cef-codecs"
$tmp = Join-Path $root "third_party\_codecs_tmp"
Remove-Item -Recurse -Force $tmp, $target -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force $tmp | Out-Null
tar -xjf $archive -C $tmp
Move-Item (Get-ChildItem $tmp -Directory | Select-Object -First 1).FullName $target
Remove-Item -Recurse -Force $tmp
Write-Host "Готово: $target" -ForegroundColor Green
