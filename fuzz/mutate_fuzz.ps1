# mutate_fuzz.ps1 — 甲言编译器变异 fuzzer（真跑版本）
#
# 为什么有它：Windows runner 上 libFuzzer 支持不稳，而 fuzz 的价值在「真跑真喂」。
#   本脚本用确定性的变异算子 + fuzz_compile.c 的 standalone 驱动，真调编译器、真判崩溃。
#   libFuzzer 形态的 harness 同时在 fuzz/fuzz_compile.c 里保留（Linux/macOS 或支持 libFuzzer
#   的环境可直接：clang -fsanitize=fuzzer fuzz/fuzz_compile.c -o fuzzbin）。
#
# 判据（与 harness 一致）：
#   harness 退出 0  = 编译器正常结束（含「报诊断并非零退出」这种正确行为）
#   harness 退出 ≠0 = 捕获到缺陷（崩溃 / 超时），样本会落到 -CrashDir
#
# 用法：
#   powershell -File fuzz/mutate_fuzz.ps1 -Rounds 300
#   powershell -File fuzz/mutate_fuzz.ps1 -Rounds 300 -Seed 7 -CrashDir fuzz/crashes
#
# 环境变量：
#   QCC_FUZZ_COMPILER  被测编译器（默认仓库根的 qcc_zh_seed.exe）

[CmdletBinding()]
param(
    [int]$Rounds = 200,
    [int]$Seed = 20261104,
    [string]$CorpusDir = 'fuzz/corpus',
    [string]$CrashDir = 'fuzz/crashes',
    [string]$Harness = 'fuzz/fuzz_standalone.exe',
    [string]$Gcc = ''
)

$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot

function Write-Step($m) { Write-Host "[fuzz] $m" }

# ── 定位编译器 ──────────────────────────────────────────────
if (-not $env:QCC_FUZZ_COMPILER) {
    $cand = Join-Path $repo 'qcc_zh_seed.exe'
    if (Test-Path $cand) { $env:QCC_FUZZ_COMPILER = $cand }
}
if (-not $env:QCC_FUZZ_COMPILER -or -not (Test-Path $env:QCC_FUZZ_COMPILER)) {
    Write-Host "[fuzz] 找不到被测编译器（QCC_FUZZ_COMPILER 未设置且仓库根无 qcc_zh_seed.exe）" -ForegroundColor Red
    exit 2
}
Write-Step "被测编译器: $env:QCC_FUZZ_COMPILER"

# ── 准备 harness（没有就现编）───────────────────────────────
$harnessPath = if ([IO.Path]::IsPathRooted($Harness)) { $Harness } else { Join-Path $repo $Harness }
if (-not (Test-Path $harnessPath)) {
    if (-not $Gcc) {
        foreach ($c in @('gcc.exe', 'C:\mingw-w64\bin\gcc.exe', 'C:\mingw64\bin\gcc.exe')) {
            $g = Get-Command $c -ErrorAction SilentlyContinue
            if ($g) { $Gcc = $g.Source; break }
            if (Test-Path $c) { $Gcc = $c; break }
        }
    }
    if (-not $Gcc -or -not (Test-Path $Gcc)) {
        Write-Host "[fuzz] 找不到 gcc，无法构建 harness" -ForegroundColor Red
        exit 2
    }
    Write-Step "构建 harness（$Gcc）"
    $srcC = Join-Path $repo 'fuzz/fuzz_compile.c'
    $oldEap = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    & $Gcc -O2 -Wall -Wextra -DFUZZ_STANDALONE $srcC -o $harnessPath 2>&1 | Out-Null
    $gccrc = $LASTEXITCODE
    $ErrorActionPreference = $oldEap
    if ($gccrc -ne 0) { Write-Host "[fuzz] harness 构建失败" -ForegroundColor Red; exit 2 }
}
Write-Step "harness: $harnessPath"

# ── 种子 ────────────────────────────────────────────────────
$corpusPath = if ([IO.Path]::IsPathRooted($CorpusDir)) { $CorpusDir } else { Join-Path $repo $CorpusDir }
$seeds = @(Get-ChildItem $corpusPath -Filter '*.c' -File -ErrorAction SilentlyContinue)
if ($seeds.Count -eq 0) { Write-Host "[fuzz] 语料目录为空: $corpusPath" -ForegroundColor Red; exit 2 }
Write-Step "种子 $($seeds.Count) 个，轮数 $Rounds，随机种子 $Seed"

$crashPath = if ([IO.Path]::IsPathRooted($CrashDir)) { $CrashDir } else { Join-Path $repo $CrashDir }
if (-not (Test-Path $crashPath)) { New-Item -ItemType Directory -Force -Path $crashPath | Out-Null }

# ── 变异算子 ────────────────────────────────────────────────
$tokens = @('{', '}', '(', ')', '[', ']', '"', "'", '*/', '/*', '#define ', '#if ', '#include <', 'struct ', 'union ',
            'enum ', 'goto ', 'static ', 'sizeof(', '->', '...', '0x', '\0', '中文', '：', '；')
$rng = New-Object System.Random $Seed

function Invoke-Mutate([byte[]]$buf) {
    $b = [System.Collections.Generic.List[byte]]::new()
    $b.AddRange($buf)
    $ops = $rng.Next(1, 7)
    for ($k = 0; $k -lt $ops; $k++) {
        if ($b.Count -eq 0) { $b.Add([byte]65); continue }
        switch ($rng.Next(0, 7)) {
            0 { # 翻转单字节
                $i = $rng.Next(0, $b.Count); $b[$i] = [byte]$rng.Next(0, 256)
            }
            1 { # 删除片段
                $i = $rng.Next(0, $b.Count); $n = [Math]::Min($rng.Next(1, 32), $b.Count - $i)
                if ($n -gt 0) { $b.RemoveRange($i, $n) }
            }
            2 { # 插入随机字节
                $i = $rng.Next(0, $b.Count + 1)
                for ($j = 0; $j -lt $rng.Next(1, 16); $j++) { $b.Insert($i, [byte]$rng.Next(0, 256)) }
            }
            3 { # 插入 C 记号
                $i = $rng.Next(0, $b.Count + 1)
                $tk = [Text.Encoding]::UTF8.GetBytes($tokens[$rng.Next(0, $tokens.Count)])
                $b.InsertRange($i, [byte[]]$tk)
            }
            4 { # 截断
                if ($b.Count -gt 2) { $keep = $rng.Next(1, $b.Count); $b.RemoveRange($keep, $b.Count - $keep) }
            }
            5 { # 复制片段
                $i = $rng.Next(0, $b.Count); $n = [Math]::Min($rng.Next(1, 48), $b.Count - $i)
                if ($n -gt 0) { $seg = $b.GetRange($i, $n).ToArray(); $b.InsertRange($rng.Next(0, $b.Count + 1), $seg) }
            }
            6 { # 注入 NUL 与高字节（编码边界）
                $i = $rng.Next(0, $b.Count + 1); $b.Insert($i, [byte]0)
                $i2 = $rng.Next(0, $b.Count + 1); $b.Insert($i2, [byte]0xEF)
            }
        }
    }
    return $b.ToArray()
}

# ── 主循环 ──────────────────────────────────────────────────
$tmp = Join-Path ([IO.Path]::GetTempPath()) ("qccfuzzcase_{0}.c" -f $PID)
$found = 0
$sw = [Diagnostics.Stopwatch]::StartNew()

for ($r = 1; $r -le $Rounds; $r++) {
    $seedFile = $seeds[$rng.Next(0, $seeds.Count)]
    $data = [IO.File]::ReadAllBytes($seedFile.FullName)
    $mut = Invoke-Mutate $data
    [IO.File]::WriteAllBytes($tmp, $mut)

    # 用 .NET Process 显式启动：原生程序崩溃时不会触发 PowerShell 的 NativeCommandError
    # （$ErrorActionPreference='Stop' 下，直接 & 调用会把 stderr 当错误抛并中止脚本 —— 那样样本就落不了盘）
    $psi = New-Object System.Diagnostics.ProcessStartInfo
    $psi.FileName = $harnessPath
    $psi.Arguments = '"' + $tmp + '"'
    $psi.UseShellExecute = $false
    $psi.RedirectStandardOutput = $true
    $psi.RedirectStandardError = $true
    $psi.CreateNoWindow = $true
    $proc = [System.Diagnostics.Process]::Start($psi)
    $null = $proc.StandardOutput.ReadToEnd()
    $null = $proc.StandardError.ReadToEnd()
    $proc.WaitForExit()
    $code = $proc.ExitCode

    if ($code -ne 0 -and $code -ne 2) {
        $found++
        $name = "crash_{0}_round{1}_seed={2}.c" -f (Get-Date -Format 'HHmmss'), $r, [IO.Path]::GetFileNameWithoutExtension($seedFile.Name)
        $dst = Join-Path $crashPath $name
        [IO.File]::WriteAllBytes($dst, $mut)
        Write-Host ("[{0}/{1}] 捕获缺陷 exit={2} -> {3}" -f $r, $Rounds, $code, $name) -ForegroundColor Red
    }
    if ($r % 25 -eq 0) {
        Write-Step ("进度 {0}/{1} · 已捕获 {2} · 用时 {3:N0}s" -f $r, $Rounds, $found, $sw.Elapsed.TotalSeconds)
    }
}
if (Test-Path $tmp) { Remove-Item $tmp -Force -ErrorAction SilentlyContinue }
$sw.Stop()

Write-Step ("完成：{0} 轮 / {1:N1}s（{2:N0} ms/轮）· 捕获缺陷 {3} 个" -f $Rounds, $sw.Elapsed.TotalSeconds, ($sw.Elapsed.TotalMilliseconds / [Math]::Max(1, $Rounds)), $found)
if ($found -gt 0) {
    Write-Host "[fuzz] 有缺陷样本落在: $crashPath —— 复现：QCC_FUZZ_COMPILER=<编译器> <harness> <样本>" -ForegroundColor Yellow
    exit 1
}
Write-Host '[fuzz] 本轮未捕获崩溃/超时' -ForegroundColor Green
exit 0
