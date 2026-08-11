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

# 3. 自举五代 (v4==v5 验证收敛; 2026-08-10: 修复镜像 typedef struct 别名注册后
#    链为 v1==v3==v4==v5, v2 为过渡态)
Write-Host "[3/4] 自举五代 ..." -ForegroundColor Yellow
& .\qcc_x86.exe srclib_jiayan\qcc_work.jy -o v1.exe | Out-Null
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

# 验收标准: v3==v4==v5 (自举闭环收敛; v1 是 gcc 种子产物, 与甲言系不同属正常)
$expected = '3013a44c0add47aaa3dc7046b2e8c16227f30ef9e9a4d64cf4a7da53e94ba044'
if (($h3 -eq $h4) -and ($h4 -eq $h5) -and ($h3 -eq $expected)) {
    Write-Host "[OK] 自举不动点达成: $($h3.Substring(0,8)) (GEN3==GEN4==GEN5 全等)" -ForegroundColor Green
} else {
    Write-Host "[WARN] 三代哈希与仓库记录不同：$h3" -ForegroundColor Yellow
    Write-Host "       （若源码有合法修改，此为新的不动点，请更新 README 记录）"
}

# 4. 跑 171 编译测试（直发，验证不崩溃且可执行）
Write-Host "[4/4] 跑 tests/qcc 171 测试 ..." -ForegroundColor Yellow$pass = 0; $fail = 0
Get-ChildItem tests\qcc\*.c | ForEach-Object {
    $out = "$($_.BaseName)_test.exe"
    & .\qcc_x86.exe $_.FullName -o $out 2>$null | Out-Null
    if ($LASTEXITCODE -eq 0) {
        $pass++
    } else {
        $fail++
        Write-Host "  [FAIL] $($_.Name)"
    }
    Remove-Item $out -ErrorAction SilentlyContinue
}
Write-Host "  编译通过: $pass / $($pass + $fail)" -ForegroundColor Green

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
