# =============================================================================
#  jyaudit — 甲言产物外部审计器（指令级）
#
#  为什么存在：
#    甲言的验证一直闭环在自己内部（自己编译 → 自己链接 → 自己运行）。
#    闭环永远测不出 ABI 级缺陷 —— 因为甲言产物的最终映像基址低（0x400000 量级），
#    某些错误编码在低基址下"恰好"仍然合法。一旦交给外部工具链（gcc/ld，
#    基址 0x140000000、运行时 0x7ff6...），缺陷立刻现形。
#
#    本工具就是那面镜子：用外部工具链的视角，检查甲言编出的 .obj。
#
#  用法：
#    powershell -File jyaudit.ps1                          # 扫默认源目录（全量）
#    powershell -File jyaudit.ps1 -Src <目录> -N 200        # 抽样 200
#    powershell -File jyaudit.ps1 -Src <单个.c>             # 单文件
#    powershell -File jyaudit.ps1 -Verbose                  # 打印详细反汇编片段
#
#  检测项（全部为静态检测，不需要驱动的目标文件没有 main 也能扫）：
#    [A] ADDR32   重定位        —— 字符串字面量 → 32 位绝对重定位 → gcc 拒绝链接
#    [B] PTR32    指针截断模式  —— 32 位寄存器承载/递增 64 位指针 → 运行时崩溃
#    [C] LINK     链接性        —— gcc -shared 能否接受该 .obj
#
#  判据：本工具只报告"可观测事实"，不做因果断言。每一行都有对应命令可复现。
# =============================================================================
param(
    [string]$Src    = 'D:\.openclaw\workspace\compiler',
    [int]   $N      = 200,                                  # 0 = 全部（全量数千文件会跑数小时）
    [string]$Work   = 'D:\qiyuan_home\jyaudit_build',       # 纯 ASCII 工作区
    [string]$OutDir = 'D:\qiyuan_home\jyaudit_out',
    [string]$Qcc    = '',                                   # 留空则自动探测
    [switch]$Verbose
)

$ErrorActionPreference = 'Continue'
$ProgressPreference    = 'SilentlyContinue'

# ---------------------------------------------------------------- 环境准备
$repo = 'C:\Users\Administrator\Desktop\qiyuan-jiayan'
if (-not $Qcc) {
    $cands = @(
        (Join-Path $repo 'qcc_zh_v2.exe'),
        (Join-Path $repo 'qcc_zh_seed.exe'),
        (Join-Path $repo 'qcc_zh.exe')
    )
    $Qcc = $cands | Where-Object { Test-Path $_ } | Select-Object -First 1
}
if (-not $Qcc -or -not (Test-Path $Qcc)) { Write-Host '致命: 未找到甲言编译器 (qcc_zh*.exe)' -ForegroundColor Red; exit 2 }
$env:QCC_RT_PATH = Join-Path $repo 'seed\qcc_rt.c'

Remove-Item $Work   -Recurse -Force -ErrorAction SilentlyContinue
Remove-Item $OutDir -Recurse -Force -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force -Path $Work, $OutDir | Out-Null

# ---------------------------------------------------------------- 目标清单
if (Test-Path -LiteralPath $Src -PathType Leaf) {
    $items = @(Get-Item -LiteralPath $Src)
} else {
    $items = @(Get-ChildItem -LiteralPath $Src -Filter '*.c' -File -ErrorAction SilentlyContinue)
}
if ($N -gt 0 -and $N -lt $items.Count) { $items = @($items | Get-Random -Count $N) }

$qccHash = (Get-FileHash $Qcc -Algorithm SHA256).Hash.Substring(0,8)

Write-Host ''
Write-Host '=============================================================================='
Write-Host ' jyaudit — 甲言产物外部审计（指令级）'
Write-Host '=============================================================================='
Write-Host " 编译器   : $(Split-Path $Qcc -Leaf)   SHA256=$qccHash"
Write-Host " 源       : $Src"
Write-Host " 目标数   : $($items.Count)"
Write-Host " 工作区   : $Work"
Write-Host ' 检测项   : [A]ADDR32重定位  [B]PTR32指针截断模式  [C]LINK链接性'
Write-Host ''

# ---------------------------------------------------------------- 检测内核
function Test-Obj([string]$objPath, [string]$baseName) {
    $res = [ordered]@{
        File       = $baseName
        Jiayan_OK  = $false
        Gcc_OK     = $false
        Addr32     = 0     # [A] IMAGE_REL_AMD64_ADDR32 计数
        Addr32Sec  = ''    # [A] 涉及的节
        Ptr32Hits  = 0     # [B] 32 位指针危险模式计数
        Ptr32Samp  = ''    # [B] 样本指令
        Link_OK    = $false
        LinkErr    = ''
        Verdict    = ''
    }

    # ---- [A] 重定位扫描 ----
    $rel = & objdump -r $objPath 2>&1
    $a = @($rel | Select-String -Pattern '^\s*[0-9a-f]{8,}\s+IMAGE_REL_AMD64_ADDR32\s+(\S+)')
    $res.Addr32 = $a.Count
    if ($a.Count -gt 0) {
        $res.Addr32Sec = (($a | ForEach-Object { $_.Matches[0].Groups[1].Value } | Sort-Object -Unique) -join ',')
    }

    # ---- [B] 32 位指针危险模式 ----
    #   甲言在内联 memset/memcpy 与 __qcc_ginit 里，会把 64 位指针装入 %r8..%r11 后，
    #   又用 32 位形式（%r8d..）覆盖/递增它。特征：
    #     (1) mov  %e??,%rNd      —— 32 位写入一个通常承载指针的寄存器
    #     (2) inc/dec  %rNd       —— 指针按 32 位步进
    #   仅当同一函数里同时出现"32 位写入"与"用该寄存器解引用"才计为命中，
    #   以避免把普通的 32 位整数运算误报。
    $dis = & objdump -d $objPath 2>&1
    $hits = @()
    for ($i = 0; $i -lt $dis.Count; $i++) {
        $line = $dis[$i]
        # (1) 32 位写入 r8d..r11d
        if ($line -match 'mov\s+%e[a-z0-9]+,\s*%(r(?:8|9|10|11))d\b') {
            $reg = $Matches[1]
            # 收窄 (2026-09-23): 只有当该寄存器【先被 64 位装入过】才计为指针承载者。
            # 原版只看"32 位写入 + 后续解引用"，把 `%eax -> %r9d` 这类普通整数传参全算成截断 ⟹ 误报 100%。
            # 真截断的签名是: 先 `mov %r??,%r8` (指针装入) 再 `mov %e??,%r8d` (32 位覆盖) 再 `(%r8)` (解引用)。
            $isPtr = $false
            for ($k = [Math]::Max(0, $i - 10); $k -lt $i; $k++) {
                if ($dis[$k] -match "mov\s+%\w+,\s*%$reg\b") { $isPtr = $true; break }
            }
            if ($isPtr) {
                $lim = [Math]::Min($i + 30, $dis.Count - 1)
                for ($j = $i + 1; $j -le $lim; $j++) {
                    if ($dis[$j] -match "\(%$reg\)|\(%$reg,") {
                        $hits += ('[截断] ' + $line.Trim() + '  ...  ' + $dis[$j].Trim())
                        break
                    }
                }
            }
        }
        # (2) 32 位递增指针寄存器
        elseif ($line -match '\b(inc|dec)\s+%(r(?:8|9|10|11))d\b') {
            $reg = $Matches[2]
            # 收窄 (2026-09-23): 必须前文有 64 位装入 —— 否则 `inc %r11d` 是普通 32 位循环计数
            $isPtr = $false
            for ($k = [Math]::Max(0, $i - 10); $k -lt $i; $k++) {
                if ($dis[$k] -match "mov\s+%\w+,\s*%$reg\b|lea\s+[^,]+,\s*%$reg\b") { $isPtr = $true; break }
            }
            if ($isPtr) {
                $lim = [Math]::Min($i + 30, $dis.Count - 1)
                for ($j = $i + 1; $j -le $lim; $j++) {
                    if ($dis[$j] -match "\(%$reg\)|\(%$reg,") {
                        $hits += ('[32位步进] ' + $line.Trim() + '  ...  ' + $dis[$j].Trim())
                        break
                    }
                }
            }
        }
    }
    $res.Ptr32Hits = $hits.Count
    if ($hits.Count -gt 0) { $res.Ptr32Samp = $hits[0] }

    # ---- [C] 链接性 ----
    $dll = Join-Path $Work ($baseName + '.probe.dll')
    $ld = & gcc -w -shared -o $dll $objPath 2>&1
    $res.Link_OK = ($LASTEXITCODE -eq 0)
    if (-not $res.Link_OK) {
        $e = ($ld | Select-String -Pattern 'truncated|undefined|error' | Select-Object -First 1)
        if ($e) { $res.LinkErr = ($e.Line -replace '\s+',' ').Trim() } else { $res.LinkErr = (($ld | Select-Object -First 1) -replace '\s+',' ').Trim() }
        if ($res.LinkErr.Length -gt 90) { $res.LinkErr = $res.LinkErr.Substring(0,90) }
    }

    # ---- 判定 ----
    $res.Verdict = if ($res.Addr32 -gt 0 -and -not $res.Link_OK) { 'A:ADDR32阻断链接' }
                   elseif ($res.Ptr32Hits -gt 0)                 { 'B:PTR32危险模式' }
                   elseif (-not $res.Link_OK)                    { 'C:链接失败(其它)' }
                   else                                          { 'OK' }
    return [pscustomobject]$res
}

# ---------------------------------------------------------------- 主循环
$rows = @()
$i = 0
foreach ($f in $items) {
    $i++
    if ($i % 25 -eq 0) { Write-Host ("    ... $i/$($items.Count)") }

    $base = 'j{0:d5}' -f $i
    $c    = Join-Path $Work ($base + '.c')
    $gObj = Join-Path $Work ($base + '.gcc.obj')
    $jObj = Join-Path $Work ($base + '.jy.obj')

    Copy-Item -LiteralPath $f.FullName $c -Force

    # gcc 有效性对照（源本身无效的，不归因于甲言）
    & gcc -w -I $Work -c $c -o $gObj 2>&1 | Out-Null
    $gccOk = ($LASTEXITCODE -eq 0)

    # 甲言编译
    Push-Location $Work
    & $Qcc '-c' ($base + '.c') '-o' ($base + '.jy.obj') 2>&1 | Out-Null
    $jyOk = ($LASTEXITCODE -eq 0)
    Pop-Location

    $r = $null
    if ($jyOk -and (Test-Path $jObj)) {
        $r = Test-Obj $jObj $f.Name
        $r.Jiayan_OK = $true
        $r.Gcc_OK    = $gccOk
        if (-not $gccOk) { $r.Verdict = 'SRC:源本身gcc不过' }
    } else {
        $r = [pscustomobject][ordered]@{
            File=$f.Name; Jiayan_OK=$false; Gcc_OK=$gccOk; Addr32=0; Addr32Sec='';
            Ptr32Hits=0; Ptr32Samp=''; Link_OK=$false; LinkErr=''; Verdict='JY:甲言编译失败'
        }
    }
    $rows += $r

    if ($Verbose -and $r.Verdict -ne 'OK') {
        Write-Host ("  [{0}] {1}" -f $r.Verdict, $f.Name) -ForegroundColor Yellow
        if ($r.Addr32Sec) { Write-Host ("        ADDR32 段: " + $r.Addr32Sec) -ForegroundColor DarkGray }
        if ($r.Ptr32Samp) { Write-Host ("        PTR32: " + $r.Ptr32Samp) -ForegroundColor DarkGray }
        if ($r.LinkErr)   { Write-Host ("        链接: " + $r.LinkErr) -ForegroundColor DarkGray }
    }

    Remove-Item $c, $gObj, $jObj -Force -ErrorAction SilentlyContinue
}

# ---------------------------------------------------------------- 汇总
Write-Host ''
Write-Host '================================ 汇总 ================================'

$jyOkAll   = @($rows | Where-Object { $_.Jiayan_OK })
$gccOkAll  = @($rows | Where-Object { $_.Gcc_OK })
$bothOk    = @($rows | Where-Object { $_.Jiayan_OK -and $_.Gcc_OK })

Write-Host ("  目标总数                : " + $rows.Count)
Write-Host ("  gcc 编过（源有效）      : " + $gccOkAll.Count)
Write-Host ("  甲言编过                : " + $jyOkAll.Count)
Write-Host ("  源有效 且 甲言编过      : " + $bothOk.Count)
if ($gccOkAll.Count -gt 0) {
    Write-Host ("  甲言编译覆盖率          : " + [Math]::Round(100.0*$bothOk.Count/$gccOkAll.Count,1) + " %")
}
Write-Host ''

Write-Host '  ---------- 缺陷分布（在"甲言编过"的样本内）----------'
$groups = $bothOk | Group-Object Verdict | Sort-Object Count -Descending
foreach ($g in $groups) {
    $pct = if ($bothOk.Count -gt 0) { [Math]::Round(100.0*$g.Count/$bothOk.Count,1) } else { 0 }
    $color = if ($g.Name -eq 'OK') { 'Green' } else { 'Yellow' }
    Write-Host ("    {0,-22} {1,5}  ({2,5}%)" -f $g.Name, $g.Count, $pct) -ForegroundColor $color
}
Write-Host ''

$addrFiles = @($bothOk | Where-Object { $_.Addr32 -gt 0 })
$ptrFiles  = @($bothOk | Where-Object { $_.Ptr32Hits -gt 0 })
Write-Host ("  [A] 出现 ADDR32 重定位的模块 : " + $addrFiles.Count)
Write-Host ("  [B] 出现 PTR32 危险模式的模块 : " + $ptrFiles.Count)
Write-Host ("  [C] gcc 拒绝链接的模块       : " + @($bothOk | Where-Object { -not $_.Link_OK }).Count)
Write-Host ''
Write-Host ("  未识别到缺陷（OK）           : " + @($bothOk | Where-Object { $_.Verdict -eq 'OK' }).Count)
Write-Host ''

# ---- 落盘 ----
$csv = Join-Path $OutDir 'jyaudit_report.csv'
$rows | Export-Csv -Path $csv -NoTypeInformation -Encoding UTF8
$json = Join-Path $OutDir 'jyaudit_summary.json'
[pscustomobject]@{
    qcc        = (Split-Path $Qcc -Leaf)
    qcc_sha256 = $qccHash
    src        = $Src
    total      = $rows.Count
    gcc_ok     = $gccOkAll.Count
    jiayan_ok  = $jyOkAll.Count
    both_ok    = $bothOk.Count
    addr32     = $addrFiles.Count
    ptr32      = $ptrFiles.Count
    link_fail  = @($bothOk | Where-Object { -not $_.Link_OK }).Count
    ok         = @($bothOk | Where-Object { $_.Verdict -eq 'OK' }).Count
    verdicts   = @($groups | ForEach-Object { [pscustomobject]@{ verdict=$_.Name; count=$_.Count } })
} | ConvertTo-Json -Depth 4 | Set-Content -Path $json -Encoding UTF8

Write-Host '---------------- 报告 ----------------'
Write-Host "  CSV    : $csv"
Write-Host "  JSON   : $json"
Write-Host ''
Write-Host '  判据说明：本工具只报告可观测事实（重定位类型 / 指令形态 / 链接结果）。' -ForegroundColor DarkGray
Write-Host '            每条结论都可用 objdump -r / objdump -d / gcc -shared 独立复现。' -ForegroundColor DarkGray
Write-Host ''
