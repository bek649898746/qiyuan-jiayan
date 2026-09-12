# check_fast.ps1 — G0-3 (2026-09-11) 小改 快速 回路 (反馈 提速)
#
# 用途: 「小改」 时 不 跑 自举链、不 换 seed、不 跑 全闸 —— 只 做 两 件事:
#   ① 用 **当前 seed** 编 当前 源 出 一个 探针 编译器 (probe.exe, 一次 ~1-2min);
#   ② 用 probe 只 跑 **目标锚** (1-3 个, 每个 ~0.1s)。
# 判据: 目标锚 全绿 → 改动 行为 对 (自行 定位 级); 再 决定 是否 上 链 验收。
#
# 用法:
#   powershell -NoProfile -File scripts/check_fast.ps1 scripts/feat_x.c,scripts/feat_y.c
#   powershell -NoProfile -File scripts/check_fast.ps1                     # 只 编 探针, 不 跑 锚
#
# 与 全链 的 分工 (验证 分层 纪律):
#   小改 → 本 脚本 (≤3min)  |  公共闸/收尾 → build_zh.ps1 + run_tests_zh.py --zh-feat + check_closure.ps1
#
param(
    [string]$Only = '',
    [switch]$Quiet
)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
Set-Location $root

$probe = 'scratch_gl\probe.exe'
New-Item -ItemType Directory -Force -Path 'scratch_gl' | Out-Null
Write-Host "== check_fast: 旧 seed 编 当前 源 → 探针 编译器 =="
$t0 = Get-Date
& .\qcc_zh_seed.exe srclib_jiayan\qcc_zh.jy -o $probe
if ($LASTEXITCODE -ne 0) { Write-Host "  [失败 FAIL] 探针 编译 失败 (当前 源 编 不过 → 立即 回查)"; exit 1 }
Write-Host ("  [通过 PASS] 探针 通过 ({0:n0}s)" -f ((Get-Date) - $t0).TotalSeconds)

if (-not $Only) { Write-Host "  (未 给 --Only 锚 列表 → 只 编 探针)"; exit 0 }

$env:QCC_TEST_BIN = $probe
Write-Host "== check_fast: 目标 锚 (probe) =="
python scripts/run_tests_zh.py --only $Only
$rc = $LASTEXITCODE
Remove-Item Env:\QCC_TEST_BIN -ErrorAction SilentlyContinue
if ($rc -ne 0) { Write-Host "  [失败 FAIL] 目标 锚 有 失败 → 改动 未 达标"; exit 1 }
Write-Host "  [通过 PASS] 目标 锚 全绿 (≤3min 回路; 验收 再 上 链 + 闸)"
exit 0
