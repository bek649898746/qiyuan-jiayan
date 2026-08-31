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
$expected = 'cf06acbf'  # 2026-08-31 深度中文化后新不动点 (string_sinicize: 错误消息正文中文化 OOM→内存不足/AST→抽象语法树; 保留 [ERR] 前缀/纯英文判断词/asm/符号模板; 原 46ef0edb 是注释中文化版)  # 2026-08-31 补 signed 中文词 (有符号) 后新不动点 (signed char 中文可写: 有符号 字; kw/strcmp/cast 全链认)  # 2026-08-31 中文链 cast 类型解析中文等价注入修复  # 2026-08-31 中文链 cast 类型解析中文等价注入修复 (layer_change 变量形式词名 strcmp: 转型型/转型型二/中粗) + MAX_LABELS 262144 扩容后新不动点; 原 4a19764d 缺 cast 中文等价 → (无 长 长) cast 8B 标记丢失 → 64 位移位/比较全崩 (t1 c64=0 复现)  # 2026-08-31 无整中文语法缺口修复后新不动点 (sinicize KW_ALIAS unsigned→无整; 原 19277eb6 缺无整连写)  # 2026-08-31 镜像同步修复后新不动点 (qcc_zh.jy 重生成含 ll_ext32/p_esz/movsxd 修复; 原 c198c445 是修复前镜像)  # 2026-08-31 站A+B+C + typedef double + cast32 修复 (csmith s61):
                         # cast32 修复后中文链 4-step 收敛 (v4..v7 稳定; v1 种子 transient)
                         # cast32: 32 位类型 cast 标记表 + nll 清零 (uint32<int64 比较误判无符号根治)
                         # (en 链同步 2DED3F47, 均种子修复)  # 2026-08-31 中文前端语法独立 站A+B+C + typedef double 局部声明修复 (种子层):
                         # implicit_cast_check rsp_off 越界修复 + typedef double 类型标记传导
                         # (en 链同步 BC0E5B22, 均为种子修复)  # 2026-08-31 中文前端语法独立 站A+站B+站C (函数/变量/常量 + 控制词无括号 + 类型别名):
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

Write-Host "== [2/2] 中文自举链 v1..v7 =="
Remove-Item qcc_zh_v1.exe -ErrorAction SilentlyContinue
Remove-Item qcc_zh_v2.exe -ErrorAction SilentlyContinue
Remove-Item qcc_zh_v3.exe -ErrorAction SilentlyContinue
Remove-Item qcc_zh_v4.exe -ErrorAction SilentlyContinue
Remove-Item qcc_zh_v5.exe -ErrorAction SilentlyContinue
Remove-Item qcc_zh_v6.exe -ErrorAction SilentlyContinue
Remove-Item qcc_zh_v7.exe -ErrorAction SilentlyContinue

& .\qcc_x86.exe srclib_jiayan\qcc_zh.jy -o qcc_zh_v1.exe
if ($LASTEXITCODE -ne 0) { throw "种子编译 qcc_zh.jy 失败" }
& .\qcc_zh_v1.exe srclib_jiayan\qcc_zh.jy -o qcc_zh_v2.exe
& .\qcc_zh_v2.exe srclib_jiayan\qcc_zh.jy -o qcc_zh_v3.exe
& .\qcc_zh_v3.exe srclib_jiayan\qcc_zh.jy -o qcc_zh_v4.exe
& .\qcc_zh_v4.exe srclib_jiayan\qcc_zh.jy -o qcc_zh_v5.exe
& .\qcc_zh_v5.exe srclib_jiayan\qcc_zh.jy -o qcc_zh_v6.exe
& .\qcc_zh_v6.exe srclib_jiayan\qcc_zh.jy -o qcc_zh_v7.exe

$h1 = (Get-FileHash qcc_zh_v1.exe -Algorithm SHA256).Hash
$h2 = (Get-FileHash qcc_zh_v2.exe -Algorithm SHA256).Hash
$h3 = (Get-FileHash qcc_zh_v3.exe -Algorithm SHA256).Hash
$h4 = (Get-FileHash qcc_zh_v4.exe -Algorithm SHA256).Hash
$h5 = (Get-FileHash qcc_zh_v5.exe -Algorithm SHA256).Hash
$h6 = (Get-FileHash qcc_zh_v6.exe -Algorithm SHA256).Hash
$h7 = (Get-FileHash qcc_zh_v7.exe -Algorithm SHA256).Hash
Write-Host "v1=$($h1.Substring(0,8))"
Write-Host "v2=$($h2.Substring(0,8))"
Write-Host "v3=$($h3.Substring(0,8))"
Write-Host "v4=$($h4.Substring(0,8))"
Write-Host "v5=$($h5.Substring(0,8))"
Write-Host "v6=$($h6.Substring(0,8))"
Write-Host "v7=$($h7.Substring(0,8))"

# fix 2026-08-31: cast32 修复后中文链多代收敛 (v1->v2->v3->v4 稳定), 判据 = 最后三代收敛 (v5==v6==v7)
if (($h5 -eq $h6) -and ($h6 -eq $h7) -and ($h5.Substring(0,8) -eq $expected)) {
    Write-Host "[PASS] 中文自举链不动点 $expected 达成 (v5..v7 收敛; v1=$($h1.Substring(0,8)) 种子 transient)" -ForegroundColor Green
    exit 0
} else {
    Write-Host "[FAIL] 中文链未收敛到 $expected (实际 v5=$($h5.Substring(0,8)))" -ForegroundColor Red
    exit 1
}
