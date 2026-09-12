# build_zh.ps1 — 中文版甲言 (qcc_zh.jy) 自举验证
#
# 流程:
#   1. python scripts/sinicize.py   # 按 scripts/zh_map.txt 把 qcc_work.jy 生成 qcc_zh.jy
#   2. qcc_x86.exe (英文种子) 编译 qcc_zh.jy -> qcc_zh_v1.exe
#   3. qcc_zh_v1.exe 自编译 qcc_zh.jy -> v2 -> v3 -> v4 -> v5
#   4. v1..v5 必须收敛到 $expected (中文不动点)
#
# 英文链不动点 (build.ps1) 不受影响: qcc_x86.c 从未修改。

param([switch]$Regenerate, [switch]$Full, [switch]$NoSeed)
$ErrorActionPreference = 'Stop'
$expected = '66A42A17'  # (2026-09-13) 嵌套匿名体 字段 **对齐** 修复: 匿名嵌套 结构体 注册 时 未 设 标合 (结构体自身对齐)
                        #   → 字段尺寸递归 按 大小 猜 对齐 (12 字节 嵌套 → 8) → 外层 满卯 抬到 8 → 总尺寸 12 轮 up 16;
                        #   修 5 处 调用点 (② 具名 路径 L16232 + ③④ 匿名嵌套/内联具名 各两处) + 1 处 手写对 收口 进 字段登记.
                        #   新锚 feat_zh_nested_anon_matrix_ok (8 形态 与 gcc 逐值 一致). 本次 v1 = v2 = 8B9C8656
                        # 前值 FD7B9D80 (探雷 六雷)
$expected_旧1 = 'FD7B9D80'  # (2026-09-13) 探雷 启灵 六雷 续修: 雷4 `返 错 N;` 构造 (原 报 未定义函数 '错') · 雷5 未声明标识符 文案
                        #   (原「未定义函数…多文件」红鲱鱼) · 雷6 实参 成员链 = 潜在出参 (I2 字段级 逃逸, 免 A4001 假阳) ·
                        #   雷2 声明 `可空 T x = 空;` 置空生效 + 值位置 `空` 响亮报错 A8002. 本次 v1 = v2 = FD7B9D80
                        # 前值 ADDC80B0 (R4)
$expected_旧 = 'ADDC80B0'  # (2026-09-13) R4: `尺寸(((struct T *)0)->字段)` —— 反射 路径 补 结构词 前缀 (struct/union/构/联);
                        #   原 判据 只认 (词变量|型词)+`*` → C 形态 落 通用表达式 → 码生成 对成员取址 → 未定义函数 'x'. 本次 v1 = v2 = ADDC80B0
                        # 前值 4BDDCAD2 (R3-匿 匿名体别名 等长重写)
$expected_前 = '4BDDCAD2'  # (2026-09-13) R3-匿: 匿名体 中文 类型别名 (类型 名 = 构/联合/枚 { ... };) 词元级 等长重写 →
                        #   文件级 复用 具名标签解析器 (原 解析中断); 函数级 具名/别名链 取真尺寸 (原 静默 4);
                        #   函数级 匿名体 → 响亮报错 A1002; 词法 补注册 中文关键字 类型定义 ≡ typedef. 本次 v1 = v2 = 4BDDCAD2
                        # 前值 6349DC50 (启元复盘 R1b/R2/R3). 注: 中间版本 C0BD5664 因 ③ 分支 插在 `;` 消费 之后
                        #   (词位 已 越过 语句 → 下一条 语句 首 类型名 被 误吃) 而 回退重做 ⇒ 见 施工日志
                         # 前值 395B5732 (R1a)

Write-Host "== [1/2] 生成 qcc_zh.jy =="
if ($Regenerate) {
    python scripts/sinicize.py
    if ($LASTEXITCODE -ne 0) { throw "sinicize 失败" }
} else {
    # fix 2026-08-31 中文主源模式: qcc_zh.jy 是主源, 特性直接写中文端, sinicize 不再覆盖
    # (英文源修复需同步时, 手动跑 sinicize.py 重建后重做中文特性; 或显式 -Regenerate)
    Write-Host "  [主源] 使用现有 srclib_jiayan/qcc_zh.jy (跳过 sinicize; 如需重建加 -Regenerate)"
    if (-not (Test-Path srclib_jiayan\qcc_zh.jy)) { throw "qcc_zh.jy 不存在, 请先 -Regenerate 生成" }
}

Write-Host "== [2/2] 中文自举链 (早停: 连续两代全等 = 不动点; 小改通常 2 段 ~2.5min; -Full 强制 7 段) =="
foreach ($n in 1..7) { Remove-Item "qcc_zh_v$n.exe" -ErrorAction SilentlyContinue }

# S2b-2 (2026-09-09): seed 从 qcc_x86 切换 自举不动点编译器 — qcc_x86 (旧 frozen 英文) 已无法编译当前规模中文源 (死循环), 仅作 oracle/独立验证
if (-not (Test-Path qcc_zh_seed.exe)) { if (Test-Path qcc_zh_v7.exe) { Copy-Item qcc_zh_v7.exe qcc_zh_seed.exe } else { throw "qcc_zh_seed.exe 缺失: 先用现有自举 vN 复制为 qcc_zh_seed.exe" } }

# 2026-09-10 大哥: 去冗余 — 原固定跑 v1..v7 (7×~100s ≈ 11min) 只为看 v5==v6==v7;
# 但"连续两代全等"已经 = 不动点 (编译器 编自己 得同一二进制), 后续代 必然同值 → 早停省 ~8min/站。
# 小改默认 2 段; 只有 真不收敛 (v1!=v2) 时才自动续跑, 用最少的代 定位收敛深度。
$chainT0 = Get-Date
$prevExe = 'qcc_zh_seed.exe'; $prevName = 'seed'
$hs = @(); $finalExe = ''; $convDepth = -1
foreach ($n in 1..7) {
    $out = "qcc_zh_v$n.exe"
    $t0 = Get-Date
    & ".\$prevExe" srclib_jiayan\qcc_zh.jy -o $out
    if ($LASTEXITCODE -ne 0) { throw "第 $n 代 编译失败 (上一代 = $prevName)" }
    $h = (Get-FileHash $out -Algorithm SHA256).Hash.Substring(0,8)
    $sec = [math]::Round(((Get-Date) - $t0).TotalSeconds, 1)
    $finalExe = $out; $hs += $h
    if ($n -ge 2 -and $h -eq $hs[$hs.Count - 2]) {
        Write-Host ("  v{0} = {1}   ({2,5}s, 由 {3} 编译)  <- 与 v{4} 全等" -f $n, $h, $sec, $prevName, ($n-1))
        if (-not $Full) { $convDepth = $n; break }
    } else {
        Write-Host ("  v{0} = {1}   ({2,5}s, 由 {3} 编译)" -f $n, $h, $sec, $prevName)
    }
    $prevExe = $out; $prevName = "v$n"
}
$total = [math]::Round(((Get-Date) - $chainT0).TotalSeconds, 1)

if ($hs.Count -eq 0) { Write-Host "[失败 FAIL] 链未产出 (第 1 代失败)" -ForegroundColor Red; exit 1 }
$finalHash = $hs[$hs.Count - 1]
if ($finalHash -ne $expected) {
    Write-Host "[失败 FAIL] 不动点 $finalHash != 期望 $expected  (链耗时 ${total}s)" -ForegroundColor Red
    exit 1
}
$converged = ($hs.Count -ge 2 -and $hs[$hs.Count - 1] -eq $hs[$hs.Count - 2])
if (-not $converged) {
    Write-Host "[失败 FAIL] $($hs.Count) 段内 无连续两代全等 (疑 振荡/多代漂移) — 链: $($hs -join ' ')" -ForegroundColor Red
    exit 1
}
if ($convDepth -lt 0) { $convDepth = $hs.Count }
Write-Host "[通过 PASS] 中文自举链不动点 $expected 达成 (收敛深度 $convDepth 段: v$($convDepth-1)==v$convDepth; 链耗时 ${total}s)" -ForegroundColor Green
if (-not $NoSeed) { Copy-Item $finalExe qcc_zh_seed.exe -Force; Write-Host "  [种子 seed] qcc_zh_seed.exe <- $finalExe (同步到不动点; -NoSeed 可跳过)" }
exit 0
