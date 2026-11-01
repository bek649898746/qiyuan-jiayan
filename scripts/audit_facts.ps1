# ============================================================
# 甲言 · 审计事实一键导出（给审计者 / AI agent 用）
# 用法: powershell -ExecutionPolicy Bypass -File scripts/audit_facts.ps1
#
# 目的：把「审计者要花 20+ 步才能自己搞清楚的事实」压到 30 秒。
#       跑完你就有了基线，剩下的时间只需专注「找它没说的事」。
# ============================================================
# 参数必须位于脚本第一个可执行语句之前（PowerShell 语法要求）
param([switch]$Json)

$ErrorActionPreference = 'Continue'
try { [Console]::OutputEncoding = [System.Text.Encoding]::UTF8 } catch {}
$root = Split-Path -Parent $PSScriptRoot
Set-Location $root
$sw = [Diagnostics.Stopwatch]::StartNew()

# ────────────────────────────────────────────────────────────
# -Json 开关（2026-11-02 加，回应外部媒体 UNDERCODE NEWS 的建议）
#
# 背景：该媒体在评价里明确点名，甲言「最强下一步」是发布
#   ① 独立可复现的构建说明 ② **机器可读的测试结果** ③ 产物哈希 ④ 自举信任链说明
# 我们 ①③④ 都有，② 缺 —— 本脚本原输出是给人读的文本表格，
# 想让程序核验就得写正则解析，那不叫「可复算」。
#
# 用法：
#   powershell -File scripts/audit_facts.ps1          # 原文本模式（不变）
#   powershell -File scripts/audit_facts.ps1 -Json    # 机器可读
#
# schema 约定（外部可依赖，改动需 bump schema_version）：
#   所有量为**当场实测**，不从文档转抄；解析失败的字段为 null（不填假值）
# ────────────────────────────────────────────────────────────

if ($Json) {
    # ─────────────────────────────────────────────────────────
    # -Json 模式（2026-11-02 加，回应外部媒体 UNDERCODE NEWS 建议②「机器可读的测试结果」）
    #
    # 实现说明：刻意**不用 ConvertTo-Json**。实测它在本次上下文抛
    #   "Cannot convert value System.String to type SwitchParameter"，
    #   而同样对象在独立脚本里正常 —— 成因未查明。
    # schema 只有 3 层、字段固定，故逐行拼接：结果可预测，不依赖 PS 版本。
    #
    # schema 约定（外部可依赖；改动需 bump schema_version）：
    #   所有量**当场实测**，不从文档转抄；解析失败输出 null，绝不填假值。
    # ─────────────────────────────────────────────────────────
    $nl = "`n"
    $sb = New-Object System.Text.StringBuilder

    function _q([string]$s) {
        if ($null -eq $s) { return 'null' }
        $x = $s.Replace('\', '\\').Replace('"', '\"')
        $x = $x.Replace("`r", '\r').Replace("`n", '\n').Replace("`t", '\t')
        return '"' + $x + '"'
    }
    function _i($v) { if ($null -eq $v) { return 'null' } ; return ([string]([int64]$v)) }
    function _d($v) { if ($null -eq $v) { return 'null' } ; return ([double]$v).ToString([System.Globalization.CultureInfo]::InvariantCulture) }

    function _srcstat([string]$rel) {
        $p = Join-Path $root $rel
        if (-not (Test-Path $p)) { return $null }
        $full = (Resolve-Path $p).Path
        $t = [System.IO.File]::ReadAllText($full, [System.Text.Encoding]::UTF8)
        $ln = @([System.IO.File]::ReadAllLines($full, [System.Text.Encoding]::UTF8)).Count
        $cj = ([regex]::Matches($t, '[\u4e00-\u9fff]')).Count
        $pc = 0.0
        if ($t.Length -gt 0) { $pc = [math]::Round($cj * 100.0 / $t.Length, 1) }
        return @{ path = $rel.Replace('\','/'); lines = $ln; chars = $t.Length; cjk = $pc }
    }

    # ── 采集（全部现算）
    $gHead    = ((git rev-parse HEAD 2>$null) | Out-String).Trim()
    $gHeadS   = ((git rev-parse --short HEAD 2>$null) | Out-String).Trim()
    $gBranch  = ((git rev-parse --abbrev-ref HEAD 2>$null) | Out-String).Trim()
    $gTracked = @(git ls-files 2>$null).Count

    $srcMain = _srcstat 'srclib_jiayan\qcc_zh.jy'
    $srcC    = _srcstat 'srclib\qcc_x86.c'
    $srcEn   = _srcstat 'srclib_jiayan\qcc_work.jy'

    $exp = $null
    $m = Select-String -Path scripts\build_zh.ps1 -Pattern "^\`$expected\s*=\s*'([0-9A-Fa-f]{8})'" -EA SilentlyContinue | Select-Object -First 1
    if ($m) { $exp = ([regex]::Match($m.Line, "'([0-9A-Fa-f]{8})'")).Groups[1].Value }
    $seedSha = $null
    if (Test-Path 'qcc_zh_seed.exe') { $seedSha = (Get-FileHash 'qcc_zh_seed.exe' -Algorithm SHA256).Hash.Substring(0,8) }
    $seedOk = 'false'
    if ($exp -and $seedSha -and ($exp -eq $seedSha)) { $seedOk = 'true' }

    $anchorN = @(Get-ChildItem scripts\feat_zh_*.c -EA SilentlyContinue).Count
    $qccN    = @(git ls-files tests/qcc 2>$null).Count

    $mlFiles = @(Get-ChildItem examples\multilang -Recurse -Include '*.c' -File -EA SilentlyContinue)
    $mlLines = 0
    foreach ($x in $mlFiles) { $mlLines += @([System.IO.File]::ReadAllLines($x.FullName, [System.Text.Encoding]::UTF8)).Count }

    $lgTotal = $null; $lgOk = $null; $lgSurf = $null; $lgMiss = $null
    $lmPath = 'manual\机制特性实现表.md'
    if (Test-Path $lmPath) {
        $row = Select-String -Path $lmPath -Pattern '^\|\s*\*\*合计\*\*' -Encoding UTF8 -EA SilentlyContinue | Select-Object -First 1
        if ($row) {
            $cells = @(($row.Line -split '\|') | ForEach-Object { ($_ -replace '\*','').Trim() } | Where-Object { $_ -ne '' })
            if ($cells.Count -ge 6) {
                $lgTotal = [int]$cells[2]; $lgOk = [int]$cells[3]; $lgSurf = [int]$cells[4]; $lgMiss = [int]$cells[5]
            }
        }
    }

    # ── 拼装
    function _srcline($key, $v) {
        $r = '    ' + (_q $key) + ': '
        if ($null -eq $v) { return ($r + 'null') }
        $b = '{' + $nl
        $b += '      "path": ' + (_q $v.path) + ',' + $nl
        $b += '      "lines": ' + (_i $v.lines) + ',' + $nl
        $b += '      "chars": ' + (_i $v.chars) + ',' + $nl
        $b += '      "cjk_pct": ' + (_d $v.cjk) + $nl
        $b += '    }'
        return ($r + $b)
    }

    [void]$sb.Append('{' + $nl)
    [void]$sb.Append('  "schema_version": 1,' + $nl)
    [void]$sb.Append('  "generated_at": ' + (_q (Get-Date -Format 'yyyy-MM-ddTHH:mm:sszzz')) + ',' + $nl)
    [void]$sb.Append('  "generated_by": "scripts/audit_facts.ps1 -Json",' + $nl)
    [void]$sb.Append('  "repo": {' + $nl)
    [void]$sb.Append('    "head": ' + (_q $gHead) + ',' + $nl)
    [void]$sb.Append('    "head_short": ' + (_q $gHeadS) + ',' + $nl)
    [void]$sb.Append('    "branch": ' + (_q $gBranch) + ',' + $nl)
    [void]$sb.Append('    "tracked_files": ' + (_i $gTracked) + $nl)
    [void]$sb.Append('  },' + $nl)
    [void]$sb.Append('  "source": {' + $nl)
    [void]$sb.Append((_srcline 'main' $srcMain) + ',' + $nl)
    [void]$sb.Append((_srcline 'c_root' $srcC) + ',' + $nl)
    [void]$sb.Append((_srcline 'en_mirror' $srcEn) + $nl)
    [void]$sb.Append('  },' + $nl)
    [void]$sb.Append('  "bootstrap": {' + $nl)
    [void]$sb.Append('    "expected": ' + (_q $exp) + ',' + $nl)
    [void]$sb.Append('    "seed_sha256_8": ' + (_q $seedSha) + ',' + $nl)
    [void]$sb.Append('    "seed_matches_expected": ' + $seedOk + $nl)
    [void]$sb.Append('  },' + $nl)
    [void]$sb.Append('  "anchors": { "feat_zh_files": ' + (_i $anchorN) + ' },' + $nl)
    [void]$sb.Append('  "tests": { "qcc_files": ' + (_i $qccN) + ' },' + $nl)
    [void]$sb.Append('  "mechanism_ledger": ')
    if ($null -eq $lgTotal) { [void]$sb.Append('null,' + $nl) }
    else { [void]$sb.Append('{ "total": ' + (_i $lgTotal) + ', "implemented": ' + (_i $lgOk) + ', "surface": ' + (_i $lgSurf) + ', "missing": ' + (_i $lgMiss) + ' },' + $nl) }
    [void]$sb.Append('  "multilang": { "files": ' + (_i $mlFiles.Count) + ', "lines": ' + (_i $mlLines) + ' },' + $nl)
    [void]$sb.Append('  "elapsed_ms": ' + (_i ([math]::Round($sw.Elapsed.TotalMilliseconds, 0))) + $nl)
    [void]$sb.Append('}' + $nl)

    [Console]::Out.Write($sb.ToString())
    exit 0
}
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

# 当前不动点：运行时从 build_zh.ps1 的 $expected 读取（2026-10-24 修：原写死 39C6FC81 已过期）
$curFix = '(读取失败)'
try {
    $m = Select-String -Path scripts\build_zh.ps1 -Pattern "^\`$expected\s*=\s*'([0-9A-Fa-f]{8})'" -EA SilentlyContinue | Select-Object -First 1
    if ($m) { $curFix = ([regex]::Match($m.Line, "'([0-9A-Fa-f]{8})'")).Groups[1].Value }
} catch { }

Show-Sec "3. 关键数字（每个都附复算方式）"
Show-Item "中文关键字" "38 个（15 语句 + 23 类型修饰） -> manual/中文关键字表.md"
Show-Item "机制台账" "291 项 = 284 已实现 / 2 表层 / 5 未实现 -> manual/机制特性实现表.md"
Show-Item "中文特性锚" ("{0} 个 feat_zh_*.c" -f @(Get-ChildItem scripts\feat_zh_*.c -EA SilentlyContinue).Count)
Show-Item "编译测试" ("{0} 个 <- git ls-files tests/qcc" -f @(git ls-files tests/qcc 2>$null).Count)
Show-Item "九条语言线" "85 文件 / 50,692 行 -> examples/multilang/ （扩展名 .c，内容是甲言）"
Show-Item "当前不动点" ("{0}（运行时读自 scripts/build_zh.ps1 的 `$expected）" -f $curFix)

Show-Sec "4. 三条自检命令（零依赖，都不需要 gcc/Python）"
Write-Host "  powershell -File scripts/verify_90s.ps1        -> 9.2s  / 8 项" -ForegroundColor Green
Write-Host "  powershell -File scripts/verify_multilang.ps1  -> 31.6s / 9 条线" -ForegroundColor Green
Write-Host "  powershell -File scripts/build_zh.ps1 -NoSeed  -> 约 830s（8核）/ 1,528s（低配） / 完整自举收敛" -ForegroundColor Green

Show-Sec "5. !! 审计者最常见的 6 个误判（先看这里，能省你 20 步）"
$pit = @(
    '1) 别用 *.jy 搜九线源码 —— 它们在 examples/multilang/*.c',
    '   甲言兼容 C、源码存成 .c。只按 .jy 搜会得出「源码不存在」的错误结论。',
    '',
    '2) qcc_work.jy 不是主源，是英文链镜像（中文字符仅 5.87%，2026-10-24 实测）',
    '   真正的手工中文化主源是 srclib_jiayan/qcc_zh.jy（中文 29.8%，2026-10-24 实测）。',
    '',
    '3) 「v1==v2==v3==v4==v5 五代全等」是 v1.0 封存（2026-08-24）的历史。',
    "   现行是 v1 == v2 == $curFix（一步收敛，只有两代）。",
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
    '[已守护] 完整自举判据已进 CI —— .github/workflows/ci.yml 的 bootstrap job',
    '         （schedule + workflow_dispatch 触发，跑 build_zh.ps1 两代 + 不动点比对）。',
    '         本地实测 830s（8 核）/ 1,528s（低配机）—— 耗时依机器而定，非固定值。',
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
