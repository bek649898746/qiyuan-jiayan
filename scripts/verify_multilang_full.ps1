# ============================================================
# 甲言 · 九线全量编译验证（85 个 .c 逐个编译，双向判据）
#
# 用法: powershell -ExecutionPolicy Bypass -File scripts/verify_multilang_full.ps1
#
# 为什么需要它:
#   verify_multilang.ps1 只编 9 个入口（每条线一个），回答的是「这条线能不能跑」。
#   它回答不了「85 个归档文件里有几个真能独立编译」——
#   2026-10-21 实测答案是 72/85，不是 85/85。
#
# 双向判据（缺一即错）:
#   ① 72 个正例  → 必须 rc=0 且产出 exe   （防退化）
#   ② 13 个非通过 → 必须 rc≠0            （防误放：它们本来就该失败）
#
# 13 个的构成（逐条说明，不是"允许失败"而是"期望失败"）:
#   故意负例 4 个 —— 验证关键字保护 / 未声明检测，报错才是正确行为:
#     java/kw1.c                    `跳 = 1;`    关键字不能作赋值目标
#     typescript/对照1_简体未声明.c   `甲乙丙 = 1;` 未声明标识符
#     typescript/对照2_英文未声明.c   `zzz = 1;`   未声明标识符
#     typescript/繁体.c              `出錯了 = 1;` 未声明标识符
#   历史旧版 9 个 —— 自身含已被后续文件修复的 bug，保留作留痕:
#     java/min{2,3,4,5,6,11,15}.c   用关键字 `跳` 作变量名（java2.c 已改 `该跳`）
#     csharp/cs9.c                  用类型词 `短` 作形参名
#     js/jsl7.c                     `串长` 撞甲言内建名（jsl8/9.c 已改 `串长一`）
#
# 判据来源: D:\.openclaw\audit\_SETTLED.md 2026-10-21 段 · _full_compile_probe.csv
# ============================================================
# 2026-10-21 启元：Stop -> Continue（大同步适配）。
#   本脚本的核心判据就是「13 个用例必须 rc≠0」⟹ 它**必须容忍编译失败**。
#   而新编译器（种子 7B5D3CFB）把 [ERR] 写到 **stderr**，`2>&1` 在 Stop 模式下会抛
#   NativeCommandError 直接终止脚本（实测第 77 行崩在 csharp/cs9.c 的 A9001）。
#   旧种子（39C6FC81）把错误写 stdout，所以这个缺陷从写出来那天就一直潜伏。
$ErrorActionPreference = 'Continue'
try { [Console]::OutputEncoding = [System.Text.Encoding]::UTF8 } catch {}

$root = Split-Path -Parent $PSScriptRoot
Set-Location $root

$qcc = Join-Path $root 'qcc_zh_seed.exe'
if (-not (Test-Path $qcc)) { Write-Host "[FAIL] 找不到 qcc_zh_seed.exe" -ForegroundColor Red; exit 1 }

$srcRoot = Join-Path $root 'examples\multilang'
if (-not (Test-Path $srcRoot)) { Write-Host "[FAIL] 找不到 examples/multilang" -ForegroundColor Red; exit 1 }

$work = Join-Path $root '_ml_full'
if (Test-Path $work) { Remove-Item $work -Recurse -Force -ErrorAction SilentlyContinue }
New-Item -ItemType Directory -Force -Path $work | Out-Null

# ---- 期望非通过的 13 个（正斜杠归一，便于比对）----
$expectFail = @{
    'java/kw1.c'                    = '负例: 关键字作赋值目标'
    'typescript/对照1_简体未声明.c'  = '负例: 未声明标识符'
    'typescript/对照2_英文未声明.c'  = '负例: 未声明标识符'
    'typescript/繁体.c'             = '负例: 未声明标识符'
    'java/min2.c'                   = '历史旧版: 关键字跳作变量名'
    'java/min3.c'                   = '历史旧版: 关键字跳作变量名'
    'java/min4.c'                   = '历史旧版: 关键字跳作变量名'
    'java/min5.c'                   = '历史旧版: 关键字跳作变量名'
    'java/min6.c'                   = '历史旧版: 关键字跳作变量名'
    'java/min11.c'                  = '历史旧版: 关键字跳作变量名'
    'java/min15.c'                  = '历史旧版: 关键字跳作变量名'
    'csharp/cs9.c'                  = '历史旧版: 类型词短作形参名'
    'js/jsl7.c'                     = '历史旧版: 串长撞内建名'
}

$files = @(Get-ChildItem $srcRoot -Recurse -Filter '*.c' -File | Sort-Object FullName)
Write-Host "==== 甲言 九线全量编译验证 ====" -ForegroundColor Cyan
Write-Host ("源: examples/multilang/ ({0} 个 .c / 期望 {1} 正例 + {2} 非通过)" -f $files.Count, ($files.Count - $expectFail.Count), $expectFail.Count) -ForegroundColor DarkGray

$sw = [Diagnostics.Stopwatch]::StartNew()
$okPass = 0       # 正例编译成功
$okFail = 0       # 预期失败确实失败
$badPass = @()    # 正例退化（应过没过）
$badFail = @()    # 预期失败却通过（误放）
$unknown = @()    # 不在期望表里却失败

foreach ($f in $files) {
    $rel = $f.FullName.Substring($srcRoot.Length + 1).Replace('\', '/')
    $exe = Join-Path $work 'out.exe'
    if (Test-Path $exe) { Remove-Item $exe -Force -ErrorAction SilentlyContinue }

    $out = & $qcc $f.FullName -o $exe 2>&1 | Out-String
    $rc = $LASTEXITCODE
    $produced = Test-Path $exe

    $expectThisFail = $expectFail.ContainsKey($rel)

    if ($expectThisFail) {
        if ($rc -ne 0) {
            $okFail++
        } else {
            $badFail += "$rel  [rc=0，但期望失败 — 误放]"
        }
    } else {
        if ($rc -eq 0 -and $produced) {
            $okPass++
        } elseif ($rc -ne 0) {
            $badPass += "$rel  [rc=$rc，期望通过]"
        } else {
            $unknown += "$rel  [rc=0 但未产出 exe]"
        }
    }
}

Remove-Item $work -Recurse -Force -ErrorAction SilentlyContinue
$sw.Stop()
$sec = [math]::Round($sw.Elapsed.TotalSeconds, 1)

$fail = ($badPass.Count + $badFail.Count + $unknown.Count)

Write-Host ""
Write-Host ("  正例通过        : {0}/{1}" -f $okPass, ($files.Count - $expectFail.Count)) -ForegroundColor $(if ($badPass.Count -eq 0) { 'Green' } else { 'Red' })
Write-Host ("  期望失败确认    : {0}/{1}" -f $okFail, $expectFail.Count) -ForegroundColor $(if ($badFail.Count -eq 0) { 'Green' } else { 'Red' })
Write-Host ("  耗时            : {0}s" -f $sec) -ForegroundColor DarkGray

if ($fail -eq 0) {
    Write-Host ""
    Write-Host ("[PASS] 全量编译 双向判据全绿：{0} 个正例全部通过 · {1} 个期望失败全部确认" -f $okPass, $okFail) -ForegroundColor Green
    Write-Host "       其中 4 个是故意负例（关键字保护/未声明检测），9 个是保留留痕的历史旧版。" -ForegroundColor DarkGray
    exit 0
}

Write-Host ""
Write-Host "[FAIL] 全量编译 出现 $fail 项偏差 ——" -ForegroundColor Red
if ($badPass.Count -gt 0) {
    Write-Host "  ▼ 正例退化（应过没过）:" -ForegroundColor Red
    $badPass | ForEach-Object { Write-Host "     $_" -ForegroundColor Red }
}
if ($badFail.Count -gt 0) {
    Write-Host "  ▼ 期望失败却通过（负例被误放 = 保护失效）:" -ForegroundColor Red
    $badFail | ForEach-Object { Write-Host "     $_" -ForegroundColor Red }
}
if ($unknown.Count -gt 0) {
    Write-Host "  ▼ 异常（rc=0 但无产物）:" -ForegroundColor Yellow
    $unknown | ForEach-Object { Write-Host "     $_" -ForegroundColor Yellow }
}
exit 1
