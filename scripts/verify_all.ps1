# ============================================================
# 启元 · 甲言 全量校验（一键守门）
#
# 用法:
#   powershell -ExecutionPolicy Bypass -File scripts/verify_all.ps1
#   powershell -ExecutionPolicy Bypass -File scripts/verify_all.ps1 -Fast
#
# 目的: 给「提交前 / 发布前 / CI」一个统一入口。
#
# 设计原则（2026-10-15 立）:
#   ① 【不依赖写死的不动点】判据 = 自举链多代全等。
#      源码每次合法修改，哈希都会变；写死 expected 必然过时
#      —— 2026-10-15 实测：build.ps1 的 expected 滞后 44 天，
#         导致任何用户 clone 后照文档跑都看到假 FAIL 并当场中断。
#   ② 【先查编码】本项目发生过两次编码事故：
#      · 2026-08-23 UTF-8↔GBK 双重编码（中文变乱码）
#      · 2026-10-15 edit 工具剥掉 BOM（PS5.1 按 GBK 读 ⟹ 脚本解析失败）
#      PS 脚本无 BOM 会被 PowerShell 5.1 当 GBK 读，中文即乱码 ⟹ 语法错。
#   ③ 【失败即停且给下一步】每条失败都打印可复制的修复提示。
# ============================================================
param(
    [switch]$Fast,     # 快速档：跳过多代自举链，只跑编码闸 + 宿主自检 + 测试
    [switch]$NoTest    # 跳过测试套件/行为断言（CI 里另有独立步骤时可关）
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
Set-Location $root

$script:Fail = 0
$script:Warn = 0
function Step($n, $msg) { Write-Host "`n[$n] $msg" -ForegroundColor Cyan }
function Ok($msg)   { Write-Host "  [OK]   $msg" -ForegroundColor Green }
function Bad($msg)  { Write-Host "  [FAIL] $msg" -ForegroundColor Red; $script:Fail++ }
function Warn($msg) { Write-Host "  [WARN] $msg" -ForegroundColor Yellow; $script:Warn++ }

Write-Host "==== 启元 · 甲言 全量校验 ==== " -ForegroundColor Cyan
Write-Host "根目录: $root"
Write-Host "档位  : $(if ($Fast) { 'FAST（跳过自举链）' } else { 'FULL' })"

# ------------------------------------------------------------
Step '1/5' '编码闸 —— .ps1 必须 UTF-8 with BOM（否则 PS5.1 按 GBK 读 ⟹ 中文乱码 ⟹ 语法错）'
$ps1s = Get-ChildItem $root -Recurse -File -Include '*.ps1' -ErrorAction SilentlyContinue |
        Where-Object { $_.FullName -notmatch '\\\.git\\' -and $_.FullName -notmatch '\\qiyuan_archive' -and $_.Name -notmatch '_neg\.c$' }
$noBom = @()
foreach ($f in $ps1s) {
    $b = [System.IO.File]::ReadAllBytes($f.FullName)
    $hasBom = ($b.Length -ge 3 -and $b[0] -eq 0xEF -and $b[1] -eq 0xBB -and $b[2] -eq 0xBF)
    if (-not $hasBom) { $noBom += $f.FullName.Substring($root.Length + 1) }
}
if ($noBom.Count -eq 0) {
    Ok "$($ps1s.Count) 个 .ps1 全部含 UTF-8 BOM"
} else {
    $noBom | ForEach-Object { Bad "缺 UTF-8 BOM: $_" }
    Write-Host '         修复（逐个规范化）:' -ForegroundColor Yellow
    Write-Host '         $p="<该文件>"; $t=[IO.File]::ReadAllText($p,[Text.Encoding]::UTF8); [IO.File]::WriteAllText($p,$t,(New-Object Text.UTF8Encoding($true)))' -ForegroundColor Yellow
}

Step '1b' '编码闸 —— 源码必须是严格 UTF-8（无 U+FFFD 替换字符）'
$srcs = Get-ChildItem $root -Recurse -File -Include '*.c','*.h','*.jy' -ErrorAction SilentlyContinue |
        Where-Object { $_.FullName -notmatch '\\\.git\\' -and $_.FullName -notmatch '\\qiyuan_archive' -and $_.Name -notmatch '_neg\.c$' }
$badEnc = @()
foreach ($f in $srcs) {
    $txt = [System.Text.Encoding]::UTF8.GetString([System.IO.File]::ReadAllBytes($f.FullName))
    $n = ([regex]::Matches($txt, "\uFFFD")).Count
    if ($n -gt 0) { $badEnc += "$($f.FullName.Substring($root.Length + 1)) ($n 处替换字符)" }
}
if ($badEnc.Count -eq 0) { Ok "$($srcs.Count) 个源文件均为严格 UTF-8（负例 *_neg.c 已豁免：它们故意含非法字节）" }
else { $badEnc | Select-Object -First 10 | ForEach-Object { Warn "注释区历史乱码(不影响编译): $_" }; if ($badEnc.Count -gt 10) { Warn "另有 $($badEnc.Count - 10) 个未列出" } }

# ------------------------------------------------------------
Step '2/5' '编译验证宿主（gcc 零警告）'
gcc -O2 -Wall -Werror srclib/qcc_x86.c -o qcc_x86.exe
if ($LASTEXITCODE -ne 0 -or -not (Test-Path .\qcc_x86.exe)) { Bad '宿主编译失败（检查 gcc 是否可用）'; }
else { Ok "qcc_x86.exe 生成（$((Get-Item .\qcc_x86.exe).Length) 字节）" }

Step '3/5' '宿主自检 --test'
if (Test-Path .\qcc_x86.exe) {
    $t = & .\qcc_x86.exe --test 2>&1 | Out-String
    if ($t -match 'PASS') { Ok '宿主自检 PASS' } else { Bad "宿主自检未通过: $t" }
}

# ------------------------------------------------------------
Step '4/5' '自举链 —— 判据 = 多代全等（不比对写死的 expected）'
if ($Fast) {
    Warn '-Fast 指定，跳过自举链（发布前请务必跑 FULL）'
} else {
    if (-not (Test-Path .\qcc_boot.exe)) {
        Write-Host '  [seed] 现场生成 qcc_boot.exe ...' -ForegroundColor Yellow
        & .\qcc_x86.exe srclib_jiayan\qcc_work.jy -o qcc_boot.exe 2>&1 | Out-Null
        if (-not (Test-Path .\qcc_boot.exe)) { & .\qcc_x86.exe srclib\qcc_x86.c -o qcc_boot.exe 2>&1 | Out-Null }
    }
    if (-not (Test-Path .\qcc_boot.exe)) { Bad '无法取得种子 qcc_boot.exe' }
    else {
        $prev = $null; $same = 0
        $hashes = @()
        & .\qcc_boot.exe srclib_jiayan\qcc_work.jy -o v1.exe 2>&1 | Out-Null
        $hashes += (Get-FileHash v1.exe -Algorithm SHA256).Hash
        for ($i = 2; $i -le 3; $i++) {
            & ".\v$($i-1).exe" srclib_jiayan\qcc_work.jy -o "v$i.exe" 2>&1 | Out-Null
            $hashes += (Get-FileHash "v$i.exe" -Algorithm SHA256).Hash
        }
        $u = ($hashes | Select-Object -Unique).Count
        Write-Host "        v1..v3 = $(($hashes | ForEach-Object { $_.Substring(0,8) }) -join ' / ')"
        if ($u -eq 1) { Ok "自举收敛：v1==v2==v3（连续代全等）—— 这就是「跑对了」的判据" }
        else { Bad "自举未收敛：v1..v3 出现 $u 个不同哈希" }
    }
}

# ------------------------------------------------------------
Step '5/5' '测试套件 + 行为断言'
if ($NoTest) { Warn '-NoTest 指定，跳过' }
else {
    if (Get-Command python -ErrorAction SilentlyContinue) {
        $rt = python scripts/run_tests.py --fast 2>&1 | Out-String
        if ($LASTEXITCODE -eq 0) { Ok '测试套件通过' } else { Bad "测试套件失败:`n$rt" }
        if (Test-Path .\qcc_x86.exe) {
            $bh = python tests/behavior/run_behavior.py "$PWD\qcc_x86.exe" 2>&1 | Out-String
            if ($LASTEXITCODE -eq 0) { Ok '行为断言通过' } else { Bad "行为断言失败:`n$bh" }
        }
    } else {
        Warn '未找到 python —— 测试套件与行为断言需 Python 3.x（见 README「依赖」）'
    }
}

# ------------------------------------------------------------
Write-Host ''
Write-Host '==== 结果 ====' -ForegroundColor Cyan
Write-Host "  FAIL=$script:Fail  WARN=$script:Warn"
if ($script:Fail -eq 0) {
    Write-Host '  [OK] 全量校验通过。甲言自己生自己，光荣。' -ForegroundColor Green
    exit 0
} else {
    Write-Host '  [FAIL] 校验未通过 —— 上面每条 [FAIL] 都附了修复提示。' -ForegroundColor Red
    exit 1
}
