# build_zh.ps1 — 中文版甲言 (qcc_zh.jy) 自举验证
#
# 流程:
#   1. python scripts/sinicize.py   # 按 scripts/zh_map.txt 把 qcc_work.jy 生成 qcc_zh.jy
#   2. qcc_x86.exe (英文种子) 编译 qcc_zh.jy -> qcc_zh_v1.exe
#   3. qcc_zh_v1.exe 自编译 qcc_zh.jy -> v2 -> v3 -> v4 -> v5
#   4. v1..v5 必须收敛到 $expected (中文不动点)
#
# 英文链不动点 (build.ps1) 不受影响: qcc_x86.c 从未修改。

$ErrorActionPreference = 'Stop'
$expected = '85379af4'  # 2026-08-31 完全中文化 批1+批2+批3a(40库符号)+批3b(流符号桥)+批3c(取环境/x):
                         # 中文自举链不动点
                         # (批3 库桥: 词法器「甲言库函数」表已有的中文名直接改调用点 (比较字符串→strcmp 等),
                         #  词法器 lex 时自动改写回 DLL 符号 → 自举破环自动解, 语义不变;
                         #  流符号 stderr/stdout/stdin + 取环境/判十六进制/格式化限长/取指数 走 #define 桥 +
                         #  词法器反向条目; x 回映射;
                         #  内含 hex 字面量后缀撕碎修复 (0x7FFFFFFFULL 的 ULL 被改名 → 词法器判据坏
                         #  → 字面量误标无符号 → fnptr 比较置高 → qsort 反转 → 自举 3-循环))

Write-Host "== [1/2] 生成 qcc_zh.jy =="
python scripts/sinicize.py
if ($LASTEXITCODE -ne 0) { throw "sinicize 失败" }

Write-Host "== [2/2] 中文自举链 v1..v5 =="
Remove-Item qcc_zh_v1.exe -ErrorAction SilentlyContinue
Remove-Item qcc_zh_v2.exe -ErrorAction SilentlyContinue
Remove-Item qcc_zh_v3.exe -ErrorAction SilentlyContinue
Remove-Item qcc_zh_v4.exe -ErrorAction SilentlyContinue
Remove-Item qcc_zh_v5.exe -ErrorAction SilentlyContinue

& .\qcc_x86.exe srclib_jiayan\qcc_zh.jy -o qcc_zh_v1.exe
if ($LASTEXITCODE -ne 0) { throw "种子编译 qcc_zh.jy 失败" }
& .\qcc_zh_v1.exe srclib_jiayan\qcc_zh.jy -o qcc_zh_v2.exe
& .\qcc_zh_v2.exe srclib_jiayan\qcc_zh.jy -o qcc_zh_v3.exe
& .\qcc_zh_v3.exe srclib_jiayan\qcc_zh.jy -o qcc_zh_v4.exe
& .\qcc_zh_v4.exe srclib_jiayan\qcc_zh.jy -o qcc_zh_v5.exe

$h1 = (Get-FileHash qcc_zh_v1.exe -Algorithm SHA256).Hash
$h2 = (Get-FileHash qcc_zh_v2.exe -Algorithm SHA256).Hash
$h3 = (Get-FileHash qcc_zh_v3.exe -Algorithm SHA256).Hash
$h4 = (Get-FileHash qcc_zh_v4.exe -Algorithm SHA256).Hash
$h5 = (Get-FileHash qcc_zh_v5.exe -Algorithm SHA256).Hash
Write-Host "v1=$($h1.Substring(0,8))"
Write-Host "v2=$($h2.Substring(0,8))"
Write-Host "v3=$($h3.Substring(0,8))"
Write-Host "v4=$($h4.Substring(0,8))"
Write-Host "v5=$($h5.Substring(0,8))"

if (($h1 -eq $h2) -and ($h2 -eq $h3) -and ($h3 -eq $h4) -and ($h4 -eq $h5) -and ($h1.Substring(0,8) -eq $expected)) {
    Write-Host "[PASS] 中文自举链不动点 $expected 达成" -ForegroundColor Green
    exit 0
} else {
    Write-Host "[FAIL] 中文链未收敛到 $expected (实际 $($h1.Substring(0,8)))" -ForegroundColor Red
    exit 1
}
