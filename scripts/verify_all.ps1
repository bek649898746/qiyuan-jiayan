# ============================================================
# 甲言 · 全量校验（一键守门）—— 2026-10-21 重写：从英文链切到中文链
#
# 用法:
#   powershell -ExecutionPolicy Bypass -File scripts/verify_all.ps1            # 快速档 ~5 分钟
#   powershell -ExecutionPolicy Bypass -File scripts/verify_all.ps1 -Full      # 加完整自举 ~25 分钟
#   powershell -ExecutionPolicy Bypass -File scripts/verify_all.ps1 -Legacy    # 加英文链（历史信任根）
#   powershell -ExecutionPolicy Bypass -File scripts/verify_all.ps1 -NoTest    # 跳过测试类
#
# ── 本次重写的原因（2026-10-21 审计发现）──────────────────────────
# 原版第 2/3/4/5 步全是英文链：
#     gcc srclib/qcc_x86.c -o qcc_x86.exe
#     qcc_x86.exe srclib_jiayan/qcc_work.jy -o qcc_boot.exe
#     qcc_boot.exe qcc_work.jy -o v1.exe ... v1..v3 全等
# 而英文链 2026-09-06 就停更了（qcc_x86.c 22,600 行 vs 中文主源 34,335 行），
# `qcc_work.jy` 是废弃镜像（中文仅 5.8%）。脚本自己都写着「build.ps1 的 expected 滞后 44 天」。
#
# ⟹ 一个叫「全量校验」的脚本验的是死链，是**对外入口级的误导**。
# 现改为：默认走中文主链；英文链降为 -Legacy 可选（它仍有价值 —— 证明历史信任根还能编）。
#
# ── 设计原则 ──────────────────────────────────────────────
#   ① 【不依赖写死的不动点】判据 = 自举链多代全等。
#      源码每次合法修改哈希都会变，写死 expected 必然过时。
#   ② 【先查编码】本项目发生过两次编码事故：
#      · 2026-08-23 UTF-8↔GBK 双重编码（中文变乱码）
#      · 2026-10-15 edit 工具剥掉 BOM（PS5.1 按 GBK 读 ⟹ 脚本解析失败）
#   ③ 【失败即停且给下一步】每条失败都打印可复制的修复提示。
#   ④ 【不重复实现】本脚本只做**编排**，各步骤的真实逻辑在专门脚本里。
# ============================================================
param(
    [switch]$Full,     # 加完整自举链（build_zh.ps1，约 20.8 分钟）
    [switch]$Legacy,   # 加英文链验证（历史信任根，已停更）
    [switch]$NoTest    # 跳过中文特性锚（CI 里另有独立步骤时可关）
)

$ErrorActionPreference = 'Stop'
try { [Console]::OutputEncoding = [System.Text.Encoding]::UTF8 } catch {}
$root = Split-Path -Parent $PSScriptRoot
Set-Location $root

$script:Fail = 0
$script:Warn = 0
function Step($n, $msg) { Write-Host "`n[$n] $msg" -ForegroundColor Cyan }
function Ok($msg)   { Write-Host "  [OK]   $msg" -ForegroundColor Green }
function Bad($msg)  { Write-Host "  [FAIL] $msg" -ForegroundColor Red; $script:Fail++ }
function Warn($msg) { Write-Host "  [WARN] $msg" -ForegroundColor Yellow; $script:Warn++ }

Write-Host "==== 甲言 · 全量校验（中文主链）====" -ForegroundColor Cyan
Write-Host "根目录: $root"
Write-Host ("档位  : {0}{1}" -f $(if ($Full) { 'FULL（含完整自举 ~25min）' } else { 'FAST（~5min）' }), $(if ($Legacy) { ' + Legacy英文链' } else { '' }))

# ------------------------------------------------------------
Step '1/7' '编码闸 —— .ps1 必须 UTF-8 with BOM（否则 PS5.1 按 GBK 读 ⟹ 中文乱码 ⟹ 语法错）'
$ps1s = @(Get-ChildItem $root -Recurse -File -Include '*.ps1' -ErrorAction SilentlyContinue |
        Where-Object { $_.FullName -notmatch '\\\.git\\' -and $_.FullName -notmatch '\\qiyuan_archive' })
$noBom = @()
foreach ($f in $ps1s) {
    $b = [System.IO.File]::ReadAllBytes($f.FullName)
    $hasBom = ($b.Length -ge 3 -and $b[0] -eq 0xEF -and $b[1] -eq 0xBB -and $b[2] -eq 0xBF)
    if (-not $hasBom) { $noBom += $f.FullName.Substring($root.Length + 1) }
}
if ($noBom.Count -eq 0) {
    Ok "$($ps1s.Count) 个 .ps1 全部含 UTF-8 BOM"
} else {
    $noBom | ForEach-Object { Bad "缺 UTF-8 BOM: $_" }
    Write-Host '         修复（保 BOM 的安全写法 —— 不要用 New-Object Type($arg)，那不是构造语法）:' -ForegroundColor Yellow
    Write-Host '         $b=[IO.File]::ReadAllBytes($p); $n=New-Object byte[] ($b.Length+3); $n[0]=0xEF;$n[1]=0xBB;$n[2]=0xBF; [Array]::Copy($b,0,$n,3,$b.Length); [IO.File]::WriteAllBytes($p,$n)' -ForegroundColor Yellow
}

Step '1b' '编码闸 —— 源码严格 UTF-8（U+FFFD 计数；负例 *_neg.c 豁免）'
$srcs = @(Get-ChildItem $root -Recurse -File -Include '*.c','*.h','*.jy' -ErrorAction SilentlyContinue |
        Where-Object { $_.FullName -notmatch '\\\.git\\' -and $_.FullName -notmatch '\\qiyuan_archive' -and $_.Name -notmatch '_neg\.c$' })
$badEnc = @()
foreach ($f in $srcs) {
    $txt = [System.Text.Encoding]::UTF8.GetString([System.IO.File]::ReadAllBytes($f.FullName))
    $n = ([regex]::Matches($txt, "\uFFFD")).Count
    if ($n -gt 0) { $badEnc += "$($f.FullName.Substring($root.Length + 1)) ($n 处)" }
}
if ($badEnc.Count -eq 0) {
    Ok "$($srcs.Count) 个源文件均为严格 UTF-8"
} else {
    $badEnc | Select-Object -First 8 | ForEach-Object { Warn "历史乱码（已知债务，不影响编译）: $_" }
    if ($badEnc.Count -gt 8) { Warn "另有 $($badEnc.Count - 8) 个未列出" }
    Warn "共 $($badEnc.Count) 个文件含乱码 —— 主源 qcc_zh.jy 的 214 处是专章（204 注释内 → 丢失 / 10 在 -bin 分支字符串）"
}

# ------------------------------------------------------------
Step '2/7' '中文链 90 秒自证（8 项 · 零依赖 · 不需要 gcc）'
if (-not (Test-Path 'scripts\verify_90s.ps1')) {
    Bad '缺 scripts\verify_90s.ps1'
} else {
    powershell -NoProfile -ExecutionPolicy Bypass -File scripts\verify_90s.ps1
    if ($LASTEXITCODE -eq 0) { Ok '90 秒自证 8 项全过' } else { Bad "90 秒自证未通过（exit=$LASTEXITCODE）" }
}

Step '3/7' '九条语言线 · 入口验证（每线 1 个入口编译并运行）'
if (-not (Test-Path 'scripts\verify_multilang.ps1')) {
    Bad '缺 scripts\verify_multilang.ps1'
} else {
    powershell -NoProfile -ExecutionPolicy Bypass -File scripts\verify_multilang.ps1
    if ($LASTEXITCODE -eq 0) { Ok '九线入口 9/9' } else { Bad "九线入口未通过（exit=$LASTEXITCODE）" }
}

Step '4/7' '九条语言线 · 全量编译（85 个 .c · 双向判据：72 正例过 + 13 期望失败确认）'
if (-not (Test-Path 'scripts\verify_multilang_full.ps1')) {
    Bad '缺 scripts\verify_multilang_full.ps1'
} else {
    powershell -NoProfile -ExecutionPolicy Bypass -File scripts\verify_multilang_full.ps1
    if ($LASTEXITCODE -eq 0) { Ok '全量编译 72/72 + 13/13' } else { Bad "全量编译出现偏差（exit=$LASTEXITCODE）" }
}

# ------------------------------------------------------------
Step '5/7' '中文特性锚（run_tests_zh.py --zh-feat · CORE_FEAT 白名单）'
if ($NoTest) {
    Warn '-NoTest 指定，跳过'
} elseif (-not (Get-Command python -ErrorAction SilentlyContinue)) {
    Warn '未找到 python —— 特性锚需 Python 3.x（见 README「依赖」）；纯编译/自证不需要它'
} else {
    $rt = python scripts/run_tests_zh.py --zh-feat 2>&1 | Out-String
    if ($LASTEXITCODE -eq 0) { Ok '中文特性锚全绿' } else { Bad "中文特性锚失败:`n$rt" }
}

# ------------------------------------------------------------
Step '6/7' '完整自举链（v1 == v2 == $expected · 约 20.8 分钟）'
if (-not $Full) {
    Warn '未指定 -Full，跳过（对外发布前请务必跑一次）'
    Write-Host '         命令: powershell -ExecutionPolicy Bypass -File scripts/build_zh.ps1' -ForegroundColor DarkGray
} elseif (-not (Test-Path 'scripts\build_zh.ps1')) {
    Bad '缺 scripts\build_zh.ps1'
} else {
    Write-Host '         （这一步最久，中途不要中断）' -ForegroundColor DarkGray
    powershell -NoProfile -ExecutionPolicy Bypass -File scripts\build_zh.ps1
    if ($LASTEXITCODE -eq 0) { Ok '完整自举链收敛' } else { Bad "完整自举链未收敛（exit=$LASTEXITCODE）" }
}

# ------------------------------------------------------------
Step '7/7' '英文链（历史信任根 · 已停更 2026-09-06）'
if (-not $Legacy) {
    Write-Host '         默认跳过 —— 英文链只作历史信任根保留，不是主链。加 -Legacy 可验。' -ForegroundColor DarkGray
    Write-Host '         为什么默认跳过: qcc_x86.c 停在 22,600 行, qcc_work.jy 是废弃镜像(中文仅 5.8%)' -ForegroundColor DarkGray
} else {
    if (-not (Get-Command gcc -ErrorAction SilentlyContinue)) {
        Warn '未找到 gcc —— 英文链需要 gcc 编宿主'
    } else {
        gcc -O2 -Wall -Werror srclib/qcc_x86.c -o qcc_x86.exe
        if ($LASTEXITCODE -ne 0 -or -not (Test-Path .\qcc_x86.exe)) {
            Bad '英文链宿主编译失败'
        } else {
            Ok "qcc_x86.exe 生成（$((Get-Item .\qcc_x86.exe).Length) 字节）"
            $t = & .\qcc_x86.exe --test 2>&1 | Out-String
            if ($t -match 'PASS') { Ok '英文链宿主自检 PASS' } else { Bad "英文链宿主自检未通过: $t" }
            if (-not (Test-Path .\qcc_work.exe)) {
                & .\qcc_x86.exe srclib_jiayan\qcc_work.jy -o qcc_work.exe 2>&1 | Out-Null
                if (Test-Path .\qcc_work.exe) { Ok "qcc_work 镜像编译成功（$((Get-Item .\qcc_work.exe).Length) 字节）" }
                else { Bad 'qcc_work 镜像编译失败' }
            }
        }
    }
}

# ------------------------------------------------------------
Write-Host ''
Write-Host '==== 结果 ====' -ForegroundColor Cyan
Write-Host "  FAIL=$script:Fail  WARN=$script:Warn"
if ($script:Fail -eq 0) {
    Write-Host '  [OK] 全量校验通过（中文主链）。' -ForegroundColor Green
    if (-not $Full) { Write-Host '       提示：发布前请再跑一次 -Full 验完整自举。' -ForegroundColor DarkGray }
    exit 0
} else {
    Write-Host '  [FAIL] 校验未通过 —— 上面每条 [FAIL] 都附了修复提示。' -ForegroundColor Red
    exit 1
}
