# =============================================================================
#  build_rt.ps1 — 产出甲言运行库目标文件（qcc_rt.obj）
#
#  为什么需要：
#    甲言 -c 编译出的 .obj，除了用户函数外，还引用一组【运行时符号】
#    （__qcc_div_check / __qcc_div_check64 / _scanf_rt / __qcc_heap_counter / …）。
#    这些符号的定义在 seed\qcc_rt.c 里 —— 但甲言【不会】自动把它塞进 .obj
#    （单文件模式是源码级 prepend；-c 模式不做）。
#
#    ⟹ 外部链接甲言模块时，必须【一并链接本运行库】：
#
#        gcc your_driver.c your_module.obj  <repo>\lib\qcc_rt.obj  -lm  -o app.exe
#
#  用法：
#    powershell -File scripts\build_rt.ps1
#    powershell -File scripts\build_rt.ps1 <qcc.exe>      # 指定编译器
#
#  退出码: 0 = 成功 · 1 = 失败
# =============================================================================
param([string]$Qcc = '')

$ErrorActionPreference = 'Continue'

$root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$rtc  = Join-Path $root 'seed\qcc_rt.c'
$lib  = Join-Path $root 'lib'
$obj  = Join-Path $lib 'qcc_rt.obj'

if (-not $Qcc) {
    $Qcc = (7..1 | ForEach-Object { Join-Path $root "qcc_zh_v$_.exe" } | Where-Object { Test-Path $_ } | Select-Object -First 1)
    if (-not $Qcc) { $Qcc = Join-Path $root 'qcc_zh_seed.exe' }
}
if (-not (Test-Path $Qcc)) { Write-Host "[FAIL] 找不到编译器: $Qcc" -ForegroundColor Red; exit 1 }
if (-not (Test-Path $rtc)) { Write-Host "[FAIL] 找不到运行库源: $rtc" -ForegroundColor Red; exit 1 }

New-Item -ItemType Directory -Force -Path $lib | Out-Null
$env:QCC_RT_PATH = $rtc

Write-Host ''
Write-Host '=============================================='
Write-Host ' build_rt — 甲言运行库目标文件'
Write-Host '=============================================='
Write-Host (" 编译器 : {0}  ({1})" -f (Split-Path $Qcc -Leaf), (Get-FileHash $Qcc -Algorithm SHA256).Hash.Substring(0,8))
Write-Host (" 源     : $rtc")
Write-Host (" 产物   : $obj")
Write-Host ''

$out = & $Qcc '-c' $rtc '-o' $obj 2>&1
$rc  = $LASTEXITCODE
if ($rc -ne 0 -or -not (Test-Path $obj)) {
    Write-Host "[FAIL] 编译运行库失败 rc=$rc" -ForegroundColor Red
    $out | Select-Object -First 8 | ForEach-Object { Write-Host ('   ' + $_) }
    exit 1
}
Write-Host ("[OK] 产出 {0}  ({1:N0} B)" -f (Split-Path $obj -Leaf), (Get-Item $obj).Length) -ForegroundColor Green

# ---- 自检：甲言内建是否全部内联（不应留成外部引用）----
$shouldBeInlined = @('_bump_set', '_bump_top', 'atomic_fetch_add', 'atomic_compare_exchange',
                     'get_proc_address', 'load_library', '__qcc_heap_counter')
$undef = @()
(& objdump -t $obj 2>&1) | ForEach-Object {
    if ($_ -match '\(sec\s+0\)') { $undef += ($_ -replace '\s+', ' ').Trim().Split(' ')[-1] }
}
$undef = @($undef | Sort-Object -Unique)

Write-Host ''
Write-Host '---- 自检：甲言内建符号应已内联（不该出现在未定义里）----'
$bad = @()
foreach ($s in $shouldBeInlined) {
    if ($undef -contains $s) { Write-Host ("  [X] {0,-26} 仍是外部引用" -f $s) -ForegroundColor Red; $bad += $s }
    else                     { Write-Host ("  [v] {0,-26} 已内联/已定义" -f $s) -ForegroundColor Green }
}

Write-Host ''
Write-Host (' 剩余未定义（应只剩 Windows API __imp_* / 数学库 / CRT）: 共 ' + $undef.Count + ' 个')
$undef | Select-Object -First 12 | ForEach-Object { Write-Host ('   ' + $_) }
if ($undef.Count -gt 12) { Write-Host ('   … 余 ' + ($undef.Count - 12) + ' 个') }

Write-Host ''
if ($bad.Count -gt 0) {
    Write-Host '⚠️  有甲言内建仍留成外部引用 —— 检查 qcc_zh.jy 的 主薄() 名单（位非标[]）是否登记齐全' -ForegroundColor Yellow
}

Write-Host '---- 用法 ----' -ForegroundColor Cyan
Write-Host ('  gcc your_driver.c your_module.obj "{0}" -lm -o app.exe' -f $obj)
Write-Host ''

if ($bad.Count -gt 0) { exit 1 }
exit 0
