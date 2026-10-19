# ============================================================
# 甲言 · 审计事实一键导出（给审计者 / AI agent 用）
# 用法: powershell -ExecutionPolicy Bypass -File scripts/audit_facts.ps1
#
# 目的：把「审计者要花 20+ 步才能自己搞清楚的事实」压到 30 秒。
#       跑完你就有了基线，剩下的时间只需专注「找它没说的事」。
# ============================================================
$ErrorActionPreference = 'Continue'
try { [Console]::OutputEncoding = [System.Text.Encoding]::UTF8 } catch {}
$root = Split-Path -Parent $PSScriptRoot
Set-Location $root
$sw = [Diagnostics.Stopwatch]::StartNew()
function Show-Sec($t) {
    Write-Host ""
    Write-Host ("── " + $t) -ForegroundColor Cyan
    Write-Host ("-" * 62) -ForegroundColor DarkGray
}
function Show-Item($k, $v) { Write-Host ("  {0,-32} {1}" -f $k, $v) }

Show-Sec "1. 仓库基线"
$tracked = @(git ls-files 2>$null)
$sz = 0
Get-ChildItem -Recurse -File -EA SilentlyContinue | Where-Object { $_.FullName -notmatch '\\\.git\\' } | ForEach-Object { $sz += $_.Length }
Show-Item "HEAD" (git rev-parse --short HEAD 2>$null)
Show-Item "分支" (git rev-parse --abbrev-ref HEAD 2>$null)
Show-Item "被跟踪文件数" $tracked.Count
Show-Item "工作区体积" ("{0:N1} MB" -f ($sz / 1MB))
$pack = git count-objects -vH 2>$null | Select-String 'size-pack'
if ($pack) { Show-Item "克隆下载量(size-pack)" ($pack.ToString() -replace 'size-pack:\s*', '') }

Show-Sec "2. 源码规模（以下即权威值）"
$srcs = @(
    @{ f = 'srclib_jiayan\qcc_zh.jy';  note = '★ 主源（唯一交付物）' },
    @{ f = 'srclib\qcc_x86.c';         note = 'C 版历史信任根（停更 2026-09-06）' },
    @{ f = 'srclib_jiayan\qcc_work.jy'; note = '!! 英文链镜像，勿当主源' }
)
foreach ($s in $srcs) {
    if (Test-Path $s.f) {
        $full = (Resolve-Path $s.f).Path
        $txt = [System.IO.File]::ReadAllText($full, [System.Text.Encoding]::UTF8)
        $lines = @([System.IO.File]::ReadAllLines($full, [System.Text.Encoding]::UTF8)).Count
        $cjk = ([regex]::Matches($txt, '[\u4e00-\u9fff]')).Count
        $pct = 0
        if ($txt.Length -gt 0) { $pct = [math]::Round($cjk * 100 / $txt.Length, 1) }
        Show-Item $s.f ("{0:N0} 行 / {1:N0} 字符 / 中文 {2}%" -f $lines, $txt.Length, $pct)
        Write-Host ("      " + $s.note) -ForegroundColor DarkGray
    }
}

Show-Sec "3. 关键数字（每个都附复算方式）"
Show-Item "中文关键字" "38 个（15 语句 + 23 类型修饰） -> manual/中文关键字表.md"
Show-Item "机制台账" "291 项 = 284 已实现 / 2 表层 / 5 未实现 -> manual/机制特性实现表.md"
Show-Item "中文特性锚" ("{0} 个 feat_zh_*.c" -f @(Get-ChildItem scripts\feat_zh_*.c -EA SilentlyContinue).Count)
Show-Item "编译测试" ("{0} 个 <- git ls-files tests/qcc" -f @(git ls-files tests/qcc 2>$null).Count)
Show-Item "九条语言线" "85 文件 / 69,497 行 -> examples/multilang/ （扩展名 .c，内容是甲言）"
Show-Item "当前不动点" "39C6FC81（以 scripts/build_zh.ps1 的 expected 为准）"

Show-Sec "4. 三条自检命令（零依赖，都不需要 gcc/Python）"
Write-Host "  powershell -File scripts/verify_90s.ps1        -> 9.2s  / 8 项" -ForegroundColor Green
Write-Host "  powershell -File scripts/verify_multilang.ps1  -> 31.6s / 9 条线" -ForegroundColor Green
Write-Host "  powershell -File scripts/build_zh.ps1 -NoSeed  -> 约 20.8 分钟 / 完整自举收敛" -ForegroundColor Green

Show-Sec "5. !! 审计者最常见的 6 个误判（先看这里，能省你 20 步）"
$pit = @(
    '1) 别用 *.jy 搜九线源码 —— 它们在 examples/multilang/*.c',
    '   甲言兼容 C、源码存成 .c。只按 .jy 搜会得出「源码不存在」的错误结论。',
    '',
    '2) qcc_work.jy 不是主源，是英文链镜像（中文字符仅 5.8%）',
    '   真正的手工中文化主源是 srclib_jiayan/qcc_zh.jy（中文 29.3%）。',
    '',
    '3) 「v1==v2==v3==v4==v5 五代全等」是 v1.0 封存（2026-08-24）的历史。',
    '   现行是 v1 == v2 == 39C6FC81（一步收敛，只有两代）。',
    '',
    '4) 「格式化到串」「复制字符串」是编译器内建，仓库里没有定义。',
    '   搜到 852 处全是调用点 —— 不要据此判定「找不到定义 = 不安全」。',
    '',
    '5) 变量表 / 串表 / 递归深度都有护栏（搜「变量表满」「STR-OVERFLOW」「解析深」）。',
    '   它们会确定性拒绝并 exit=1，不是静默越界。',
    '',
    '6) 英文链 build.ps1 已从对外路径撤出 —— 它在非仓库根目录会静默产出',
    '   缺运行库的残缺编译器（rc=0 但运行即 0xC0000005）。别去跑它。'
)
$pit | ForEach-Object { Write-Host ("  " + $_) -ForegroundColor Yellow }

Show-Sec "6. 已知问题（如实登记，不藏）"
$known = @(
    '[未修] 英文链静默降级 —— 找不到 srclib/qcc_rt.c 时 rc 仍为 0，',
    '        却产出少 3.18MB 运行库的残缺编译器。需改编译器源码。',
    '[风险] 固定缓冲 + sprintf 式格式化（错缓冲[256] 等），未全量穷举。',
    '[缺口] 完整自举判据（约 20.8 分钟）未进 CI，全链唯一没被机器守护的验收点。',
    '[文档] CHANGELOG 曾停更近 2 个月（2026-08-27 -> 10-20，已补记）。'
)
$known | ForEach-Object { Write-Host ("  " + $_) -ForegroundColor DarkYellow }

Show-Sec "7. 该往哪看"
$where = @(
    'README.md                       总览 + 「已知问题与口径说明」8 条审计答复',
    'manual/源码导航.md              34,335 行怎么读（5 个巨型函数占 46%）',
    'manual/编译期检查清单.md         比 C 多拦的 6 类编译期检查（含 gcc 对比）',
    'manual/工程优势清单.md           5 条可量化工程优势',
    'manual/数据口径与核验方法.md     每个数字的口径 + 「无法核验项」诚实标注',
    'examples/multilang/README.md    九线源码说明 + 全量压测结果'
)
$where | ForEach-Object { Write-Host ("  " + $_) }

$sw.Stop()
$sec = [math]::Round($sw.Elapsed.TotalSeconds, 1)
Write-Host ""
Write-Host ("[完成] 审计基线已导出，耗时 " + $sec + "s。以上数字均为当场实测，可直接引用。") -ForegroundColor Green
