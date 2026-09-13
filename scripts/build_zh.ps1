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
$expected = 'AB3A0F97'
                        # (2026-09-13) A29 修复 — `返移节` 非聚合豁免 (Copy/标量 return 不是移动):
                        #   病灶: `返移节` (返回=移动) 对 **标量 (Copy) 也置移标**, 与 源移动/参移动 的 Copy 豁免
                        #     不一致 → 循环体内 `return 标量;` 使 循环外 变量 被标「已移」→ A3102
                        #     「循环携带移动」假阳。
                        #   实害 (csmith 差分 20 种子实测): 中文链 **11/20 编译失败** (英文链同批 0 失败) ——
                        #     即 中文链 无法 接替 oracle (09-02 P0 待办 的 判据 答案 = 不能, 成因 即 本 缺陷)。
                        #   修: `型序 < 0 || 类型表[型序].函 <= 0` (非聚合) → 不置移标。聚合 保持 原行为。
                        #   ⚠️ 刻意 **不** 调 `类型可复制`: 它写 `类型表.可复制` 缓存位 → 对 后续注册析构 的类型
                        #     误标 → A3110 (Copy+Drop) 假阳 —— 实测 第一版 因此 回归 feat_zh_typedrop_borrow_ok,
                        #     证实 源码 L6529 的 历史 约束 仍 成立 (两版 试修: ①直接 Copy 查询 → 回归;
                        #     ②先判 析构函 再 查 Copy → 仍 回归 [析构注册 时序 在 return 之后])。
                        #   效果: csmith 差分 zh_cerr 11 → **0**, same 7 → 11 (余 8 DIFF 均 为
                        #     「甲言 运行时除零检查 × gcc 未定义行为」= 安全特性差异, 非缺陷)。
                        #   新锚: feat_zh_ret_scalar_loop_ok.c (闸 617 → 618)
                        #   本次 v1 = v2 = AB3A0F97
                        # 前值 99FB8B3D (T5 二步) · BC350B7F (T5 一步) · ADADA751 (A44c+A44d)
                        # (2026-09-13) T5 第二步 — 5 处 **跨行** 紧跟对 并入 `字段登记`:
                        #   (解析器 里 `字段尺寸递归(...)` 与 `字段类型(...)` 分写 相邻两行 的 站点)
                        #   ① 匿名 结构体 内 字段 (L8695 区)  ② 类型定义 带标签 (L8744 区)
                        #   ③ 匿名 结构体 (暂构序) 数组字段  ④ 类型定义 带标签 数组字段
                        #   ⑤ 匿名 全局 结构体 指针字段
                        #   等价性 前提: 原 `字段类型(..., -1)` 只是 把 毛利 设回 -1, 而 字段创建 时
                        #   毛利 恒 -1 (`字段序标` 4 个 分配点) → `字段登记` 的 `!= -1` 跳过 与 之 等价.
                        #   累计: `字段尺寸递归(` 48 → 35; `字段登记(` 7 → 20.
                        #   本次 v1 = v2 = 99FB8B3D
                        # 前值 BC350B7F (T5 第一步) · ADADA751 (A44c+A44d)
                        # (2026-09-13) T5 字段登记 单点收口 扩面 (结构债, 行为等价):
                        #   ① `字段登记` 的 透传 条件 `若 (构型序 >= 0)` → `!= -1`: 原 条件 把 -2 (函数指针 标记) /
                        #      -3 (接口 隐式槽) 两类 特殊值 **静默 跳过** → 这两 类 调用点 无法 并入 收口点.
                        #      -1 是 唯一 合法「无类型」值 → 放宽 到 `!= -1`; 现有 6 调用点 只 传 -1 或 >= 0, 行为 不变.
                        #   ② 8 处 「字段尺寸递归 + 字段类型」 紧跟对 并入 `字段登记` (含 -3 ×4, -2 ×1, 真类型序 ×3).
                        #      `字段尺寸递归(` 调用点 48 → 40 (1 定义 + 1 前向 + 38 调用), `字段登记(` 7 → 15.
                        #   本次 v1 = v2 = BC350B7F (v1==v2 收敛; 不动点 变化 因 编译器自身 源码 改 → 产物 字节 必变)
                        # 前值 ADADA751 (A44c+A44d) · E56A6391 (A44c)
                        # (2026-09-13) A44c + A44d 根治 (同一家族, 同形态全扫):
                        #   **A44c** 闭包 捕获 **展开路径**: 原 展开循环 条件 带 `&& 匿数 < 255` (内层 接口槽 `&& 匿数 < 250`)
                        #     → 超限 静默丢弃 多余捕获 → 生成 **缺参 的 调用**; 两份 副本 主收集 阈值 还 不一致 (252/255).
                        #     修: 展开前 容量预检 (捕数 + 各接口捕获 槽数, 超 256 → 原得 A5001) + 循环去上限 + 阈值 统一 256.
                        #   **A44d** 主调用路径 **接口盒装展开**: 四处 静默 (探测 匿数+4<250 / 接口引用展开 匿数+4<250 /
                        #     字段传参 匿数+盒N<250 / 内联盒装 匿数+盒N<250, 内层 槽循环 亦 带 匿数<250)
                        #     → 放不下 就 静默 不展开 → 该传 接口引用 却 传了 对象/漏槽 → 运行期 垃圾 (同 A44 家族).
                        #     修: 探测 不设守卫 + 各展开前 预算 (1+N) 超 256 → 原得 + 槽循环 去上限.
                        #   本次 v1 = v2 = v3 = ADADA751 (A44c 时 E56A6391; 两站 都 v1==v2 → 不 影响 编译器自身 布局)
                        # 前值 E56A6391 (A44c) · 18CEC416 (D 系列 收口 五站) · 941A7F15 (位运算族) · 080361B0 (D6)
                        # (2026-09-13) A44c 根治: 闭包 捕获 **展开路径** 的 静默截断 去掉.
                        #   原 两份 副本 的 展开循环 条件 带 `&& 匿数 < 255` (内层 接口槽 展开 `&& 匿数 < 250`)
                        #   → 超限 **静默丢弃** 多余捕获 → 生成 **缺参 的 调用** (静默 错值, 非 报错);
                        #   且 两份 副本 的 主收集 阈值 还 **不一致** (252 vs 255, 与 参数个数[256] 容量 都 不符).
                        #   修: ① 展开前 **容量预检** (捕数 + 各 接口捕获 的 槽数, 超 256 → 原得 确定性拒绝 A5001);
                        #      ② 循环内 不再 设 上限; ③ 阈值 统一 到 容量 256.
                        #   本次 v1 = v2 = v3 = E56A6391 (与 前值 同样 v1==v2 → 改动 不 影响 编译器自身 布局)
                        # 前值 18CEC416 (D 系列 收口 五站) · 941A7F15 (位运算族) · 080361B0 (D6) · 7019D4C0 (D11-B)
                        # (2026-09-13) D 系列 收口 5 站: D2/D8 裸匿名联合 (④ 文件级匿名块 补 联合分派) ·
                        #   D4 位域容器宽度按基类型 (原硬编 4 字节槽: `无 字 a:1` 占 4 应 1) ·
                        #   D5/D9 多维数组字段对齐依据 = 元素尺寸 ·
                        #   D12/D13 大小(&表达式) 恒 8 (原 返 0/随机值, 且 使 含 该形态 的 源 被 A4001 误拒) ·
                        #   D14 联合含数组成员 的 对齐 (联合字段 补 满卯 汇总).
                        #   ⚠️ D5/D9 的 **正确修法** 教训: `字段尺寸递归` 的 `行` 参数 一身二职 (①对齐依据 ②存进
                        #     字段.域行数 = 多维索引 第一维 缩放). 首版 直接 `行 = 新平` 虽 修对 sizeof, 却 把 域行数
                        #     从 行宽 变成 元素尺寸 → 自举断裂 (v2 启动即 0xC0000005; `.data` 少 16384 = 常量表/度伍
                        #     各 -4×2048). 正解 = **拆开**: 新增第 5 参 `对齐依` (46 处调用点 批量补 = 行, 行为不变;
                        #     仅 6 处 多维 调用点 传 `新平`), `行` 恢复 `域尺寸/首` 保 域行数.
                        #   本次 v1 = E6224B94 (种子重推中间态) / v2 = v3 = 18CEC416 (不动点, 收敛 2 段)
                        # 前值 941A7F15 (位运算族) · 080361B0 (D6) · 7019D4C0 (D11-B) · 4EBC0B9B (D11-A) · CEC5FFE3 (T3b)
                        #   缺陷: `~0u` 建节点 未 设 `节无符号` → 后续 `>>` 在 码生成 处 读 该 标记 为 0 → 发射 **算术右移**
                        #     → `(~0u)>>1` = 4294967295 (gcc 2147483647); 值 对 但 **属性 丢**. 影响 4 探针 (const_fold_shr /
                        #     uint_not_shr_min / intwidth_edge / uop_precedence). 对照: 字面量 `0xFFFFFFFFu>>1` 本就对
                        #     (字面量 建节点 设了 无符号), 变量/强转 本就对 ⇒ 缺口 只在 `~`.
                        #   修: 原级() 按位非 分支 建节 后 继承 子0 的 节无符号 (C 语义: ~x 类型 = 提升后 类型).
                        #   另 修 常量折叠 (`末列`/`常量表达式求值`): 新增 静态 `常无符号`, 字面量 记录 + 右移 按 无符号 选 语义
                        #     (编译期 常量 上下文, 数组维度 等 路径).
                        #   锚: feat_zh_uint_shr_ok.c (四格: ~0u>>1 / ~0xFFu>>4 / 字面量 / 经变量).
                        #   本次 v1 = v2 = 941A7F15
                        # 前值 080361B0 (D6) · 7019D4C0 (D11-B) · 4EBC0B9B (D11-A) · CEC5FFE3 (T3b)
$expected_旧1 = '656BD07A'  # (2026-09-13) T1 零静默: `大小(裸标签)` 原 静默 返 4 (`大小(P3)` = 4 而 `大小(构 P3)` = 12;
                        #   裸标签 指针 形态 更 荒谬 `大小(P3 *)` = 16 静默通过) → 「大小(变量名)」 分支 加 命中标记,
                        #   未命中 且 名字 是 已注册 结构体/联合 标签 (尺寸 > 0) → A1001 提示 写 `大小(构 X)`.
                        #   新锚 3: feat_zh_sizeof_tag_ok (正) + feat_zh_bare_tag_sizeof_neg/neg2 (负). 本次 v1 = v2 = 656BD07A
                        # 前值 66A42A17 (嵌套匿名体 对齐 标合)
$expected_旧0 = '66A42A17'  # (2026-09-13) 嵌套匿名体 字段 **对齐** 修复: 匿名嵌套 结构体 注册 时 未 设 标合 (结构体自身对齐)
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
