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
$st = Join-Path $work 'jylib_sha256_selftest.c'
$drv = Join-Path $work 'selftest_main.c'
@"
#include <stdio.h>
#include "jylib_sha256.h"
int main(void){ int f = jy_sha256_selftest(); return f; }
"@ | Set-Content -Path $drv -Encoding UTF8

if (Test-Path $st) {
    & gcc -w -I $work -c $st -o (Join-Path $work 'st.obj') 2>&1 | Out-Null
    foreach ($tag in @('gcc','jy')) {
        $obj = Join-Path $work "jylib_sha256.$tag.obj"
        if (-not (Test-Path $obj)) { continue }
        $exe = Join-Path $work "selftest_$tag.exe"
        & gcc -w -I $work -o $exe $drv $obj (Join-Path $work 'st.obj') 2>&1 | Out-Null
        if ($LASTEXITCODE -ne 0) { Write-Host "  [$tag] 链接失败" -ForegroundColor Red; continue }
        $p = Start-Process -FilePath $exe -NoNewWindow -Wait -PassThru -RedirectStandardOutput (Join-Path $work "st_$tag.txt")
        $code = $p.ExitCode
        $txt = (Get-Content (Join-Path $work "st_$tag.txt") -ErrorAction SilentlyContinue | Where-Object { $_ -match 'pass=' }) -join ''
        $color = if ($code -eq 0) { 'Green' } else { 'Red' }
        Write-Host ("  [{0}] exit={1}  {2}" -f $tag, $code, $txt) -ForegroundColor $color
    }
}

# ---- 汇总 ----
Write-Host ''
Write-Host '--------------- 汇总 ---------------'
$results | ForEach-Object { Write-Host ("  {0,-20} 甲言={1,-6} gcc={2,-6}" -f $_.Module, $(if($_.JY){'OK'}else{'--'}), $(if($_.GCC){'OK'}else{'--'})) }
Write-Host ''
Write-Host '  产物: lib/'
Get-ChildItem (Join-Path $here 'lib') -ErrorAction SilentlyContinue | ForEach-Object { Write-Host ("    {0,-28} {1,8:N0} B" -f $_.Name, $_.Length) }
Write-Host ''
Write-Host '  注: 甲言版构建状态 —— 第 262 站已修 __qcc_ginit 内联 memset 指针截断；' -ForegroundColor Yellow
Write-Host '      启元 Q8 修 -> 字段基址 32 位截断（参数指针 未登记 指元素尺寸 致判据漏）。' -ForegroundColor Yellow
Write-Host '      [jy] 行须 pass=6 fail=0 且 exit=0；直接跑，勿用 gdb（gdb 抑制 ASLR 会掩盖截断）。' -ForegroundColor Yellow
Write-Host ''
