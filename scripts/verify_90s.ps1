# ============================================================
# 甲言 · 90 秒自证（零依赖：不需要 gcc，不需要 Python，不需要联网）
# 用法: powershell -ExecutionPolicy Bypass -File scripts/verify_90s.ps1
# 目的: 让任何人（含 AI agent）在 90 秒内独立确认「这个编译器真的能用」，
#       而不必跑 20 分钟的完整自举。失败时 exit 1。
# ============================================================
# 2026-10-21 启元（大同步验证实测）：Stop -> Continue。
#   原因：新编译器（种子 7B5D3CFB）把 `[graph]` 进度信息改写到 **stderr**
#         （旧种子 39C6FC81 写在 stdout —— 新行为更正确：进度不污染 stdout）。
#         而 `$ErrorActionPreference='Stop'` 配合 `2>&1` 会把这类 stderr 当成致命错误
#         直接终止脚本，实测 90 秒自证从 8/8 全绿变成 3 项 FAIL（卡在关4）。
#   本脚本本来就逐关检查退出码与输出内容（Chk / Test-Path / JSON 解析），不依赖 Stop。
$ErrorActionPreference = 'Continue'
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
# ==== D43 绕行（2026-10-22 启元审计 · CI run #179 zh_chain 唯一红线）====
#   GitHub windows-latest runner 的 ANSI 代码页 = 1252，中文文件名经 argv 传给
#   编译器会退化成 '??.jy' ⟹ 编译器 fopen 打不开（本源另一处 ANSI 通道，见施工单修法 B）。
#   这里用 .NET（Unicode 安全）先把示例复制成 ASCII 名，再编译 ——
#   **内容与语义完全不变**，只避开 argv 的 ANSI 通道。开发机（936）不受影响。
$srcAscii = Join-Path $work 'score.jy'
Copy-Item -LiteralPath (Join-Path $root 'examples\成绩.jy') -Destination $srcAscii -Force
if (-not (Test-Path -LiteralPath $srcAscii)) { Write-Host "[FAIL] D43 前置：示例复制成 ASCII 名失败" -ForegroundColor Red; $script:fail++ }
& $qcc $srcAscii -o $ex 2>&1 | Out-Null
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
# 2026-10-21 启元：新编译器（种子 7B5D3CFB）把 `--dj` 的 JSON 写到 **stderr**
#   （旧种子 39C6FC81 写 stdout —— 两者都是「纯 JSON、无前缀」，实测 stderr 328 字符、
#    { } 各 6 个配对）。而 PowerShell 的 `2>file` 重定向会给 stderr 加 `程序名 : ` 前缀，
#   正则截取又会被 JSON 内换行截断（实测两种都失败过）。
#   ⟹ 用 cmd 重定向拿**原始流**，再「stdout 优先、stderr 兜底」选一个解析。新旧行为都兼容。
$djOutFile = Join-Path $work 'dj_out.txt'
$djErrFile = Join-Path $work 'dj_err.txt'
$badExe    = Join-Path $work 'bad.exe'
$null = cmd /c ('"{0}" --dj "{1}" -o "{2}" >"{3}" 2>"{4}"' -f $qcc, $bad, $badExe, $djOutFile, $djErrFile)
$djOut = (Get-Content $djOutFile -Raw -Encoding UTF8 -ErrorAction SilentlyContinue)
$djErr = (Get-Content $djErrFile -Raw -Encoding UTF8 -ErrorAction SilentlyContinue)
if ($djOut -and $djOut.Trim().Length -gt 0) { $dj = $djOut } else { $dj = $djErr }
if (-not $dj) { $dj = '' }
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