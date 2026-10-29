# ============================================================
# jylib 双轨构建脚本
#   同时产出「甲言编译版」与「gcc 编译版」，便于交叉验证
#
#   用法: powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1
#
#   注意: 源会被复制到纯 ASCII 工作路径后再编译 ——
#         甲言拒收中文路径参数（实测传中文目录名收到乱码）
# ============================================================
$ErrorActionPreference = 'Continue'

$here  = Split-Path -Parent $MyInvocation.MyCommand.Path
$work  = 'D:\qiyuan_home\jylib_build'          # 纯 ASCII 工作区
$repo  = 'C:\Users\Administrator\Desktop\qiyuan-jiayan'
$qccCands = @(
    (Join-Path $repo 'qcc_zh_v2.exe'),
    (Join-Path $repo 'qcc_zh_seed.exe'),
    (Join-Path $repo 'qcc_zh.exe')
)
$qcc = $qccCands | Where-Object { Test-Path $_ } | Select-Object -First 1

Write-Host ''
Write-Host '=============================================='
Write-Host ' jylib build  (jiayan + gcc dual track)'
Write-Host '=============================================='
Write-Host " 源目录   : $here"
Write-Host " 工作区   : $work"
Write-Host " 甲言     : $(if($qcc){Split-Path $qcc -Leaf}else{'(未找到)'})"
Write-Host ''

# ---- 准备 ASCII 工作区 ----
Remove-Item $work -Recurse -Force -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force -Path $work | Out-Null
Copy-Item (Join-Path $here 'src\*.c')     $work -Force
Copy-Item (Join-Path $here 'include\*.h') $work -Force
New-Item -ItemType Directory -Force -Path (Join-Path $here 'lib') | Out-Null
New-Item -ItemType Directory -Force -Path (Join-Path $here 'build') | Out-Null

$env:QCC_RT_PATH = Join-Path $repo 'seed\qcc_rt.c'

$sources = @(Get-ChildItem $work -Filter 'jylib_*.c' -File | Where-Object { $_.Name -notlike '*selftest*' })
$results = @()

# ---- 双轨编译 ----
foreach ($s in $sources) {
    $base = $s.BaseName
    Write-Host "[$base]"

    # --- 甲言 ---
    $jyObj = "$base.jy.obj"
    if ($qcc) {
        Push-Location $work
        $out = & $qcc '-c' $s.Name '-o' $jyObj 2>&1
        $rc  = $LASTEXITCODE
        Pop-Location
        if ($rc -eq 0) { Write-Host "  甲言 -> $jyObj   OK" -ForegroundColor Green }
        else {
            Write-Host "  甲言 -> FAIL (rc=$rc)" -ForegroundColor Red
            $out | Select-Object -First 3 | ForEach-Object { Write-Host "      $_" }
        }
        if (Test-Path (Join-Path $work $jyObj)) { Copy-Item (Join-Path $work $jyObj) (Join-Path $here "lib\$jyObj") -Force }
    } else {
        Write-Host "  甲言 -> 跳过（未找到 qcc_zh*.exe）" -ForegroundColor Yellow
    }

    # --- gcc ---
    $gccObj = "$base.gcc.obj"
    & gcc -w -I $work -c $s.FullName -o (Join-Path $work $gccObj) 2>&1 | Select-Object -First 3 | ForEach-Object { Write-Host "      $_" }
    if (Test-Path (Join-Path $work $gccObj)) { Copy-Item (Join-Path $work $gccObj) (Join-Path $here "lib\$gccObj") -Force; Write-Host "  gcc  -> $gccObj   OK" -ForegroundColor Green }
    else { Write-Host "  gcc  -> FAIL" -ForegroundColor Red }

    $results += [pscustomobject]@{ Module=$base; JY=$(Test-Path (Join-Path $here "lib\$jyObj")); GCC=$(Test-Path (Join-Path $here "lib\$gccObj")) }
}

# ---- 自测（gcc 编自测模块 + 驱动，分别链接两种库）----
Write-Host ''
Write-Host '[selftest]'
$selftests = @(Get-ChildItem $work -Filter '*_selftest.c' -File | Sort-Object Name)
if ($selftests.Count -eq 0) { Write-Host '  (无自测模块)' -ForegroundColor Yellow }

$selftestResults = @()
foreach ($stf in $selftests) {
    $mod = $stf.BaseName -replace '_selftest$',''
    $libObjGcc = Join-Path $work "$mod.gcc.obj"
    $libObjJy  = Join-Path $work "$mod.jy.obj"
    if (-not (Test-Path $libObjGcc) -and -not (Test-Path $libObjJy)) {
        Write-Host ("  [{0}] 跳过（库 obj 不存在）" -f $mod) -ForegroundColor Yellow
        continue
    }
    $hdr = Join-Path $work "$mod.h"
    if (-not (Test-Path $hdr)) {
        Write-Host ("  [{0}] 跳过（头文件 {1}.h 不存在）" -f $mod, $mod) -ForegroundColor Yellow
        continue
    }
    # 驱动：调用 <mod>_selftest() 并作为退出码
    $drv = Join-Path $work "stmain_$mod.c"
    $fnName = 'jy_' + ($mod -replace '^jylib_','') + '_selftest'
    @"
#include <stdio.h>
#include "$mod.h"
int main(void){ int f = $fnName(); printf("; exit=%d\n", f); return f; }
"@ | ForEach-Object { [IO.File]::WriteAllText($drv, $_, (New-Object Text.UTF8Encoding $false)) }

    & gcc -w -I $work -c $stf.FullName -o (Join-Path $work "st_$mod.obj") 2>&1 | Out-Null
    if ($LASTEXITCODE -ne 0) {
        Write-Host ("  [{0}] 自测编译失败" -f $mod) -ForegroundColor Red
        continue
    }
    foreach ($tag in @('gcc','jy')) {
        $obj = Join-Path $work "$mod.$tag.obj"
        if (-not (Test-Path $obj)) { continue }
        $exe = Join-Path $work "selftest_${mod}_$tag.exe"
        & gcc -w -I $work -o $exe $drv $obj (Join-Path $work "st_$mod.obj") 2>&1 | Out-Null
        if ($LASTEXITCODE -ne 0) {
            Write-Host ("  [{0}/{1}] 链接失败" -f $mod, $tag) -ForegroundColor Red
            $selftestResults += [pscustomobject]@{ Module=$mod; Tag=$tag; Exit='LINK-FAIL'; Detail='' }
            continue
        }
        $logf = Join-Path $work "st_${mod}_$tag.txt"
        $p = Start-Process -FilePath $exe -NoNewWindow -Wait -PassThru -RedirectStandardOutput $logf
        $code = $p.ExitCode
        $txt = (Get-Content $logf -ErrorAction SilentlyContinue | Where-Object { $_ -match 'exit=' }) -join ''
        $color = if ($code -eq 0) { 'Green' } else { 'Red' }
        Write-Host ("  [{0}/{1}] exit={2}  {3}" -f $mod, $tag, $code, $txt) -ForegroundColor $color
        $selftestResults += [pscustomobject]@{ Module=$mod; Tag=$tag; Exit=$code; Detail=$txt }
    }
}

# ---- 汇总 ----
Write-Host ''
Write-Host '--------------- 汇总 ---------------'
$results | ForEach-Object { Write-Host ("  {0,-20} 甲言={1,-6} gcc={2,-6}" -f $_.Module, $(if($_.JY){'OK'}else{'--'}), $(if($_.GCC){'OK'}else{'--'})) }
Write-Host ''
if ($selftestResults.Count -gt 0) {
    Write-Host '  自测（双轨对照）:'
    $byMod = $selftestResults | Group-Object Module
    foreach ($g in $byMod) {
        $gccRow = $g.Group | Where-Object Tag -eq 'gcc'
        $jyRow  = $g.Group | Where-Object Tag -eq 'jy'
        $gv = if ($gccRow) { $gccRow.Exit } else { '--' }
        $jv = if ($jyRow)  { $jyRow.Exit }  else { '--' }
        $ok = ($gv -eq 0) -and ($jv -eq 0)
        $tag = if ($ok) { '✅ 双轨全绿' } elseif ($gv -eq 0 -or $jv -eq 0) { '⚠️ 单轨' } else { '❌ 失败' }
        Write-Host ("    {0,-22} gcc={1,-8} jy={2,-8} {3}" -f $g.Name, $gv, $jv, $tag) -ForegroundColor $(if($ok){'Green'}else{'Red'})
    }
    Write-Host ''
    Write-Host '  判据: 两轨 exit 都必须为 0；任一非 0 即为回归。'
} else {
    Write-Host '  (本轮无自测)' -ForegroundColor Yellow
}
Write-Host ''
Write-Host '  产物: lib/'
Get-ChildItem (Join-Path $here 'lib') -ErrorAction SilentlyContinue | ForEach-Object { Write-Host ("    {0,-28} {1,8:N0} B" -f $_.Name, $_.Length) }
Write-Host ''
Write-Host '  注: 甲言版构建状态 —— 第 262 站已修 __qcc_ginit 内联 memset 指针截断；' -ForegroundColor Yellow
Write-Host '      启元 Q8 修 -> 字段基址 32 位截断（参数指针 未登记 指元素尺寸 致判据漏）。' -ForegroundColor Yellow
Write-Host '      [jy] 行须 pass=6 fail=0 且 exit=0；直接跑，勿用 gdb（gdb 抑制 ASLR 会掩盖截断）。' -ForegroundColor Yellow
Write-Host ''
