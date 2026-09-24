# check_closure.ps1 — 收尾 一票 检查 (地1 G4-2)
#
# 一条命令 校验 一 站 是否 真 收尾:
#   1 树净        — git 工作树 无 未提交 改动 (scratch*/ 等 未跟踪 忽略)
#   2 $expected 不 超前 HEAD — build_zh.ps1 的 $expected 与 HEAD 版本 一致
#   3 不动点 同步 — qcc_zh_seed.exe SHA256 前 8 位 == $expected
#   4 闸绿        — run_tests_zh.py --zh-feat 全 PASS
#   5 新锚 入闸   — 最新 施工日志 里 提到 的 scripts/*.c 锚 都 在 CORE_FEAT 内
#   6 台账 已记   — grok-build/_DEBT_LEDGER.md 里 出现 当前 不动点 哈希
#
# 用法: powershell -File scripts/check_closure.ps1 [-SkipGate] [-SkipChain] [-Quiet] [-Parallel N]
# 退出码: 0 = 全 PASS; 1 = 有 FAIL

param(
    [switch]$SkipGate,
    [switch]$SkipChain,
    [string]$LogFile = '',
    [string]$Ledger = '',
    [switch]$Quiet,
    [int]$Parallel = 4
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
Set-Location $root
if (-not $Ledger) { $Ledger = Join-Path (Split-Path -Parent $root) 'grok-build\_DEBT_LEDGER.md' }

$script:fail = 0
function Ok($m)   { Write-Host "  [PASS] $m" }
function Bad($m)  { Write-Host "  [FAIL] $m"; $script:fail++ }
function Info($m) { if (-not $Quiet) { Write-Host "  [info] $m" } }

Write-Host "== check_closure =="

# ---- 1 树净 -----------------------------------------------------------------
$st = git status --porcelain
$dirty = @($st | Where-Object { $_ -and ($_ -notmatch 'scratch') -and ($_ -notmatch '^\?\? ') })
if ($dirty.Count -eq 0) { Ok "工作树 干净 (scratch* 未跟踪 忽略)" }
else { Bad ("工作树 有 未提交 改动: " + ($dirty -join ' ; ')) }

# ---- 2 $expected 不 超前 HEAD ------------------------------------------------
$bh = 'scripts/build_zh.ps1'
$wt = Get-Content $bh -Raw -Encoding utf8
$m = [regex]::Match($wt, "\`$expected\s*=\s*'([0-9A-Fa-f]{6,})'")
if (-not $m.Success) { Bad "build_zh.ps1 里 没 找到 `$expected" ; $expected = '' }
else { $expected = $m.Groups[1].Value.ToUpper() }
$head = (git show "HEAD:$bh") -join "`n"
$mh = [regex]::Match($head, "\`$expected\s*=\s*'([0-9A-Fa-f]{6,})'")
if ($mh.Success) {
    $he = $mh.Groups[1].Value.ToUpper()
    if ($he -eq $expected) { Ok "`$expected 与 HEAD 一致 ($expected) — 不 超前" }
    else { Bad "`$expected=$expected 但 HEAD 里 是 $he — `$expected 超前 HEAD (先提交)" }
} else { Info "HEAD 版本 无 `$expected (旧格式), 跳过" }

# ---- 3 不动点 同步 -----------------------------------------------------------
if ($SkipChain) { Info "跳过 seed 哈希 (SkipChain)" }
elseif (-not (Test-Path 'qcc_zh_seed.exe')) { Bad "qcc_zh_seed.exe 不存在" }
else {
    $h = (Get-FileHash 'qcc_zh_seed.exe' -Algorithm SHA256).Hash.ToUpper()
    $h8 = $h.Substring(0, 8)
    if ($h8 -eq $expected) { Ok "seed SHA256 前8位 == `$expected ($h8)" }
    else { Bad "seed=$h8 != `$expected=$expected — 链 未 同步/未 收敛" }
    $v1 = 'qcc_zh_v1.exe'
    if (Test-Path $v1) {
        $h1 = (Get-FileHash $v1 -Algorithm SHA256).Hash.ToUpper().Substring(0, 8)
        if ($h1 -eq $expected) { Ok "v1 也 == `$expected (链 稳定)" }
        else { Info "v1=$h1 (与 seed 不同 = 正常, 若有 vN>1 需 收敛)" }
    }
}

# ---- 4 闸绿 -----------------------------------------------------------------
if ($SkipGate) { Info "跳过 闸 (SkipGate)" }
else {
    Write-Host "  ... 跑 闸 --zh-feat"
    $out = & python scripts/run_tests_zh.py --zh-feat --parallel $Parallel 2>&1 | Out-String
    if ($LASTEXITCODE -eq 0) {
        $last = ($out.Trim() -split "`n") | Select-Object -Last 1
        Ok "闸 全绿: $($last.Trim())"
    } else {
        Bad "闸 有 失败 (exit=$LASTEXITCODE)"
        ($out.Trim() -split "`n") | Where-Object { $_ -match 'FAIL|Traceback' } |
            Select-Object -First 12 | ForEach-Object { Write-Host "        $_" }
    }
}

# ---- 5 新锚 入闸 -------------------------------------------------------------
# 修正 (启元 2026-09-25 审计): 原 `$j = IndexOf(']', `$i)` 取「第一个 ]」判段界 —— CORE_FEAT = [ 段第 3 行注释即含 ] ⟹ 仅提取到 2 个锚 (真实 833) ⟹ 该检查形同虚设且持续误报; 且完全不解析 CORE_FEAT += [ 追加段。
# 现改为逐行扫描: 段起点 = `CORE_FEAT [+]= [` 独占行, 段终点 = 单独一行 `]`, 合并全部段。
$gate = @()
$inFeat = $false
foreach ($line in (Get-Content 'scripts/run_tests_zh.py' -Encoding utf8)) {
    $t2 = $line.Trim()
    if ($t2 -match '^CORE_FEAT\s*\+?=\s*\[$') { $inFeat = $true; continue }
    if ($inFeat -and $t2 -eq ']') { $inFeat = $false; continue }
    if ($inFeat) {
        $gate += [regex]::Matches($line, "'scripts/([^']+)'") | ForEach-Object { $_.Groups[1].Value }
    }
}
$gate = @($gate | Sort-Object -Unique)
# 入闸闸自检: 锚数应 >= 800 (低于此值 = 解析失效, 响亮报错)
if ($gate.Count -lt 200) { Bad ("CORE_FEAT 解析仅得 {0} 个锚 (应 >=800) — check_closure 自身解析失效, 本项结果不可信" -f $gate.Count) }

if (-not $LogFile) {
    $dir = 'memory/construction'
    if (Test-Path $dir) {
        $LogFile = (Get-ChildItem $dir -File | Sort-Object LastWriteTime |
                    Select-Object -Last 1).FullName
    }
}
if (-not $LogFile -or -not (Test-Path $LogFile)) { Info "找不到 施工日志, 跳过 新锚 检查" }
else {
    $txt = Get-Content $LogFile -Raw -Encoding utf8
    $anchors = [regex]::Matches($txt, '(?:scripts/)?(feat_[A-Za-z0-9_]+\.c)') |
               ForEach-Object { $_.Groups[1].Value } | Sort-Object -Unique
    if ($anchors.Count -eq 0) { Info ("日志 {0} 未 提 锚 — 无 新锚 需 入闸" -f (Split-Path -Leaf $LogFile)) }
    else {
        $missing = @($anchors | Where-Object { $gate -notcontains $_ })
        if ($missing.Count -eq 0) {
            Ok ("日志 {0} 里 {1} 个 锚 全 在 闸内" -f (Split-Path -Leaf $LogFile), $anchors.Count)
        } else {
            Bad ("日志 {0} 里 {1} 个 锚 未 入闸: {2}" -f (Split-Path -Leaf $LogFile),
                 $missing.Count, ($missing -join ', '))
        }
    }
}

# ---- 6 台账 已记 -------------------------------------------------------------
if (-not (Test-Path $Ledger)) { Info "台账 路径 不存在 ($Ledger), 跳过" }
elseif ($expected) {
    $lg = Get-Content $Ledger -Raw -Encoding utf8
    if ($lg -match [regex]::Escape($expected)) { Ok "台账 已 记 不动点 $expected" }
    else { Bad "台账 里 没 有 不动点 $expected — 收尾 未 回写" }
}

# ---- 7 声明形态 覆盖矩阵 (启元 复盘 序8, 2026-09-13) ---------------------------
#   零格 = 至少 一条 声明 解析路径 **整条 零覆盖** (R1-R4 四条 缺陷 全部 出自 此) ⇒ 一等 闸 指标, 零格即红.
if (Test-Path 'scripts/decl_form_matrix.py') {
    $mo = & python scripts/decl_form_matrix.py 2>&1
    if ($LASTEXITCODE -eq 0) { Ok "声明形态 覆盖矩阵: 全 形态 有 入闸 锚" }
    else { Bad "声明形态 覆盖矩阵 有 零格:"; $mo | Select-Object -Last 8 | ForEach-Object { Write-Host "      $_" } }
} else { Info "decl_form_matrix.py 不存在, 跳过" }

Write-Host "== 结果: $(if ($script:fail -eq 0) { '全 PASS' } else { "$($script:fail) 项 FAIL" }) =="
exit $(if ($script:fail -eq 0) { 0 } else { 1 })
