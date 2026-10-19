# ============================================================
# 甲言 · 90 秒自证（零依赖：不需要 gcc，不需要 Python，不需要联网）
# 用法: powershell -ExecutionPolicy Bypass -File scripts/verify_90s.ps1
# 目的: 让任何人（含 AI agent）在 90 秒内独立确认「这个编译器真的能用」，
#       而不必跑 20 分钟的完整自举。失败时 exit 1。
# ============================================================
$ErrorActionPreference = 'Stop'
try { [Console]::OutputEncoding = [System.Text.Encoding]::UTF8 } catch {}
$root = Split-Path -Parent $PSScriptRoot
Set-Location $root

$qcc = Join-Path $root 'qcc_zh_seed.exe'
if (-not (Test-Path $qcc)) { Write-Host "[FAIL] 找不到 qcc_zh_seed.exe —— 请确认在仓库根运行" -ForegroundColor Red; exit 1 }

$work = Join-Path $root '_90s_run'
if (Test-Path $work) { Remove-Item $work -Recurse -Force -ErrorAction SilentlyContinue }
New-Item -ItemType Directory -Force -Path $work | Out-Null

$sw = [Diagnostics.Stopwatch]::StartNew()
$script:pass = 0; $script:fail = 0
function Chk($name, $ok, $detail) {
    if ($ok) { Write-Host ("  [PASS] " + $name) -ForegroundColor Green; $script:pass++ }
    else     { Write-Host ("  [FAIL] " + $name + "  " + $detail) -ForegroundColor Red; $script:fail++ }
}
function W($p, $s) { [System.IO.File]::WriteAllText($p, $s, (New-Object System.Text.UTF8Encoding($false))) }

Write-Host "==== 甲言 90 秒自证（零依赖） ====" -ForegroundColor Cyan

# ---- 关 1: 编译器自检 ----
Write-Host "[1/5] 编译器自检 ..." -ForegroundColor Yellow
$o = & $qcc --test 2>&1 | Out-String
Chk "selftest 通过" ($o -match 'selftest\s*(通过|PASS)') $o.Trim()

# ---- 关 2: 官方示例 编译 + 运行 ----
Write-Host "[2/5] 编译并运行 examples/成绩.jy ..." -ForegroundColor Yellow
$ex = Join-Path $work 'cj.exe'
& $qcc (Join-Path $root 'examples\成绩.jy') -o $ex 2>&1 | Out-Null
$rc = $LASTEXITCODE
$out = if (Test-Path $ex) { (& $ex 2>&1 | Out-String) } else { '' }
Chk "编译 rc=0" ($rc -eq 0) "rc=$rc"
Chk "运行输出含【最高分: 100】" ($out -match '最高分:\s*100') $out.Trim()
Chk "运行输出含【及格数: 4/5】" ($out -match '及格数:\s*4/5') $out.Trim()

# ---- 关 3: 中文关键字覆盖（整/函数/空/遍/若/否则/输出/返）----
Write-Host "[3/5] 中文关键字覆盖 ..." -ForegroundColor Yellow
$src = Join-Path $work 'kw.jy'
$code = "函数 整 平方(整 甲) {`r`n    返 甲 * 甲;`r`n}`r`n`r`n函数 整 主(空) {`r`n    整 和 = 0;`r`n    遍 (整 我 = 1; 我 <= 5; 我++) {`r`n        和 = 和 + 平方(我);`r`n    }`r`n    若 (和 == 55) {`r`n        输出(`"ZH-KW OK 55\n`");`r`n    } 否则 {`r`n        输出(`"ZH-KW FAIL %d\n`", 和);`r`n    }`r`n    返 0;`r`n}`r`n"
W $src $code
$kw = Join-Path $work 'kw.exe'
& $qcc $src -o $kw 2>&1 | Out-Null
$rc2 = $LASTEXITCODE
$out2 = if (Test-Path $kw) { (& $kw 2>&1 | Out-String) } else { '' }
Chk "中文关键字编译 rc=0" ($rc2 -eq 0) "rc=$rc2"
Chk "运行输出【ZH-KW OK 55】" ($out2 -match 'ZH-KW OK 55') $out2.Trim()

# ---- 关 4: -graph 代码知识图谱（AI 接口 1）----
Write-Host "[4/5] -graph 代码知识图谱 ..." -ForegroundColor Yellow
$gj = Join-Path $work 'g.json'
$gk = Join-Path $work 'kw2.exe'
& $qcc -graph $gj $src -o $gk 2>&1 | Out-Null
$gok = $false; $gmsg = 'no json'
if (Test-Path $gj) {
    try {
        $j = Get-Content $gj -Raw -Encoding UTF8 | ConvertFrom-Json
        if ($j.relations.Count -gt 0) { $gok = $true; $gmsg = "relations=$($j.relations.Count)" }
        else { $gmsg = 'relations=0' }
    } catch { $gmsg = "parse: $($_.Exception.Message)" }
}
Chk "-graph 导出可解析且 relations>0" $gok $gmsg

# ---- 关 5: --dj 结构化诊断（AI 接口 2）----
Write-Host "[5/5] --dj 结构化诊断 ..." -ForegroundColor Yellow
$bad = Join-Path $work 'bad.jy'
W $bad "函数 整 主(空) { 返 未声明变量; }`r`n"
$dj = (& $qcc --dj $bad -o (Join-Path $work 'bad.exe') 2>&1 | Out-String)
$djok = $false; $djmsg = $dj.Trim()
if ($dj -match '\{"diagnostics"') {
    try {
        $d = $dj | ConvertFrom-Json
        $e = @($d.diagnostics)[0]
        if ($e.code -and $e.message -and $e.fixes) { $djok = $true; $djmsg = "code=$($e.code) fixes=$($e.fixes.Count) codeLine=$(if($e.codeLine){'有'}else{'无'})" }
        else { $djmsg = 'JSON 缺 code/message/fixes' }
    } catch { $djmsg = "parse: $($_.Exception.Message)" }
}
Chk "--dj 输出含 code/message/fixes" $djok $djmsg

# ---- 收尾 ----
Remove-Item $work -Recurse -Force -ErrorAction SilentlyContinue
$sw.Stop()
$sec = [math]::Round($sw.Elapsed.TotalSeconds, 1)
Write-Host ""
if ($script:fail -eq 0) {
    Write-Host ("[PASS] 90 秒自证 全绿：$($script:pass) 项通过 / 0 失败 / 耗时 ${sec}s") -ForegroundColor Green
    Write-Host "       编译器可用、中文关键字可用、AI 接口可用 —— 以上全部零依赖、零联网。" -ForegroundColor Green
    exit 0
} else {
    Write-Host ("[FAIL] $($script:fail) 项失败 / $($script:pass) 项通过 / 耗时 ${sec}s") -ForegroundColor Red
    exit 1
}