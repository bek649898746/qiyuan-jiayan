# anchor_diff.ps1 —— 锚差分扫描（用已有锚作文档化探针 · 回归检测器）
#
# 由来: R500（第 369 站 round 87）发现「直接用已有锚作文档化探针做差分」比手写探针更可靠 ——
#       锚有 @EXPECTED 断言、已被闸验证过、覆盖 goal 特性，且**零语法风险**
#       （手写探针会踩词法/类型词陷阱：§9.142 的 双精度、§9.143 的 形 单字名各踩一次）。
#
# 用法:
#   pwsh -File scripts\anchor_diff.ps1                    # 默认扫 goal 特性相关的 10 个模式
#   pwsh -File scripts\anchor_diff.ps1 -Patterns 'feat_zh_nullable_*'
#
# 判读（与 diff_sweep.ps1 同纪律）:
#   · **差异 != 回归** —— r394 是旧编译器，差异多为"旧缺陷已被修好"（改善）。
#   · 真回归的判据 = 「r394 的输出 == 锚注释里写的 @EXPECTED」而「当前 != @EXPECTED」。
#   · **双失败**（两边都编不过）多为"语义负例锚"（如 feat_err2.c 故意触发 A9001，缺 _neg 后缀）。
#   ⚠️ 必须逐条核对锚头注释，不可只看"有差异"就报警。
param(
    [string[]]$Patterns = @(
        'feat_zh_nullable_*', 'feat_*err*', 'feat_zh_disc_*', 'feat_zh_own_*',
        'feat_zh_borrow*', 'feat_raii*', 'feat_*atomic*', 'feat_*coro*',
        'feat_*thread*', 'feat_mutex*'
    ),
    [string]$Scripts = 'scripts',
    [string]$Baseline = '.\scratch_c1b2\r394.exe',
    [string]$Current = '.\qcc_zh_v2.exe',
    [string]$Scratch = 'scratch_c1b2\as'
)
$ErrorActionPreference = 'Continue'
$files = @()
foreach ($p in $Patterns) { $files += Get-ChildItem "$Scripts\$p" -File -ErrorAction SilentlyContinue }
$files = $files | Sort-Object Name -Unique
Write-Host ("候选锚数 = " + $files.Count + "（_neg 负例锚将跳过）")

$diff = @(); $same = 0; $skip = 0; $negSkip = 0
foreach ($f in $files) {
    if ($f.Name -match '_neg') { $negSkip++; continue }
    $r = @{}
    foreach ($c in @(@('r394', $Baseline), @('cur', $Current))) {
        $out = "$Scratch`_$($f.BaseName)_$($c[0]).exe"
        if (Test-Path $out) { Remove-Item $out -Force }
        & $c[1] $f.FullName -o $out *> "$Scratch`_$($f.BaseName)_$($c[0]).log" 2>&1 | Out-Null
        if (Test-Path $out) {
            $o = (& $out 2>&1 | Out-String).Trim().Trim()
            $r[$c[0]] = $o
        } else { $r[$c[0]] = 'CFAIL' }
    }
    if ($r['r394'] -eq 'CFAIL' -and $r['cur'] -eq 'CFAIL') {
        $skip++
        Write-Host ("双失败（可能是语义负例锚）: " + $f.Name + " | " + ((Get-Content "$Scratch`_$($f.BaseName)_cur.log" -TotalCount 1) -join ''))
    }
    elseif ($r['r394'] -eq $r['cur']) { $same++ }
    else { $diff += ("$($f.Name)`n   r394: [$($r['r394'])]`n   cur : [$($r['cur'])]") }
}
Write-Host ""
Write-Host "相同=$same 双失败=$skip 跳过的_neg=$negSkip 差异=$($diff.Count)"
$diff | ForEach-Object { Write-Host ('--- ' + $_) }
Write-Host ""
Write-Host '⚠️ 纪律: 差异需逐条核对锚头 @EXPECTED；双失败多系语义负例锚（缺 _neg 后缀）。'
Write-Host '⚠️ 判据: 只有「r394 == @EXPECTED 且 当前 != @EXPECTED」才是真回归。'
