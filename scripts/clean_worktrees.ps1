# worktree 清理脚本（默认 dry-run，只列不删）
#
# ⚠️ 前提：先跑 archive_worktrees.ps1 归档，确认 D:\qiyuan_home\backups\worktree_归档_<日期>\
#         存在且清单无误，再用本脚本清理。
#
# 用法：
#   .\scripts\clean_worktrees.ps1              # 只列出（安全）
#   .\scripts\clean_worktrees.ps1 -Execute     # 真删（二次确认）
#   .\scripts\clean_worktrees.ps1 -Execute -Force   # 真删不确认
#   .\scripts\clean_worktrees.ps1 -Keep jiayan_exp,_sync_judge_20261017   # 保留指定
#
# 安全：
#   - 永远不删主工作区
#   - 删除前检查归档目录是否存在
#   - 用 git worktree remove --force（已归档，改动可恢复）
#
# 启元 · 审计岗 · 2026-10-29

[CmdletBinding()]
param(
    [switch]$Execute,
    [switch]$Force,
    [string[]]$Keep = @(),
    [string]$ArchiveDir = 'D:\qiyuan_home\backups\worktree_归档_20261029'
)

$ErrorActionPreference = 'Continue'
$repo = Split-Path -Parent $PSScriptRoot
Set-Location $repo

Write-Host "=== worktree 清理 ===" -ForegroundColor Cyan
Write-Host "仓库: $repo"
Write-Host ""

# ── 1. 归档检查（安全前提）────────────────────────────────
if (-not (Test-Path $ArchiveDir)) {
    Write-Host "❌ 归档目录不存在：$ArchiveDir" -ForegroundColor Red
    Write-Host "   请先跑 .\scripts\archive_worktrees.ps1" -ForegroundColor Yellow
    exit 1
}
$manifest = Join-Path $ArchiveDir '归档清单.md'
if (-not (Test-Path $manifest)) {
    Write-Host "❌ 归档清单不存在：$manifest" -ForegroundColor Red
    exit 1
}
Write-Host "✅ 归档已确认：$ArchiveDir" -ForegroundColor Green
Write-Host ""

# ── 2. 列出 worktree ─────────────────────────────────────
$raw = & git worktree list --porcelain 2>&1
$wts = @(); $cur = @{}
foreach ($l in $raw) {
    if ($l -match '^worktree (.+)$') { if ($cur.Count) { $wts += [PSCustomObject]$cur }; $cur = @{ path = $Matches[1] } }
    elseif ($l -match '^HEAD (.+)$') { $cur.head = $Matches[1] }
    elseif ($l -match '^branch (.+)$') { $cur.branch = $Matches[1] }
    elseif ($l -match '^detached$') { $cur.detached = $true }
}
if ($cur.Count) { $wts += [PSCustomObject]$cur }

$mainNorm = $repo.TrimEnd('\')

$targets = @()
$skipped = @()
foreach ($w in $wts) {
    $p = ($w.path -replace '/', '\').TrimEnd('\')
    if ($p -eq $mainNorm) { $skipped += "主工作区（永不清理）"; continue }
    $leaf = Split-Path $p -Leaf
    if ($Keep -contains $leaf) { $skipped += "保留（-Keep）: $leaf"; continue }
    if (-not (Test-Path $p)) { $skipped += "目录不存在: $p"; continue }
    $targets += [PSCustomObject]@{ Path = $p; Leaf = $leaf; Head = $w.head; Branch = $(if($w.detached){'detached'}elseif($w.branch){$w.branch -replace 'refs/heads/',''}else{'?'}) }
}

# ── 3. 统计 ──────────────────────────────────────────────
$totalMB = 0
foreach ($t in $targets) {
    $fs = Get-ChildItem $t.Path -Recurse -File -Force -ErrorAction SilentlyContinue | Where-Object { $_.FullName -notmatch '\\\.git\\' -and $_.Name -ne '.git' }
    $sz = ($fs | Measure-Object Length -Sum).Sum
    if (-not $sz) { $sz = 0 }
    $totalMB += ($sz / 1MB)
    $t | Add-Member -NotePropertyName SizeMB -NotePropertyValue ([Math]::Round($sz / 1MB, 1)) -Force
}

Write-Host "=== 待清理 $($targets.Count) 个 ===" -ForegroundColor Cyan
foreach ($t in $targets) {
    Write-Host ("  {0,-42} {1,8:N1} MB  {2}" -f $t.Leaf, $t.SizeMB, $t.Branch)
}
Write-Host ""
Write-Host ("  合计可回收: {0:N1} MB" -f $totalMB) -ForegroundColor Yellow
Write-Host ""

if ($skipped.Count -gt 0) {
    Write-Host "=== 跳过 ===" -ForegroundColor Cyan
    $skipped | ForEach-Object { Write-Host "  $_" }
    Write-Host ""
}

if ($targets.Count -eq 0) {
    Write-Host "没有可清理的 worktree。" -ForegroundColor Green
    exit 0
}

# ── 4. 执行 ──────────────────────────────────────────────
if (-not $Execute) {
    Write-Host "（dry-run 模式，未删除任何内容）" -ForegroundColor Yellow
    Write-Host "  要真清请加 -Execute"
    Write-Host "  想保留某些：-Keep jiayan_exp,_sync_judge_20261017"
    exit 0
}

if (-not $Force) {
    Write-Host "⚠️  即将删除 $($targets.Count) 个 worktree / $('{0:N1}' -f $totalMB) MB" -ForegroundColor Yellow
    Write-Host "   它们都已归档在：$ArchiveDir"
    Write-Host "   重建方法：git worktree add <路径> <HEAD>"
    $ans = Read-Host "   输入 YES 继续"
    if ($ans -ne 'YES') {
        Write-Host "已取消。" -ForegroundColor Yellow
        exit 0
    }
}

$ok = 0; $fail = 0; $freed = 0
foreach ($t in $targets) {
    $mb = $t.SizeMB
    & git worktree remove $t.Path --force 2>&1 | Out-Null
    if ($LASTEXITCODE -eq 0) {
        Write-Host ("  ✅ {0,-42} 回收 {1,7:N1} MB" -f $t.Leaf, $mb) -ForegroundColor Green
        $ok++; $freed += $mb
    } else {
        Write-Host ("  ❌ {0,-42} 失败" -f $t.Leaf) -ForegroundColor Red
        $fail++
    }
}
& git worktree prune 2>&1 | Out-Null

Write-Host ""
Write-Host "=== 清理完成 ===" -ForegroundColor Green
Write-Host "  成功: $ok 个"
Write-Host ("  回收: {0:N1} MB" -f $freed)
if ($fail -gt 0) { Write-Host "  失败: $fail 个" -ForegroundColor Yellow }
Write-Host ""
Write-Host "=== 剩余 worktree ===" -ForegroundColor Cyan
& git worktree list 2>&1 | ForEach-Object { Write-Host "  $_" }
