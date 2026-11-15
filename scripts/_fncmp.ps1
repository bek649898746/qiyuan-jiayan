# _fncmp.ps1 — 单文件 legacy vs IR 的函数级规范化对比（临时量具，删前请确认）
# 用法: powershell -File scripts\_fncmp.ps1 llex.c qcc_zh_try.exe
param([string]$File, [string]$Cc = 'qcc_zh_try.exe')
$ErrorActionPreference = 'Stop'
$src = "D:\qcc_corpus\lua-5.4.7\src\$File"
$w = "build\fncmp"
New-Item -ItemType Directory -Force -Path $w | Out-Null
$env:QCC_CODEGEN = ''
& ".\$Cc" $src -c -o "$w\a.obj" | Out-Null
$env:QCC_CODEGEN = 'ir'
& ".\$Cc" $src -c -o "$w\b.obj" | Out-Null
Remove-Item Env:\QCC_CODEGEN
& C:\mingw-w64\bin\objdump.exe -d "$w\a.obj" | Out-File -Encoding utf8 "$w\a.dis"
& C:\mingw-w64\bin\objdump.exe -d "$w\b.obj" | Out-File -Encoding utf8 "$w\b.dis"
python "C:\Users\ADMINI~1\AppData\Local\Temp\grok-goal-bdf2beb33b3e\implementer\allfn.py" "$w\a.dis" "$w\b.dis"
