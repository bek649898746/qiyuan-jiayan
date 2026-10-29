# 批量验证 feat_zh_*.c 官方锚点
# 用法: .\验收_feat_zh.ps1 [-Filter <模式>] [-Max <数量>]
# 判据来源: 每个文件头的 // @EXPECTED out:... 和 // @EXPECTED exit:...
# 启元 · 审计岗 · 2026-10-28
param(
    [string]$Filter = 'feat_zh_*.c',
    [int]$Max = 0
)

$ErrorActionPreference = 'Continue'
$repo = 'C:\Users\Administrator\Desktop\qiyuan-jiayan'
$qcc = Join-Path $repo 'qcc_zh_seed.exe'
$work = Join-Path $env:TEMP ('_featrun_' + [guid]::NewGuid().ToString('N').Substring(0,8))
New-Item -ItemType Directory -Force -Path $work | Out-Null

$files = Get-ChildItem (Join-Path $repo 'scripts') -Filter $Filter | Sort-Object Name
if ($Max -gt 0) { $files = $files | Select-Object -First $Max }

$pass = 0; $fail = 0; $skip = 0
$details = @()

foreach ($f in $files) {
    $allTxt = [IO.File]::ReadAllText($f.FullName, [Text.Encoding]::UTF8)
    # 只取文件头 40 行 —— 正文注释里也可能出现 @EXPECTED（如解释性引用），会污染判据
    $headLines = ($allTxt -split "`r?`n" | Select-Object -First 40) -join "`n"
    $txt = $headLines
    # 解析期望值
    $expOut = $null; $expExit = $null; $expErr = $null
    # 注意: 一个文件可有多行 @EXPECTED out: —— 语义是「这些片段按序拼起来」
    $outs = @()
$rawOuts = [regex]::Matches($txt, '@EXPECTED\s+out:\s*(.+)') | ForEach-Object { $_.Groups[1].Value.Trim() }
# 过滤注释污染：真正的期望值不含反引号、不出现说明性中文短语
$outs = @()
foreach ($o in $rawOuts) {
    if ($o -match '`|只保留|教训|断言|不符|必红|限制|跨天|闸的|框架的|——|逐行|覆盖|认 最后|程序 内部') { continue }
    $o = ($o -replace '\s*\*/\s*$','').Trim()
    if ($o) { $outs += $o }
}
$outs += [regex]::Matches($txt, '@EXPECTED\s+run:\s*(.+)') | ForEach-Object { $_.Groups[1].Value.Trim() }
    $outs = $outs | Select-Object -Unique; if ($outs.Count -gt 0) { $expOut = ($outs -join ' ') }
    $m = [regex]::Match($txt, '@EXPECTED\s+exit:\s*(-?\d+)')
    if ($m.Success) { $expExit = [int64]$m.Groups[1].Value }
$compileFail = ($txt -match '@EXPECTED\s+compile_fail')
    $mWarn = [regex]::Match($txt, '@EXPECTED\s+warn:\s*(.+)')
    $expWarn = if ($mWarn.Success) { $mWarn.Groups[1].Value.Trim() } else { $null }
$expectSilent = ($txt -match '@EXPECTED\s+silent:')
    $knownGap = ($txt -match '@KNOWN-GAP')
    $mKnob = [regex]::Match($txt, '@KNOB\s+(\S+)')
    $knob = if ($mKnob.Success) { $mKnob.Groups[1].Value.Trim() } else { $null }
    $mIn = [regex]::Match($txt, '@EXPECTED\s+in:\s*(.+)')
    $expIn = if ($mIn.Success) { $mIn.Groups[1].Value.Trim() } else { $null }
    $exitNonzero = ($txt -match '@EXPECTED\s+exit:\s*nonzero')
    $m = [regex]::Match($txt, '@EXPECTED\s+err:\s*(A\d+)')
    if ($m.Success) { $expErr = $m.Groups[1].Value }

if ($knownGap) { $skip++; $details += "SKIP $($f.Name) [@KNOWN-GAP 显式登记已知差异]"; continue }

    $exe = Join-Path $work ($f.BaseName + '.exe')
if ($knob) { [Environment]::SetEnvironmentVariable($knob, '1') }
    $r = (& $qcc $f.FullName -o $exe 2>&1 | Out-String)
    if ($knob) { [Environment]::SetEnvironmentVariable($knob, $null) }
    $rc = $LASTEXITCODE

    if ($expErr -or $compileFail) {
        # 期望编译失败；有 err 时还要求错误码命中
        $hit = ($rc -ne 0)
        if ($hit -and $expErr) { $hit = ($r -match $expErr) }
        # compile_fail 用例的 out: 是「错误信息里包含的片段」
        if ($hit -and $expOut) { $hit = $r.Contains($expOut) }
        if ($hit) { $pass++ }
        else {
            $fail++
            $why = if ($rc -eq 0) { '实际编译通过' } else { '错误码/文案不匹配' }
            $details += "FAIL $($f.Name) [期望编译失败; $why]"
        }
        continue
    }

    if ($rc -ne 0) {
        $got = if ($r -match '(A\d{4})') { $Matches[1] } else { 'rc=' + $rc }
        if ($expExit -eq $null -and $expOut -eq $null) { $skip++; $details += "SKIP $($f.Name) [无期望, 编译失败 $got]" }
        else { $fail++; $details += "FAIL $($f.Name) [编译失败 $got]" }
        continue
    }
if (-not (Test-Path $exe)) { $skip++; $details += "SKIP $($f.Name) [无产物]"; continue }

    # warn: 编译输出里必须含该警告文本
    if ($expWarn -ne $null) {
        if (-not $r.Contains($expWarn)) { $fail++; $details += "FAIL $($f.Name) [期望警告[$expWarn]未出现]"; continue }
    }
    # silent: 期望编译安静（无 [WARN]/[警告] 输出）
    if ($expectSilent) {
        if ($r -match '\[WARN\]|\[警告\]|警告:') { $fail++; $details += "FAIL $($f.Name) [期望静默但有警告]"; continue }
    }

    $out = ((& $exe 2>&1) -join "`n")
    $runExit = $LASTEXITCODE
    $outTrim = $out.Trim()

    $okOut = $true; $okExit = $true
if ($outs.Count -gt 0) {
        $okOut = $true
        foreach ($frag in $outs) { if (-not $outTrim.Contains($frag)) { $okOut = $false; break } }
    }
    if ($exitNonzero) { $okExit = ($runExit -ne 0) }
    elseif ($expExit -ne $null) {
        $runU = if ($runExit -lt 0) { [uint32]($runExit + 4294967296) } else { [uint32]$runExit }
        $expU = if ($expExit -lt 0) { [uint32]($expExit + 4294967296) } else { [uint32]$expExit }
        $okExit = ($runU -eq $expU)
    }

    if ($okOut -and $okExit) { $pass++ }
    else {
        $fail++
        $why = @()
        if (-not $okOut) { $why += "out 期望[$expOut] 实际[$outTrim]" }
        if (-not $okExit) { $why += "exit 期望[$expExit] 实际[$runExit]" }
        $details += "FAIL $($f.Name) [$($why -join '; ')]"
    }
}

Remove-Item $work -Recurse -Force -ErrorAction SilentlyContinue

"=== 批量验收结果 ==="
"  文件数: $($files.Count)"
"  通过:   $pass"
"  失败:   $fail"
"  跳过:   $skip"
if ($files.Count -gt 0) {
    "  通过率: {0:P1}" -f ($pass / [double]$files.Count)
}
""
if ($fail -gt 0) { "=== 失败明细 ==="; $details | Where-Object { $_ -like 'FAIL*' } }
if ($skip -gt 0) { "=== 跳过明细（前 20）==="; $details | Where-Object { $_ -like 'SKIP*' } | Select-Object -First 20 }
