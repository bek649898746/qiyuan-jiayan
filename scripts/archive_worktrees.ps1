# worktree 归档脚本（只备份「不可从 git 重建」的部分）
#
# 背景：2026-10-29 发现 15 个历史遗留 worktree，合计 7.3 GB。
#       其中 11 个有未提交改动 —— 那些改动不在任何提交里，删了就没了。
#
# 原则：不复制整个 worktree（源码+产物都能从 git 重建，7.3 GB 里 99% 是这些）。
#       只备份：
#         A. 已跟踪文件的未提交改动 → git diff 生成 patch
#         B. 未跟踪的非产物文件 → 直接复制
#         C. 源码备份文件（qcc_zh.jy.*）→ 直接复制
#         D. 元信息（路径/分支/HEAD/体积/时间）
#
# 用法：
#   .\scripts\archive_worktrees.ps1              # 归档到默认目录
#   .\scripts\archive_worktrees.ps1 -OutDir X    # 指定目录
#
# 启元 · 审计岗 · 2026-10-29

[CmdletBinding()]
param(
    [string]$OutDir = 'D:\qiyuan_home\backups\worktree_归档_20261029'
)

# git 的 CRLF warning 走 stderr，不能用 Stop 否则批处理会中断
$ErrorActionPreference = 'Continue'

# 切到 repo 根（git worktree / git -C 都依赖有效的 repo 上下文）
$repo = Split-Path -Parent $PSScriptRoot
Set-Location $repo

$stamp = Get-Date -Format 'yyyy-MM-dd HH:mm:ss'

Write-Host "=== worktree 归档 ===" -ForegroundColor Cyan
Write-Host "目标: $OutDir"
Write-Host ""

New-Item -ItemType Directory -Force -Path $OutDir | Out-Null

# 取所有 worktree
$raw = & git worktree list --porcelain 2>&1
$wts = @(); $cur = @{}
foreach ($l in $raw) {
    if ($l -match '^worktree (.+)$') { if ($cur.Count) { $wts += [PSCustomObject]$cur }; $cur = @{ path = $Matches[1] } }
    elseif ($l -match '^HEAD (.+)$') { $cur.head = $Matches[1] }
    elseif ($l -match '^branch (.+)$') { $cur.branch = $Matches[1] }
    elseif ($l -match '^detached$') { $cur.detached = $true }
}
if ($cur.Count) { $wts += [PSCustomObject]$cur }

$mainPath = (Get-Location).Path
$manifest = New-Object System.Collections.ArrayList
[void]$manifest.Add("# worktree 归档清单 · $stamp")
[void]$manifest.Add("")
[void]$manifest.Add("> 只备份「不可从 git 重建」的部分（未提交改动 / 未跟踪文件 / 源码备份）")
[void]$manifest.Add("> 源码与构建产物可随时从各自 HEAD 重建，未复制")
[void]$manifest.Add("")
[void]$manifest.Add("| # | 路径 | 体积 | 分支/HEAD | 未提交 | patch | 未跟踪 |")
[void]$manifest.Add("|--:|:--|--:|:--|--:|:--|:--|")

$idx = 0
$archivedBytes = 0
foreach ($w in $wts) {
    $p = ($w.path -replace '/', '\')
    if (-not (Test-Path $p)) { continue }
    if ($p -eq ($mainPath -replace '/', '\')) { continue }   # 跳过主工作区
    $idx++

    $name = Split-Path $p -Leaf
    $sub = Join-Path $OutDir ('{0:D2}_{1}' -f $idx, $name)
    New-Item -ItemType Directory -Force -Path $sub | Out-Null

    # 体积
    $files = Get-ChildItem $p -Recurse -File -Force -ErrorAction SilentlyContinue | Where-Object { $_.FullName -notmatch '\\\.git\\' -and $_.Name -ne '.git' }
    $sz = ($files | Measure-Object Length -Sum).Sum
    if (-not $sz) { $sz = 0 }

    # 状态
    $st = & git -C $p status --porcelain 2>$null
    $dirty = @($st).Count
    $br = if ($w.detached) { "detached" } elseif ($w.branch) { ($w.branch -replace 'refs/heads/', '') } else { '?' }

    # A. patch（已跟踪改动）
    $hasPatch = $false
    $trackedDiff = & git -C $p diff 2>$null
    if ($trackedDiff) {
        $patchFile = Join-Path $sub 'changes.patch'
        $trackedDiff | Out-File -FilePath $patchFile -Encoding UTF8
        $hasPatch = $true
        $archivedBytes += (Get-Item $patchFile).Length
    }

    # B. 未跟踪（只复制非产物）
    $untrackedDirs = @()
    $untrackedCopy = 0
    $skipExt = @('.asm', '.o', '.obj', '.exe', '.pdb', '.ilk', '.lib', '.exp', '.log')
    foreach ($u in ($st | Where-Object { $_ -match '^\?\?' })) {
        $rel = ($u -replace '^\?\?\s+', '').Trim('"').TrimEnd('/')
        $full = Join-Path $p $rel
        if (-not (Test-Path $full)) { continue }
        if (Test-Path $full -PathType Container) {
            # 目录：检查里面有没有非产物文件
            $inner = Get-ChildItem $full -Recurse -File -ErrorAction SilentlyContinue | Where-Object { $skipExt -notcontains $_.Extension }
            if ($inner) {
                $dstDir = Join-Path (Join-Path $sub 'untracked') $rel
                New-Item -ItemType Directory -Force -Path $dstDir | Out-Null
                foreach ($f in $inner) {
                    $relInner = $f.FullName.Substring($full.Length).TrimStart('\')
                    $d = Join-Path $dstDir (Split-Path $relInner -Parent)
                    if ($d) { New-Item -ItemType Directory -Force -Path $d | Out-Null }
                    Copy-Item $f.FullName -Destination $dstDir -Force -ErrorAction SilentlyContinue
                    $untrackedCopy++
                    $archivedBytes += $f.Length
                }
                $untrackedDirs += "$rel ($($inner.Count) 个非产物文件)"
            }
        } else {
            # 单文件：非产物才复制
            if ($skipExt -notcontains ([IO.Path]::GetExtension($full))) {
                $dstDir = Join-Path (Join-Path $sub 'untracked') (Split-Path $rel -Parent)
                if ($dstDir -and $dstDir -ne (Join-Path $sub 'untracked')) { New-Item -ItemType Directory -Force -Path $dstDir | Out-Null }
                $dst = Join-Path (Join-Path $sub 'untracked') $rel
                New-Item -ItemType Directory -Force -Path (Split-Path $dst -Parent) | Out-Null
                Copy-Item $full -Destination $dst -Force
                $untrackedCopy++
                $archivedBytes += (Get-Item $full).Length
                $untrackedDirs += $rel
            }
        }
    }

    # C. 源码备份文件（qcc_zh.jy.* 等）
    $bakCount = 0
    $srcDir = Join-Path $p 'srclib_jiayan'
    if (Test-Path $srcDir) {
        $baks = Get-ChildItem $srcDir -File -ErrorAction SilentlyContinue | Where-Object { $_.Name -match '\.jy\.(bak|orig|fix)' }
        if ($baks) {
            $dstBak = Join-Path $sub '源码备份'
            New-Item -ItemType Directory -Force -Path $dstBak | Out-Null
            foreach ($b in $baks) {
                Copy-Item $b.FullName -Destination $dstBak -Force
                $bakCount++
                $archivedBytes += $b.Length
            }
        }
    }

    # D. 元信息
    $info = @"
worktree 元信息（归档于 $stamp）
================================
路径    : $p
体积    : $([Math]::Round($sz/1MB,1)) MB / $($files.Count) 文件
分支    : $br
HEAD    : $($w.head)
未提交  : $dirty 个条目
patch   : $(if($hasPatch){'changes.patch'}else{'无（无需）'})
未跟踪备份: $untrackedCopy 个文件
源码备份: $bakCount 个

重建方法
--------
  git worktree add <新路径> $($w.head)
"@
    $info | Out-File -FilePath (Join-Path $sub '元信息.txt') -Encoding UTF8

    [void]$manifest.Add("| $idx | ``$p`` | $([Math]::Round($sz/1MB,1)) MB | $br | $dirty | $(if($hasPatch){'✅'}else{'—'}) | $untrackedCopy |")

    $status = if ($dirty -gt 0) { "⚠️ $dirty 个未提交" } else { "干净" }
    Write-Host ("  [{0:D2}] {1,-42} {2,8:N1} MB  {3,-14} {4}" -f $idx, $name, ($sz/1MB), $br, $status)
}

[void]$manifest.Add("")
[void]$manifest.Add("## 汇总")
[void]$manifest.Add("")
[void]$manifest.Add("- worktree 总数（不含主工作区）: **$idx**")
[void]$manifest.Add("- 归档实际占用: **$([Math]::Round($archivedBytes/1MB,2)) MB**（其余为可重建的源码/产物）")
[void]$manifest.Add("- 归档时间: $stamp")
$manifest | Out-File -FilePath (Join-Path $OutDir '归档清单.md') -Encoding UTF8

Write-Host ""
Write-Host "=== 归档完成 ===" -ForegroundColor Green
Write-Host "  目标: $OutDir"
Write-Host ("  实际占用: {0:N2} MB" -f ($archivedBytes/1MB))
Write-Host "  清单: $(Join-Path $OutDir '归档清单.md')"
Write-Host ""
Write-Host "⚠️ 下一步（确认归档无误后才执行）：" -ForegroundColor Yellow
Write-Host "  .\scripts\clean_worktrees.ps1 -Execute"
