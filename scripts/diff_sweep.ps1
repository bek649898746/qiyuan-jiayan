# diff_sweep.ps1 —— 全探针差分扫描（r394 基线 vs 当前编译器）· 回归检测器
# 由来: R458（第 369 站 round 46）发现「老基线反而是对的」真回归（hb3）⟹ 反向差分是有效判据。
# 用法: pwsh -File scripts\diff_sweep.ps1   （在 qiyuan-jiayan 根目录）
# 输出: 相同 / 跳过(编译失败) / 差异 三类计数 + 逐条差异明细
# 判读: **差异≠回归** —— r394 多数是历史缺陷（崩/只写首字段），当前编译器修好才会出现差异。
#       真回归的判据 = 「r394 的结果 == 该探针注释里写的期望值」而「当前结果 != 期望值」。
#       ⟹ 必须**逐条读探针注释核对期望**，不可只看"有差异"就报警。
param(
    [string]$Probes = 'scratch_c1b2\probes',
    [string]$Baseline = 'scratch_c1b2\r394.exe',
    [string]$Current = 'qcc_zh_v2.exe',
    [string]$Scratch = 'scratch_c1b2\sw'
)
$ErrorActionPreference = 'Continue'
$files = Get-ChildItem "$Probes\*.jy.c" | Sort-Object Name
Write-Host ("探针总数: " + $files.Count)
$diff = @(); $same = 0; $skip = 0
foreach ($f in $files) {
    $n = $f.BaseName -replace '\.jy$', ''
    $r = @{}
    foreach ($c in @(@('r394', $Baseline), @('cur', $Current))) {
        $out = "$Scratch`_$n`_$($c[0]).exe"
        if (Test-Path $out) { Remove-Item $out -Force }
        & $c[1] $f.FullName -o $out *> "$Scratch`_$n`_$($c[0]).log" 2>&1 | Out-Null
        if (Test-Path $out) {
            $o = (& $out 2>&1 | Out-String).Trim()
            $r[$c[0]] = ($o -replace "`r`n", ' | ') + " [exit=$LASTEXITCODE]"
        } else { $r[$c[0]] = 'CFAIL' }
    }
    if ($r['r394'] -eq 'CFAIL' -or $r['cur'] -eq 'CFAIL') { $skip++ }
    elseif ($r['r394'] -eq $r['cur']) { $same++ }
    else { $diff += ("$n`n   r394: " + $r['r394'] + "`n   cur : " + $r['cur']) }
}
Write-Host "相同=$same 跳过(编译失败)=$skip 差异=$($diff.Count)"
$diff | ForEach-Object { Write-Host ('--- ' + $_) }
Write-Host '⚠️ 差异需逐条核对探针注释里的期望值, 不可只看有差异就报警。'
