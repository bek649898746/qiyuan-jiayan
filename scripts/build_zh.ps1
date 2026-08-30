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
$expected = '31786ae6'  # 2026-08-31 完全中文化 批1+批2+批3 + 改层 (中文 token 第一等):
                         # 中文自举链不动点 (v2..v5; v1 是种子 transient — 种子英文词法器与改层后自举词法器
                         # 第一次编译有语义同值差异, 二次后收敛)
                         # 改层: 词法器删「甲言关键字: 中文→英文名」strcpy 改写链 + kw() 直认中文
                         # (整/若/返/构... 与英文平级) + parser strcmp(tn,..) 检查加中文等价
                         # (静/常/无/长/字/短/双/单... 每分支各带 !, 防 !(A||B) 恒假);
                         # 铁证: 同一程序中文版 vs 英文版 → 编译产物逐字节相同 (TK 没变 → 机器码认中文)
                         # 踩坑: 正则先带!后不带!, 负向断言 (?<!!) 防嵌套膨胀 (括号爆炸→LABEL-OVERFLOW)

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

if (($h2 -eq $h3) -and ($h3 -eq $h4) -and ($h4 -eq $h5) -and ($h2.Substring(0,8) -eq $expected)) {
    Write-Host "[PASS] 中文自举链不动点 $expected 达成 (v2..v5 收敛; v1=$($h1.Substring(0,8)) 种子 transient)" -ForegroundColor Green
    exit 0
} else {
    Write-Host "[FAIL] 中文链未收敛到 $expected (实际 v2=$($h2.Substring(0,8)))" -ForegroundColor Red
    exit 1
}
