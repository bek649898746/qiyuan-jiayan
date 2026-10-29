# 清理甲言构建产物（默认 dry-run，只列不删）
#
# 背景：2026-10-28 审计发现工作区 6,067 MB 里 5,710 MB（94%）是未跟踪的构建残留
#       —— 每次自举产生的 .asm / .o / 各代 .exe 只增不减（2,834 个文件）。
#       这正是「自举多几次硬盘就爆」的根因。
#
# 用法：
#   .\scripts\clean_build.ps1                    # 只列出能回收多少（安全，不删）
#   .\scripts\clean_build.ps1 -Size              # 同上，加按类型汇总
#   .\scripts\clean_build.ps1 -Execute           # 真删（会二次确认）
#   .\scripts\clean_build.ps1 -Execute -Force    # 真删，不确认（CI 用）
#   .\scripts\clean_build.ps1 -KeepAsm           # 保留 .asm（复现优化器输入用）
#
# 保护清单（永不删）：
#   qcc_zh_seed.exe   —— 自举起点 + 唯一入库的产物
#   任何 git 跟踪的文件
#
# 启元 · 审计岗 · 2026-10-28

[CmdletBinding()]
param(
    [switch]$Execute,
    [switch]$Force,
    [switch]$Size,
    [switch]$KeepAsm
)

$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
Set-Location $repo

Write-Host "=== 甲言构建产物清理 ===" -ForegroundColor Cyan
Write-Host "仓库: $repo"
Write-Host ""

# ── 1. 取 git 跟踪清单（这些永不删）────────────────────────────
$trackedSet = @{}
$tracked = & git ls-files 2>$null
if ($LASTEXITCODE -ne 0) {
    Write-Host "⚠️  git ls-files 失败 —— 无法判定哪些是跟踪文件，中止（安全优先）" -ForegroundColor Red
    exit 1
}
foreach ($f in $tracked) { $trackedSet[$f.Replace('/', '\')] = $true }
Write-Host "git 跟踪文件: $($trackedSet.Count) 个（全部保护）"

# ── 2. 白名单（即使未跟踪也保留）───────────────────────────────
$whitelist = @(
    'qcc_zh_seed.exe',        # 自举起点，唯一入库产物
    'qcc_zh_genprobe.exe',    # 名字含 gen，保守保留（不确定是否工具）
    '.gitignore'
)

# ── 3. 候选清理类型 ───────────────────────────────────────────
$patterns = @()
$patterns += '*.o'
$patterns += '*.obj'
if (-not $KeepAsm) { $patterns += '*.asm' }
$patterns += '*.pdb'
$patterns += '*.ilk'
$patterns += '*.lib'
$patterns += '*.exp'
$patterns += '*.log'
$patterns += '*.s'

# 非 seed 的 exe —— 历史上各代产物（实测根目录 233 个 / build 132 个是大头，约 4.7 GB）
$exePatterns = @(
    'qcc_*.exe', 'qdbg*.exe', 'dev_*.exe', 'v1_*.exe', 'control_*.exe',
    'old.exe', 'a.exe', 'b.exe', 't*.exe', 'test*.exe', 'probe*.exe'
)
# ⚠️ 这些目录下的 exe 是「工具」不是「产物」，必须排除
$exeSafeDirs = @('scripts', 'examples', '_audit_inbox', 'tools', 'jylib')

# ── 4. 收集候选 ───────────────────────────────────────────────
$candidates = New-Object System.Collections.ArrayList
$skippedTracked = 0
$skippedWhitelist = 0

function Add-Candidate {
    param($file)
    $rel = $file.FullName.Substring($repo.Length + 1)
    if ($trackedSet.ContainsKey($rel)) { $script:skippedTracked++; return }
    if ($whitelist -contains $file.Name) { $script:skippedWhitelist++; return }
    [void]$candidates.Add($file)
}

foreach ($pat in $patterns) {
    Get-ChildItem -Path $repo -Filter $pat -File -Recurse -ErrorAction SilentlyContinue |
        Where-Object { $_.FullName -notmatch '\\\.git\\' } |
        ForEach-Object { Add-Candidate $_ }
}
foreach ($pat in $exePatterns) {
    Get-ChildItem -Path $repo -Filter $pat -File -Recurse -ErrorAction SilentlyContinue |
        Where-Object { $_.FullName -notmatch '\\\.git\\' } |
        Where-Object {
            # 排除工具目录（scripts/ examples/ 等下的 exe 是工具，不是构建产物）
            $rel = $_.FullName.Substring($repo.Length + 1)
            $safe = $false
            foreach ($d in $exeSafeDirs) { if ($rel -like "$d\*") { $safe = $true; break } }
            -not $safe
        } |
        ForEach-Object { Add-Candidate $_ }
}

# ── 5. 统计 ──────────────────────────────────────────────────
$totalBytes = ($candidates | Measure-Object Length -Sum).Sum
if (-not $totalBytes) { $totalBytes = 0 }

Write-Host ""
Write-Host "=== 扫描结果 ===" -ForegroundColor Cyan
Write-Host "  候选删除 : $($candidates.Count) 个文件"
Write-Host ("  可回收   : {0:N1} MB" -f ($totalBytes / 1MB))
Write-Host "  跳过(跟踪): $skippedTracked 个"
Write-Host "  跳过(白名单): $skippedWhitelist 个"

if ($Size -and $candidates.Count -gt 0) {
    Write-Host ""
    Write-Host "=== 按类型 ===" -ForegroundColor Cyan
    $candidates | Group-Object Extension | Sort-Object { ($_.Group | Measure-Object Length -Sum).Sum } -Descending | ForEach-Object {
        $gBytes = ($_.Group | Measure-Object Length -Sum).Sum
        Write-Host ("  {0,-10} {1,4} 个  {2,9:N1} MB" -f $_.Name, $_.Count, ($gBytes / 1MB))
    }
}

if ($candidates.Count -eq 0) {
    Write-Host ""
    Write-Host "工作区已干净，无需清理。" -ForegroundColor Green
    exit 0
}

# ── 6. 干跑：列出 Top 15 ─────────────────────────────────────
Write-Host ""
Write-Host "=== 最大的 15 个 ===" -ForegroundColor Cyan
$candidates | Sort-Object Length -Descending | Select-Object -First 15 | ForEach-Object {
    $rel = $_.FullName.Substring($repo.Length + 1)
    Write-Host ("  {0,-42} {1,9:N1} MB" -f $rel, ($_.Length / 1MB))
}

# ── 7. 执行 ──────────────────────────────────────────────────
if (-not $Execute) {
    Write-Host ""
    Write-Host "（dry-run 模式，未删除任何文件）" -ForegroundColor Yellow
    Write-Host "  要真删请加 -Execute"
    exit 0
}

if (-not $Force) {
    Write-Host ""
    Write-Host "⚠️  即将删除 $($candidates.Count) 个文件 / $('{0:N1}' -f ($totalBytes/1MB)) MB" -ForegroundColor Yellow
    Write-Host "   它们都是可重建的构建产物（跑一次自举就会重新生成）。"
    $ans = Read-Host "   输入 YES 继续"
    if ($ans -ne 'YES') {
        Write-Host "已取消。" -ForegroundColor Yellow
        exit 0
    }
}

$deleted = 0; $freed = 0; $failed = 0
foreach ($f in $candidates) {
    try {
        $sz = $f.Length
        Remove-Item $f.FullName -Force -ErrorAction Stop
        $deleted++; $freed += $sz
    } catch {
        $failed++
        Write-Host "  ❌ 删除失败: $($f.Name) — $($_.Exception.Message)" -ForegroundColor Red
    }
}

Write-Host ""
Write-Host "=== 清理完成 ===" -ForegroundColor Green
Write-Host "  已删: $deleted 个"
Write-Host ("  释放: {0:N1} MB" -f ($freed / 1MB))
if ($failed -gt 0) { Write-Host "  失败: $failed 个" -ForegroundColor Yellow }
Write-Host ""
Write-Host "验证自身仍可用："
$seed = Join-Path $repo 'qcc_zh_seed.exe'
if (Test-Path $seed) {
    Write-Host "  ✅ qcc_zh_seed.exe 仍存在（$('{0:N2}' -f ((Get-Item $seed).Length/1MB)) MB）" -ForegroundColor Green
} else {
    Write-Host "  ❌ qcc_zh_seed.exe 不见了！立即用 git checkout 恢复" -ForegroundColor Red
}
