# ci_zh_feat.ps1 —— CI 的中文特性锚步骤（2026-10-27 审计侧 ci.yml 引用，本脚本补齐）
#
# 为什么不能直接 `python scripts/run_tests_zh.py --zh-feat`：
#   CI 的 tests job 只有中文种子 `qcc_zh_seed.exe`，没有 `qcc_zh_v1.exe`；
#   run_tests_zh.py 的守卫要求 `QCC_TEST_BIN` 指向一个**存在的**编译器，
#   直接调会在「缺主体」上响亮拒绝（rc=2），而不是跑锚。
#
# 本脚本做两件事：
#   ① 用中文种子编当前主源 → qcc_zh_v1.exe（全程中文路径，不碰英文实现）；
#   ② 用 v1 跑全量中文特性锚（`--zh-feat`），并把失败打成 ::error::。
#
# 用法（CI）：powershell -NoProfile -ExecutionPolicy Bypass -File scripts/ci_zh_feat.ps1

$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
Push-Location $root
try {
    if (-not (Test-Path qcc_zh_seed.exe)) {
        Write-Host "::error::ci_zh_feat: qcc_zh_seed.exe missing"
        exit 1
    }
    & .\qcc_zh_seed.exe srclib_jiayan\qcc_zh.jy -o qcc_zh_v1.exe
    if ($LASTEXITCODE -ne 0) {
        Write-Host "::error::ci_zh_feat: seed -> v1 failed rc=$LASTEXITCODE"
        exit 1
    }
    if (-not (Test-Path qcc_zh_v1.exe)) {
        Write-Host "::error::ci_zh_feat: qcc_zh_v1.exe not produced"
        exit 1
    }
    $env:QCC_TEST_BIN = (Resolve-Path qcc_zh_v1.exe).Path
    python -X utf8 scripts\run_tests_zh.py --zh-feat --parallel 2
    if ($LASTEXITCODE -ne 0) {
        Write-Host "::error::ci_zh_feat: zh-feat anchors failed rc=$LASTEXITCODE"
        exit 1
    }
    Write-Host "ci_zh_feat PASS"
} finally {
    Pop-Location
}
