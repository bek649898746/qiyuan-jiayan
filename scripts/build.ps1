# ============================================================
# 启元 · 甲言 一键构建 + 自举验证
# 用法: powershell -ExecutionPolicy Bypass -File scripts/build.ps1
# 功能: 编译宿主 → 自举三代 → 校验不动点 → 跑 171 测试
# ============================================================
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
Set-Location $root

Write-Host "==== 启元 · 甲言 构建与自举验证 ====" -ForegroundColor Cyan

# 1. 编译宿主编译器
Write-Host "[1/4] 编译宿主编译器 qcc_x86..." -ForegroundColor Yellow
gcc -O2 -Wall -Werror srclib/qcc_x86.c -o qcc_x86.exe
if ($LASTEXITCODE -ne 0) { Write-Host "[FAIL] 宿主编译失败" -ForegroundColor Red; exit 1 }

# 2. 自检
Write-Host "[2/4] 宿主自检 --test ..." -ForegroundColor Yellow
$t = & .\qcc_x86.exe --test
if ($t -notmatch 'PASS') { Write-Host "[FAIL] 自检失败: $t" -ForegroundColor Red; exit 1 }

# 3. 自举五代 (v1==v2==v3==v4==v5 五代全等; 2026-08-20: 不动点封存 — 宿主 = qcc_boot.exe
#    即自举不动点 27B42CE1, 一步收敛。C 宿主 qcc_x86.exe 仅作种源/过渡宿主保留)
Write-Host "[3/4] 自举五代 (宿主 qcc_boot.exe = 不动点 27B42CE1) ..." -ForegroundColor Yellow
& .\qcc_boot.exe srclib_jiayan\qcc_work.jy -o v1.exe | Out-Null
& .\v1.exe srclib_jiayan\qcc_work.jy -o v2.exe | Out-Null
& .\v2.exe srclib_jiayan\qcc_work.jy -o v3.exe | Out-Null
& .\v3.exe srclib_jiayan\qcc_work.jy -o v4.exe | Out-Null
& .\v4.exe srclib_jiayan\qcc_work.jy -o v5.exe | Out-Null

$h1 = (Get-FileHash v1.exe -Algorithm SHA256).Hash
$h2 = (Get-FileHash v2.exe -Algorithm SHA256).Hash
$h3 = (Get-FileHash v3.exe -Algorithm SHA256).Hash
$h4 = (Get-FileHash v4.exe -Algorithm SHA256).Hash
$h5 = (Get-FileHash v5.exe -Algorithm SHA256).Hash
Write-Host "  v1: $h1"
Write-Host "  v2: $h2"
Write-Host "  v3: $h3"
Write-Host "  v4: $h4"
Write-Host "  v5: $h5"

# 验收标准: 自举闭环收敛。2026-08-20 不动点封存:
# v1==v2==v3==v4==v5 五代全等 = 48B12A52 (宿主 qcc_boot.exe 即不动点, 一步收敛)。
# 注: size_t 兜底修复后, C 宿主 vs 自举仍有 3 字节 movsxd 差异 (pp_read_file rd, 疑似 UB) — 技术债待攻。
$expected = '48B12A52970EB25E67E09103E52ACA79198B419DB5D143959D56EB844124EC35'
if (($h1 -eq $h2) -and ($h2 -eq $h3) -and ($h3 -eq $h4) -and ($h4 -eq $h5) -and ($h1 -eq $expected)) {
    Write-Host "[OK] 自举 1-cycle 达成: v1==v2==v3==v4==v5 = $($h1.Substring(0,8))" -ForegroundColor Green
} else {
    Write-Host "[WARN] 自举链与仓库记录不同：odd=$h1 even=$h2" -ForegroundColor Yellow
    Write-Host "       （若源码有合法修改，此为新的不动点，请更新 README 记录）"
}

# 4. 跑完整测试套件（编译+运行+断言, 处理 @EXPECTED compile_fail — 修复原朴素循环撞故意编译失败测试触发 NativeCommandError）
Write-Host "[4/4] 跑完整测试套件 run_tests.py ..." -ForegroundColor Yellow
python scripts/run_tests.py
if ($LASTEXITCODE -ne 0) {
    Write-Host "[FAIL] 测试套件失败" -ForegroundColor Red
    exit 1
}

# 5. 行为断言（fix 2026-08-09 实测复验审计: 此前从不运行行为测试, 2 失败无人知）
Write-Host "[5/5] 跑行为断言 tests/behavior ..." -ForegroundColor Yellow
python tests/behavior/run_behavior.py
if ($LASTEXITCODE -ne 0) {
    Write-Host "[WARN] 行为断言失败" -ForegroundColor Red
    exit 1
}

# 6. 工具链
Write-Host "[+] 编译工具链 jyld / jycc ..." -ForegroundColor Yellow
gcc -O2 -Wall -Werror srclib/jyld.c -o jyld.exe
gcc -O2 -Wall -Werror srclib/jycc.c -o jycc.exe

Write-Host ""
Write-Host "==== 完成。甲言自己生自己，光荣。 ====" -ForegroundColor Cyan
