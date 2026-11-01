# ============================================================
# build.ps1 — 一键构建 + 自举验证（对外入口，2026-11-01 起转发到主链）
#
# 【为什么改】
#   本脚本原名「英文链构建」，验的是 qcc_work.jy（英文→中文镜像链）。
#   而对外主链是 qcc_zh.jy（中文主源，全中文标识符），由 build_zh.ps1 验证。
#   两个脚本两套不动点，导致「照 README 跑 build.ps1」的人拿到与文档不符的结果。
#
# 【现在】
#   本脚本 = build_zh.ps1 的转发器，参数原样传递。
#   跑 `scripts/build.ps1` 与跑 `scripts/build_zh.ps1` 完全等价，都验中文主链。
#
# 【英文链去哪了】
#   原英文链内容保留在 git 历史中（见 commit a67eb026 及之前）。
#   如需单独验英文链：git show a67eb026:scripts/build.ps1 > build_en_legacy.ps1
# ============================================================
param([switch]$Regenerate, [switch]$Full, [switch]$NoSeed, [switch]$Fast)

$ErrorActionPreference = 'Stop'
$target = Join-Path $PSScriptRoot 'build_zh.ps1'

if (-not (Test-Path $target)) {
    Write-Host "[FAIL] 找不到 $target" -ForegroundColor Red
    exit 1
}

Write-Host "==== 甲言 一键构建（转发到主链 build_zh.ps1）====" -ForegroundColor Cyan
Write-Host "  中文主源: srclib_jiayan/qcc_zh.jy" -ForegroundColor Gray
Write-Host ""

& $target @PSBoundParameters
exit $LASTEXITCODE