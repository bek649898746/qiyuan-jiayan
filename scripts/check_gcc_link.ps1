# =============================================================================
# check_gcc_link.ps1 — 闸外检查: 甲言编出的 .obj 经 gcc 链接后必须正常运行
#
# 这是 scripts\check_gcc_link.py 的等价替代（铁律 #100: 能不走 Python 就不走）
# 并在原版基础上增加一项检查:
#   [R] 重定位类型检查 — .text / .data 段不得出现 IMAGE_REL_AMD64_ADDR32
#
# ## 为什么这条检查必须在闸外
#
# 闸（run_tests_zh）只做「单文件 → 甲言编译 → 运行」。
# 而下面两类缺陷**只在 gcc/ld 链接时暴露**:
#
#   · 32 位寄存器承载 64 位指针（内联库函数 / __qcc_ginit）
#   · ADDR32 绝对重定位（字符串取址 / 函数地址 / .data 指针槽）
#
# 原因: 甲言自己链接自己时映像基址低（0x400000 量级），
#       32 位截断后恰好仍落在合法范围 ⟹ 闸永远测不出来。
#       交给 gcc/ld（基址 0x140000000、运行时 0x7ff6...）立刻非法。
#
# ## 用法
#
#   powershell -File scripts\check_gcc_link.ps1            # 用当前最新链产物
#   powershell -File scripts\check_gcc_link.ps1 <qcc.exe>  # 指定编译器
#
# 退出码: 0 = 通过 · 1 = 失败（编译/重定位/链接/运行/断言 任一）
# =============================================================================
param([string]$Qcc = '')

$ErrorActionPreference = 'Continue'

$根     = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$工作区 = Join-Path $根 'scratch_gcclink'
$MODULE = Join-Path $根 'scripts\gate_gcc_link_module.c'
$DRIVER = Join-Path $根 'scripts\gate_gcc_link_driver.c'
$期望   = @('G1=428A2F98', 'G2=0', 'G3=1', 'G4=1', 'G5=G5-OK', 'GATE-GCC-LINK OK')

if (-not $Qcc) {
    $Qcc = (7..1 | ForEach-Object { Join-Path $根 "qcc_zh_v$_.exe" } | Where-Object { Test-Path $_ } | Select-Object -First 1)
    if (-not $Qcc) { $Qcc = Join-Path $根 'qcc_x86.exe' }
}
if (-not $Qcc -or -not (Test-Path $Qcc)) {
    Write-Host '[闸外检查 gcc 链接] 找不到编译器' -ForegroundColor Red
    exit 1
}

New-Item -ItemType Directory -Force -Path $工作区 | Out-Null
$obj = Join-Path $工作区 'module.obj'
$app = Join-Path $工作区 'app.exe'
Remove-Item $obj, $app -Force -ErrorAction SilentlyContinue

$env:QCC_RT_PATH = Join-Path $根 'seed\qcc_rt.c'

Write-Host ('[闸外检查 gcc 链接] 编译器 = ' + (Split-Path $Qcc -Leaf) + '  (' + (Get-FileHash $Qcc -Algorithm SHA256).Hash.Substring(0,8) + ')')

# ---- ① 甲言 -c ----
Push-Location $工作区
$out1 = & $Qcc '-c' $MODULE '-o' $obj 2>&1
$rc1  = $LASTEXITCODE
Pop-Location
if ($rc1 -ne 0 -or -not (Test-Path $obj)) {
    Write-Host ('  ❌ ① 甲言 -c 失败 rc=' + $rc1) -ForegroundColor Red
    $out1 | Select-Object -First 6 | ForEach-Object { Write-Host ('     ' + $_) }
    exit 1
}
Write-Host ('  ✓ ① 甲言 -c  → ' + (Get-Item $obj).Length + ' 字节 .obj')

# ---- ② 重定位类型检查（新增）----
$bad = @()
$cur = ''
$rel = & objdump -r $obj 2>&1
foreach ($line in $rel) {
    if ($line -match 'RELOCATION RECORDS FOR \[(\S+)\]') { $cur = $Matches[1] }
    elseif ($line -match '^\s*[0-9a-f]{8,}\s+IMAGE_REL_AMD64_ADDR(32|64)\s+(\S+)') {
        if ($Matches[1] -eq '32') { $bad += ('[{0}] ADDR32 -> {1}' -f $cur, $Matches[2]) }
    }
}
if ($bad.Count -gt 0) {
    Write-Host ('  ❌ ② 重定位检查失败: 发现 ' + $bad.Count + ' 处 IMAGE_REL_AMD64_ADDR32') -ForegroundColor Red
    Write-Host '     32 位绝对重定位在 x86-64 PE 下无法编码 —— gcc/ld 会拒绝或截断' -ForegroundColor DarkGray
    $bad | Select-Object -First 8 | ForEach-Object { Write-Host ('     ' + $_) }
    exit 1
}
Write-Host '  ✓ ② 重定位检查 → 无 ADDR32（ADDR64 / REL32 属正确形态）'

# ---- ③ gcc 链接 ----
$out3 = & gcc $DRIVER $obj '-o' $app 2>&1
$rc3  = $LASTEXITCODE
if ($rc3 -ne 0 -or -not (Test-Path $app)) {
    Write-Host ('  ❌ ③ gcc 链接失败 rc=' + $rc3) -ForegroundColor Red
    $out3 | Select-Object -First 6 | ForEach-Object { Write-Host ('     ' + ($_ -replace '\s+',' ').Trim()) }
    exit 1
}
Write-Host '  ✓ ③ gcc 链接 → app.exe'

# ---- ④ 运行 + 断言 ----
$p = Start-Process -FilePath $app -NoNewWindow -Wait -PassThru -RedirectStandardOutput (Join-Path $工作区 'out.txt') -RedirectStandardError (Join-Path $工作区 'err.txt')
$rc4 = $p.ExitCode
$text = (Get-Content (Join-Path $工作区 'out.txt') -ErrorAction SilentlyContinue) -join "`n"
if ($rc4 -ne 0) {
    Write-Host ('  ❌ ④ 运行失败 rc=' + $rc4 + ' (0x{0:X8})' -f [uint32]$rc4) -ForegroundColor Red
    Write-Host ('     输出: ' + ($text -replace "`n", ' | ')) -ForegroundColor DarkGray
    Write-Host '     ⟹ 典型签名: 内联库函数指针 32 位截断（第 262 站 ③）' -ForegroundColor DarkGray
    exit 1
}
$miss = @($期望 | Where-Object { $text -notmatch [regex]::Escape($_) })
if ($miss.Count -gt 0) {
    Write-Host ('  ❌ ④ 输出断言失败，缺: ' + ($miss -join ', ')) -ForegroundColor Red
    Write-Host ('     实际: ' + ($text -replace "`n", ' | ')) -ForegroundColor DarkGray
    exit 1
}
Write-Host ('  ✓ ④ 运行 rc=0 · 输出断言通过 (' + $期望.Count + ' 项)')
foreach ($e in $期望) {
    $line = ($text -split "`n" | Where-Object { $_ -match [regex]::Escape($e) } | Select-Object -First 1)
    Write-Host ('     ' + $line.Trim())
}
Write-Host '  === 闸外检查 gcc 链接: PASS ===' -ForegroundColor Green
exit 0
