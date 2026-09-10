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
$expected = '93987035'  # 2026-09-11 H1a+H1b 可空 长 长 / 双 真闭环 (不动点 E4CC6AD4->93987035): 首版 H1a 是假通过 (探针写 `长整` — 不是关键字, 被当未知类型静默降级; 真关键字 = `长 长`), 且登记逻辑两处踩坑: ①把 可空针/可空槽表/词位 内联进 余乙/变浮 (定义早于这些全局声明) → 单遍编译器下 落到 隐式新变量 全 0 → 死判断; 修法 = 新 helper 取可空待(名) 定义在 全局之后, 经调用访问; ②可空零值注入 守卫 看 变量表[变量数-1] → 分离类 值槽后 又分配 tag 槽, 末条 = tag 槽 (有值态) → 注入不触发 (未初始化声明 不再为 空); 改 按 名字 查 值槽; ③大 64 位 字面量 被 词法器 截断 → 锚 用 运行时 循环翻倍 造 >32 位 值. 新锚 feat_zh_nullable_{i64_ok, i64_unchecked_neg, f64_ok}. 链 早停 2 段 (v1==v2, 197s); 前: 2026-09-11 H1a 可空 长整 闭环 (不动点 FEC996FD->E4CC6AD4): (不动点 FEC996FD->E4CC6AD4): 载荷 放不进 8B 打包槽 的 第一类 (64 位) — 新表示 值槽 + 分离 tag 槽 `__nt_名` (可空值标=2; tag 0=有值/1=空), 因 余乙/变浮/变构 每次分配后 16 对齐 → 槽不相邻, 不能 用 'tag = 值槽 + 尺寸'; tag 独立变量 可 复用 全部 名字键 机制. 落点: 余乙 (弃可空待→外壹 接受 + 递归 分配 tag 槽) / 新 helper 可空标槽 / 判空 加 ==2 支 (读 __nt_) / 解包tag生成 加 ==2 支 (值 = 64位标记副本, 否则 块末值 被 32 位上下文 截断) / 声明节 长整支 补 tag 写 (子1 零值注入 标记 → 空) / 赋值 长整支 两处 补 tag=0. 新锚 feat_zh_nullable_i64_{ok, unchecked_neg}; 链 早停 2 段 (v1==v2, 203s); 前: 2026-09-10 G2 结构体 载荷 闭环 (不动点 94521241->FEC996FD): 错误 构 X 返回 全尺寸 放行 — ≤8B 小结构体 走 rax 值 + rdx tag (与 长整 同类, 中细 登记 8); >8B 大结构体 走 sret (rcx 隐藏指针, 结构体 由 调用方 缓冲 直写) + rax=tag + rdx=错码 (载荷类 32), 返回侧 浮返 前的 9001 分支 (错 N / 尝试 传播 → 不写结构体, rax=1, rdx=码) + 成功路径 补 tag 清 (rax=0/rdx=0); 消费侧 尝试/解构 类32: 构 槽 临时 (变构, sret 直写) + __qcc_rax 伪变量 捕获 tag (新增, 镜像 __qcc_rdx) + rdx 捕 错码; 载荷类 一致性 校验 覆盖 32; 大结构体 直接转发 `返回 f()` 显式 A6004 拒 (sret 直写 待接). 新锚 4: feat_zh_err_struct_{small_ok, big_ok, mixed_neg, forward_neg}. 链 早停 2 段 (v1==v2, 155s); 前: 2026-09-10 G1 错误 双精度 载荷 尝试 传播 闭环 (不动点 9A4444B9->94521241): 错 N 在 双精度 错误函数 里 原被当 普通 double 返回 (xmm0=码, 无 tag) → 尝试 读 rax 高32 当 tag. 修法: 双通道 {xmm0 值/码 | rdx tag} — 返回侧 浮返 分支 加 9001 判定 (错 N → 码转双 + rdx=1; 普通返回 rdx=0; 直接转发 错误双精度调用 不覆盖 rdx = 透传), 中细 登记 错误双精度 (半错一) 使 tag 判定可用, 错载宽 新增 16 类 (值走 xmm0), 尝试 载荷类 一致性 校验 (混合 → A9001), 解构 双载荷 支持; 新锚 feat_zh_err_double_try_ok + feat_zh_err_double_decons_ok + feat_zh_err_try_mixed_neg; 链 早停 2 段 (v1==v2) 276s; 前: 2026-09-10 P3-15 补挂点 for/do (不动点 BC417BC1->9A4444B9): 循环携带移动 判定 从 仅 循环(while) 扩到 标准 for(;;) + 做/do-while (移环开 快照 / 移环查 判定 对称挂点; 迭代形态 for-range/Vec/切片/数组 已统一走 标准 for 体); 三套自验 全绿 (en 380 + 闸 253 + all 393/4SKIP); 前: 2026-09-10 P3-15 循环携带移动 保守判定 (不动点 F99C5227->BC417BC1)
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

if ($hs.Count -eq 0) { Write-Host "[FAIL] 链未产出 (第 1 代失败)" -ForegroundColor Red; exit 1 }
$finalHash = $hs[$hs.Count - 1]
if ($finalHash -ne $expected) {
    Write-Host "[FAIL] 不动点 $finalHash != 期望 $expected  (链耗时 ${total}s)" -ForegroundColor Red
    exit 1
}
$converged = ($hs.Count -ge 2 -and $hs[$hs.Count - 1] -eq $hs[$hs.Count - 2])
if (-not $converged) {
    Write-Host "[FAIL] $($hs.Count) 段内 无连续两代全等 (疑 振荡/多代漂移) — 链: $($hs -join ' ')" -ForegroundColor Red
    exit 1
}
if ($convDepth -lt 0) { $convDepth = $hs.Count }
Write-Host "[PASS] 中文自举链不动点 $expected 达成 (收敛深度 $convDepth 段: v$($convDepth-1)==v$convDepth; 链耗时 ${total}s)" -ForegroundColor Green
if (-not $NoSeed) { Copy-Item $finalExe qcc_zh_seed.exe -Force; Write-Host "  [seed] qcc_zh_seed.exe <- $finalExe (同步到不动点; -NoSeed 可跳过)" }
exit 0
