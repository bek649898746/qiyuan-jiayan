# ============================================================
# 甲言 · 九条语言线验证（编译 + 运行，零依赖）
# 用法: powershell -ExecutionPolicy Bypass -File scripts/verify_multilang.ps1
# 目的: 证明「用甲言写的 9 种语言实现」是真的能编译、能运行的源码，
#       不是一句宣称。每条线约 3-5 秒，全量约 40 秒。
# ============================================================
$ErrorActionPreference = 'Stop'
try { [Console]::OutputEncoding = [System.Text.Encoding]::UTF8 } catch {}
$root = Split-Path -Parent $PSScriptRoot
Set-Location $root

$qcc = Join-Path $root 'qcc_zh_seed.exe'
if (-not (Test-Path $qcc)) { Write-Host "[FAIL] 找不到 qcc_zh_seed.exe" -ForegroundColor Red; exit 1 }

$work = Join-Path $root '_ml_run'
if (Test-Path $work) { Remove-Item $work -Recurse -Force -ErrorAction SilentlyContinue }
New-Item -ItemType Directory -Force -Path $work | Out-Null

$sw = [Diagnostics.Stopwatch]::StartNew()
$script:pass = 0; $script:fail = 0

# 线 / 入口文件 / 期望输出首行片段
$cases = @(
    @{ line='JS';         dir='js';         entry='jsl1.c';  want='JS-L1' },
    @{ line='JVM';        dir='jvm';        entry='jv1.c';   want='JV-J1' },
    @{ line='Java';       dir='java';       entry='java1.c'; want='Java-A1' },
    @{ line='C++';        dir='cpp';        entry='cpp1.c';  want='CPP-C1' },
    @{ line='C#';         dir='csharp';     entry='cs1.c';   want='CS1' },
    @{ line='Go';         dir='go';         entry='go1.c';   want='GO-G1' },
    @{ line='Python';     dir='python';     entry='py1.c';   want='PY-P1' },
    @{ line='Rust';       dir='rust';       entry='rs1.c';   want='RS-R1' },
    @{ line='TypeScript'; dir='typescript'; entry='ts1.c';   want='TS-T1' }
)

Write-Host "==== 甲言 九条语言线验证 ====" -ForegroundColor Cyan
Write-Host "源: examples/multilang/ (85 文件 / 69,497 行)" -ForegroundColor DarkGray

foreach ($c in $cases) {
    $src = Join-Path $root ("examples\multilang\" + $c.dir + "\" + $c.entry)
    $exe = Join-Path $work ($c.dir + '.exe')
    if (-not (Test-Path $src)) {
        Write-Host ("  [FAIL] {0,-11} 源文件缺失: {1}" -f $c.line, $src) -ForegroundColor Red
        $script:fail++; continue
    }
    $t0 = [Diagnostics.Stopwatch]::StartNew()
    & $qcc $src -o $exe 2>&1 | Out-Null
    $rc = $LASTEXITCODE
    $t0.Stop()
    if ($rc -ne 0 -or -not (Test-Path $exe)) {
        Write-Host ("  [FAIL] {0,-11} 编译 rc={1} ({2} ms)" -f $c.line, $rc, [int]$t0.Elapsed.TotalMilliseconds) -ForegroundColor Red
        $script:fail++; continue
    }
    $out = & $exe 2>&1 | Out-String
    $first = (($out -split "`r?`n")[0])
    if ($first -match [regex]::Escape($c.want)) {
        Write-Host ("  [PASS] {0,-11} {1,5} ms  {2}" -f $c.line, [int]$t0.Elapsed.TotalMilliseconds, $first.Trim()) -ForegroundColor Green
        $script:pass++
    } else {
        Write-Host ("  [FAIL] {0,-11} 输出不含 '{1}' / 实得: {2}" -f $c.line, $c.want, $first) -ForegroundColor Red
        $script:fail++
    }
}

Remove-Item $work -Recurse -Force -ErrorAction SilentlyContinue
$sw.Stop()
$sec = [math]::Round($sw.Elapsed.TotalSeconds, 1)
Write-Host ""
if ($script:fail -eq 0) {
    Write-Host ("[PASS] 九线验证 全绿：{0}/9 条线编译并运行成功 / 耗时 {1}s" -f $script:pass, $sec) -ForegroundColor Green
    Write-Host "       这些是【甲言源码】（.c 是扩展名，内容是中文关键字），不是宣称，是可编译可运行的实物。" -ForegroundColor Green
    exit 0
} else {
    Write-Host ("[FAIL] {0} 条线失败 / {1} 条通过 / 耗时 {2}s" -f $script:fail, $script:pass, $sec) -ForegroundColor Red
    exit 1
}