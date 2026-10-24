# gen_readme_status.ps1 - README 顶部「最近一次验证」状态块生成器
# 2026-10-24 启元 · 甲言 30h 前进 46 提交/4 代不动点 ⟹ 外部快照必然过期
# 用法: powershell -File scripts/gen_readme_status.ps1 [-Print]

param([switch]$Print)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
Set-Location $root

$head   = (git rev-parse --short HEAD 2>$null)
$branch = (git rev-parse --abbrev-ref HEAD 2>$null)
$time   = (git log -1 --format='%ad' --date=format:'%Y-%m-%d %H:%M' 2>$null)

$fix = '(读取失败)'
$pat = "^\`$expected\s*=\s*'([0-9A-Fa-f]{8})'"
$m = Select-String -Path 'scripts\build_zh.ps1' -Pattern $pat -EA SilentlyContinue | Select-Object -First 1
if ($m) { $fix = ([regex]::Match($m.Line, "'([0-9A-Fa-f]{8})'")).Groups[1].Value }

$srcLines = 0
if (Test-Path 'srclib_jiayan\qcc_zh.jy') {
    $srcLines = @([IO.File]::ReadAllLines((Resolve-Path 'srclib_jiayan\qcc_zh.jy').Path, [Text.Encoding]::UTF8)).Count
}

$anchors = @(Get-ChildItem 'scripts' -File -Filter 'feat_zh_*.c' -EA SilentlyContinue).Count

$seedSha = '(无 seed)'
if (Test-Path 'qcc_zh_seed.exe') {
    $seedSha = (Get-FileHash 'qcc_zh_seed.exe' -Algorithm SHA256).Hash.Substring(0, 8)
}

$BT = [char]96
$L = New-Object System.Collections.Generic.List[string]
$L.Add('<!-- GEN:STATUS:BEGIN - 由 scripts/gen_readme_status.ps1 自动生成, 请勿手改 -->')
$L.Add('> ### 你读到的是哪个状态')
$L.Add('>')
$L.Add('> | 项 | 值 |')
$L.Add('> |:--|:--|')
$L.Add('> | 生成时间 | **' + $time + '** |')
$L.Add('> | HEAD | ' + $BT + $head + $BT + '（分支 ' + $BT + $branch + $BT + '） |')
$L.Add('> | 自举不动点 | **' + $BT + $fix + $BT + '**（权威源 = ' + $BT + 'scripts/build_zh.ps1' + $BT + ' 的 ' + $BT + '$expected' + $BT + '） |')
$L.Add('> | 主源 | **' + $srcLines + ' 行**（' + $BT + 'srclib_jiayan/qcc_zh.jy' + $BT + '） |')
$L.Add('> | 特性锚 | **' + $anchors + ' 个** ' + $BT + 'feat_zh_*.c' + $BT + ' |')
$L.Add('> | 种子指纹 | ' + $BT + $seedSha + $BT + '（' + $BT + 'qcc_zh_seed.exe' + $BT + ' SHA256 前 8 位） |')
$L.Add('>')
$L.Add('> **数字会过期。** 主线约 **1.5 提交/小时**：30 小时可前进 ~46 提交、~4 代不动点。')
$L.Add('> 想知道**现在**的准确值，跑：')
$L.Add('>')
$L.Add('> ' + $BT + $BT + $BT)
$L.Add('> powershell -File scripts/audit_facts.ps1      # 1 秒导出全基线')
$L.Add('> powershell -File scripts/verify_90s.ps1       # 8 秒验真')
$L.Add('> ' + $BT + $BT + $BT)
$L.Add('>')
$L.Add('> 若你手里的快照与上表不符，**一律以仓库最新提交为准**。')
$L.Add('> 重生成本块的命令：' + $BT + 'powershell -File scripts/gen_readme_status.ps1' + $BT)
$L.Add('<!-- GEN:STATUS:END -->')
$block = ($L -join "`n") + "`n"

if ($Print) { Write-Host $block; exit 0 }

$readmePath = Join-Path $root 'README.md'
if (-not (Test-Path $readmePath)) { Write-Host '[gen_readme_status] README.md 不存在' -ForegroundColor Red; exit 1 }

$bomChar = [char]0xFEFF
$raw  = [IO.File]::ReadAllText($readmePath)
$hadBom = $raw.StartsWith([string]$bomChar)
$txt  = $raw.TrimStart($bomChar)

if ($txt -match '(?s)<!-- GEN:STATUS:BEGIN.*?GEN:STATUS:END -->') {
    $txt = [regex]::Replace($txt, '(?s)<!-- GEN:STATUS:BEGIN.*?GEN:STATUS:END -->', $block.TrimEnd())
    $mode = '替换现有块'
} else {
    $firstLineEnd = $txt.IndexOf("`n")
    if ($firstLineEnd -gt 0) {
        $txt = $txt.Insert($firstLineEnd + 1, "`n" + $block)
        $mode = '插入到标题后'
    } else {
        $txt = $block + $txt
        $mode = '插入到文件头'
    }
}

$out = if ($hadBom) { [string]$bomChar + $txt } else { $txt }
[IO.File]::WriteAllText($readmePath, $out, (New-Object System.Text.UTF8Encoding($false)))

Write-Host ('[gen_readme_status] {0} · HEAD={1} · 不动点={2} · 主源={3} 行 · 锚={4}' -f $mode, $head, $fix, $srcLines, $anchors) -ForegroundColor Green