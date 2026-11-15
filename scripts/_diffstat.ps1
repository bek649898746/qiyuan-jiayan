# _diffstat.ps1 — 逐文件统计 legacy/IR 的函数级差异数量（临时量具）
# 用法: powershell -File scripts\_diffstat.ps1 [cc]
param([string]$Cc = 'qcc_zh_try.exe')
$ErrorActionPreference = 'Stop'
$src = 'D:\qcc_corpus\lua-5.4.7\src'
$w = 'build\diffstat'
New-Item -ItemType Directory -Force -Path $w | Out-Null
$files = Get-ChildItem $src -Filter *.c | Sort-Object Name
$tool = 'C:\Users\ADMINI~1\AppData\Local\Temp\grok-goal-bdf2beb33b3e\implementer\allfn.py'
foreach ($f in $files) {
    $env:QCC_CODEGEN = ''
    & ".\$Cc" $f.FullName -c -o "$w\a.obj" 2>$null | Out-Null
    $env:QCC_CODEGEN = 'ir'
    & ".\$Cc" $f.FullName -c -o "$w\b.obj" 2>$null | Out-Null
    Remove-Item Env:\QCC_CODEGEN
    if (-not (Test-Path "$w\a.obj") -or -not (Test-Path "$w\b.obj")) { Write-Host "$($f.Name) SKIP"; continue }
    & C:\mingw-w64\bin\objdump.exe -d "$w\a.obj" | Out-File -Encoding utf8 "$w\a.dis"
    & C:\mingw-w64\bin\objdump.exe -d "$w\b.obj" | Out-File -Encoding utf8 "$w\b.dis"
    $out = python $tool "$w\a.dis" "$w\b.dis" 2>&1
    $bad = ($out | Select-String -Pattern '^--- ').Count
    $same = if ($null -eq $bad) { 0 } else { $bad }
    Write-Host ("{0,-14} 差异函数 {1}" -f $f.Name, $same)
}
