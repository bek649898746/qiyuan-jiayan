# 甲言 × C 混合编程一键构建（四步）
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path "$PSScriptRoot\..\..").Path
$qcc = Join-Path $repo 'qcc_zh_seed.exe'
$jyld = Join-Path $repo 'jyld.exe'
Push-Location $PSScriptRoot
try {
    gcc -O2 -c hot.c -o hot.o
    if ($LASTEXITCODE -ne 0) { throw 'gcc 编译 hot.c 失败' }
    & $qcc -c app.jy -o app.o
    if ($LASTEXITCODE -ne 0) { throw 'qcc -c app.jy 失败' }
    & $jyld app.o hot.o -o app.exe
    if ($LASTEXITCODE -ne 0) { throw 'jyld 链接失败' }
    & .\app.exe
    if ($LASTEXITCODE -ne 0) { throw 'app.exe 运行失败' }
} finally {
    Pop-Location
}
