<#
  _codegen_cost_gate.ps1 — 码生成「单位成本」常驻判据（D1038）
  启元 · 2026-11-03

  为什么有这个判据：
    2026-11-03 实测 码生成单位成本 0.431 → 0.799 ms/行（+85.4%），而主源仅涨 1.71%。
    功能在涨、性能在悄悄退，而当时没有任何一个判据在盯这条线。
    本脚本把「ms / 千行」钉成红线，超了就报红。

  判据：
    单位成本 = 码生成耗时(ms) / 主源行数
    <= $Threshold (默认 0.85)  ->  exit 0 (PASS)
    >  $Threshold              ->  exit 1 (FAIL)

  用法：
    powershell -NoProfile -ExecutionPolicy Bypass -File _codegen_cost_gate.ps1
    powershell -NoProfile -ExecutionPolicy Bypass -File _codegen_cost_gate.ps1 -Threshold 0.80
    powershell -NoProfile -ExecutionPolicy Bypass -File _codegen_cost_gate.ps1 -Strict -Quiet

  注意：
    * 本机 pwsh 不在 PATH，用 powershell（PS 5.1）
    * 本文件含中文 ⟹ 必须 UTF-8 with BOM
    * 跑一次约 40 秒（自编主源），不跑全量锚
    * 默认追加趋势到 <AuditDir>\_codegen_cost_trend.csv，-NoRecord 可关
#>
param(
    [double]$Threshold = 0.85,
    [string]$Repo = 'C:\Users\Administrator\Desktop\qiyuan-jiayan',
    [string]$AuditDir = 'D:\.openclaw\audit',
    [switch]$Strict,
    [switch]$NoRecord,
    [switch]$Quiet
)

$ErrorActionPreference = 'Continue'
$t0 = Get-Date
function Say([string]$m){ if(-not $Quiet){ Write-Host $m } }

Say ("== 码生成成本判据 == {0:yyyy-MM-dd HH:mm:ss}" -f $t0)

# ---------- 0 · 量具校准（先证明数据可信） ----------
$cores = (Get-CimInstance Win32_ComputerSystem).NumberOfLogicalProcessors
$s1 = @{}
Get-Process | ForEach-Object { if($_.CPU -ne $null){ $s1[$_.Id] = $_.CPU } }
Start-Sleep -Milliseconds 1500
$busy = @()
Get-Process | ForEach-Object {
    if($_.CPU -ne $null -and $s1.ContainsKey($_.Id)){
        $delta = $_.CPU - $s1[$_.Id]
        $pct = ($delta / 1.5 / $cores) * 100
        if($pct -gt 5){ $busy += ('{0} {1:N1}%' -f $_.ProcessName, $pct) }
    }
}
Say ("  量具: {0} 逻辑核" -f $cores)
if($busy.Count -gt 0){
    Say ("  [WARN] 存在重负载，计时可能偏高: " + ($busy -join ', '))
    if($Strict){ Say '  [FAIL] -Strict 且量具有负载，拒绝出数'; exit 1 }
} else {
    Say '  量具: 无显著后台负载'
}

# ---------- 1 · 取源事实 ----------
$src = Join-Path $Repo 'srclib_jiayan\qcc_zh.jy'
$cc  = Join-Path $Repo 'qcc_zh_seed.exe'
if(-not (Test-Path -LiteralPath $src)){ Say "  [FAIL] 主源不存在: $src"; exit 1 }
if(-not (Test-Path -LiteralPath $cc)){  Say "  [FAIL] 种子不存在: $cc";  exit 1 }

$lines = (Get-Content -LiteralPath $src -Encoding UTF8).Count
$mtime = (Get-Item -LiteralPath $src).LastWriteTime
$sha   = (Get-FileHash -LiteralPath $src -Algorithm SHA256).Hash.Substring(0,16)
Say ("  主源: {0:N0} 行  mtime={1:yyyy-MM-dd HH:mm:ss}  sha256[:16]={2}" -f $lines, $mtime, $sha)

# ---------- 2 · 跑 --profile（产物落 tmp，不污染工作区） ----------
$tmp = Join-Path $env:TEMP ('cgcost_' + [guid]::NewGuid().ToString('N').Substring(0,8))
New-Item -ItemType Directory -Path $tmp -Force | Out-Null
$outExe = Join-Path $tmp 'a.exe'
$errF   = Join-Path $tmp 'e.txt'
$outF   = Join-Path $tmp 'o.txt'
$env:QCC_DUMP = ''

Say '  正在自编主源（约 40 秒）...'
$sw = [Diagnostics.Stopwatch]::StartNew()
$proc = Start-Process -FilePath $cc `
    -ArgumentList @( ('"' + $src + '"'), '-o', ('"' + $outExe + '"'), '--profile' ) `
    -NoNewWindow -Wait -PassThru `
    -RedirectStandardError $errF -RedirectStandardOutput $outF `
    -WorkingDirectory $Repo
$sw.Stop()

if($proc.ExitCode -ne 0){
    Say ("  [FAIL] 编译失败 rc={0}" -f $proc.ExitCode)
    Get-Content -LiteralPath $errF -Encoding UTF8 -ErrorAction SilentlyContinue | Select-Object -First 5
    exit 1
}
if(-not (Test-Path -LiteralPath $outExe)){
    Say '  [FAIL] 产物未生成（rc=0 但无产物 —— 不可当成功）'; exit 1
}

# ---------- 3 · 解析码生成耗时 ----------
$txtLines = Get-Content -LiteralPath $errF -Encoding UTF8 -ErrorAction SilentlyContinue
$cgMs = 0; $cgPct = 0; $wallMs = [int]$sw.ElapsedMilliseconds
foreach($l in $txtLines){
    if($l -match '码生成\s+(\d+)\s+(\d+)%'){ $cgMs = [int]$Matches[1]; $cgPct = [int]$Matches[2]; break }
}
if($cgMs -le 0){
    Say '  [FAIL] 未从 --profile 解析到「码生成」耗时（判据无法出数）'
    exit 1
}

$cost = $cgMs / $lines          # ms / 行
$costK = $cost * 1000           # ms / 千行
$verdict = if($cost -le $Threshold){ 'PASS' } else { 'FAIL' }

Say ''
Say '  ---- 结果 ----'
Say ("  码生成      : {0,7:N0} ms  (占 合计 {1}%)" -f $cgMs, $cgPct)
Say ("  墙钟        : {0,7:N0} ms" -f $wallMs)
Say ("  主源        : {0,7:N0} 行" -f $lines)
Say ("  单位成本    : {0,7:N3} ms/行   = {1,7:N1} ms/千行" -f $cost, $costK)
Say ("  红线        : {0,7:N3} ms/行" -f $Threshold)
Say ("  判定        : [{0}]" -f $verdict)
Say ''

# ---------- 4 · 趋势留底 ----------
if(-not $NoRecord){
    $csv = Join-Path $AuditDir '_codegen_cost_trend.csv'
    $row = ('{0},{1},{2},{3},{4},{5},{6},{7}' -f `
        $t0.ToString('yyyy-MM-dd HH:mm:ss'), $sha, $lines, $cgMs, $wallMs, [math]::Round($cost,4), [math]::Round($costK,1), $verdict)
    if(-not (Test-Path -LiteralPath $csv)){
        [IO.File]::AppendAllText($csv, "time,src_sha16,src_lines,codegen_ms,wall_ms,ms_per_line,ms_per_kline,verdict`r`n", (New-Object System.Text.UTF8Encoding($false)))
    }
    [IO.File]::AppendAllText($csv, ($row + "`r`n"), (New-Object System.Text.UTF8Encoding($false)))
    Say ("  趋势已记录: {0}" -f $csv)

    # 顺带报最近 5 条趋势（看得见就往哪走）
    $hist = Get-Content -LiteralPath $csv -Encoding UTF8 -ErrorAction SilentlyContinue
    if($hist.Count -gt 1){
        Say '  最近记录:'
        $hist | Select-Object -Last 5 | ForEach-Object { Say ('    ' + $_) }
    }
}

if($verdict -eq 'PASS'){ exit 0 } else { exit 1 }
