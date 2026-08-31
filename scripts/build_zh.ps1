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
$expected = '5a11d87e'  # 2026-08-31 中文前端语法独立 站A+站B+站C (函数/变量/常量 + 控制词无括号 + 类型别名):
                         # 站C: 类型 名 = 基础类型; (整/双/长/短/字/无 字/长 长 — 型定加+左系 注册)
                         # (sinicize 层注入, zh 独有, en 链 BF89F1E8 未动)
                         # 中文自举链不动点 (v2..v5; v1 是种子 transient)
                         # 批4: #define→#定义词/#include→#包含/#ifdef→若有定义/#ifndef→若无定义/#endif→结束若
                         # 用户程序可用中文指令词 (pp 各阶段双认, C 头英文指令保留)
                         # 踩坑链: ①指令判断双认但读名字偏移硬编码英文长度 (寻二+5/6) → 跳指令词修;
                         #  ②边界替换 [6] 与 +6] 两种形态; ③expand/collect/副组 逐字符 define 判断;
                         #  ④毛简 (pp_def_parse) 寻+=6; ⑤函数宏读名字循环缺 0x80 (中文宏名);
                         #  ⑥「预处理行不展开」守卫只认英文首字符 (d/u/i/e/l/p/n) → +0x80;
                         #  ⑦正则先带!后不带! + 负向断言防嵌套膨胀 (LABEL-OVERFLOW)
                         # 验证: 中文指令词程序 (对象宏/函数宏/若有定义/包含/结束若) OK; 功能全绿

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
