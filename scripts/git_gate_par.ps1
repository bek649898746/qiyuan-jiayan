# compile_git_par.ps1 -- PARALLEL git corpus gate (the 563-file sweep).
#
# ASCII only.
#
# Why: the 563-file corpus gate is 563 fully independent compile jobs, but the
# serial version uses one core.  This machine has 8 physical / 16 logical cores.
# Same work, N-way parallel -> should cut ~13.4 min down to ~2-3 min.
#
# Design: one file, two modes.
#   master (default) : enumerate -> shard round-robin -> spawn N workers -> merge
#   worker (-Worker) : compile the shard listed in -ListFile, write .ok/.fail/.crash
#
# Sharding is round-robin (not contiguous) so that a slow directory does not
# land entirely in one worker.
#
# Usage:
#   .\compile_git_par.ps1                       # 8 workers (default)
#   .\compile_git_par.ps1 -Jobs 12              # more workers
#   .\compile_git_par.ps1 -Limit 32 -Jobs 8     # smoke test

param(
    [string]$Qcc = 'C:\Users\Administrator\Desktop\qiyuan-jiayan\qcc_zh_seed.exe',
    [string]$Git = 'C:\Users\Administrator\Desktop\git-2.45.2',
    [string]$Pre = 'C:\Users\Administrator\Desktop\qiyuan-jiayan\compat_prelude.h',
    [string]$Out = 'D:\qiyuan_home\git_rebuild\objs_par',
    [int]$Jobs = 4,
    [int]$TimeoutSec = 240,
    [int]$Limit = 0,
    [switch]$Worker,
    [string]$ListFile = '',
    [string]$ResultFile = ''
)

$ErrorActionPreference = 'Continue'

# =====================================================================
# WORKER MODE
# =====================================================================
if ($Worker) {
    $dArgs = @('-D', '__STDC_VERSION__=199901L', '-D', '__GNUC__=5', '-D', '_WIN32=1', '-D', 'REG_STARTEND=1')
    $lines = @(Get-Content $ListFile -Encoding UTF8 | Where-Object { $_.Trim() })
    $rOk = New-Object System.Collections.ArrayList
    $rFail = New-Object System.Collections.ArrayList
    $rCrash = New-Object System.Collections.ArrayList

    foreach ($line in $lines) {
        $p = $line -split '\|', 2
        if ($p.Count -lt 2) { continue }
        $oname = $p[0]; $rel = $p[1]
        $src = Join-Path $Git ($rel -replace '/', '\')
        $dst = Join-Path $Out $oname

        $extra = @()
        if ($rel -eq 'compat/regex/regex.c') {
            $extra += @('-I', (Join-Path $Git 'compat\regex\regex.h'))
            $extra += @('-D', 'NO_MBSUPPORT')
        }
        $inc = @('-I', $Pre, '-I', (Join-Path $Git 'compat\regex'), '-I', $Git)
        $argv = @($dArgs) + $inc + $extra + @('-c', $src, '-o', $dst)
        $argLine = ($argv | ForEach-Object { if ($_ -match '\s') { '"' + $_ + '"' } else { $_ } }) -join ' '

        $psi = New-Object System.Diagnostics.ProcessStartInfo
        $psi.FileName = $Qcc
        $psi.Arguments = $argLine
        $psi.WorkingDirectory = $Git
        $psi.RedirectStandardOutput = $true
        $psi.RedirectStandardError = $true
        $psi.UseShellExecute = $false
        $psi.CreateNoWindow = $true

        $rc = -1; $so = ''; $se = ''
        try {
            $proc = [System.Diagnostics.Process]::Start($psi)
            $so = $proc.StandardOutput.ReadToEnd()
            $se = $proc.StandardError.ReadToEnd()
            if (-not $proc.WaitForExit($TimeoutSec * 1000)) { try { $proc.Kill() } catch { }; $rCrash.Add("$oname|TIMEOUT") | Out-Null; continue }
            $rc = $proc.ExitCode
        } catch {
            $rCrash.Add("$oname|EXC $($_.Exception.Message)") | Out-Null
            continue
        }

        if ($rc -eq 0 -and (Test-Path $dst)) {
            $rOk.Add($oname) | Out-Null
        } else {
            $msg = (($so + "`n" + $se).Trim() -replace "`r?`n", ' ')
            if ($msg.Length -gt 150) { $msg = $msg.Substring(0, 150) }
            if ($rc -lt 0 -or $rc -gt 1000000000) {
                $rCrash.Add(("{0}|rc=0x{1:X8}" -f $oname, ($rc -band 0xFFFFFFFF))) | Out-Null
            } else {
                $rFail.Add("$oname|$msg") | Out-Null
            }
        }
    }

    $rOk | Out-File "$ResultFile.ok" -Encoding UTF8
    $rFail | Out-File "$ResultFile.fail" -Encoding UTF8
    $rCrash | Out-File "$ResultFile.crash" -Encoding UTF8
    exit 0
}

# =====================================================================
# MASTER MODE
# =====================================================================
$T0 = Get-Date
if (-not (Test-Path $Qcc)) { Write-Host "FATAL: compiler not found: $Qcc"; exit 2 }
if (-not (Test-Path $Git)) { Write-Host "FATAL: git tree not found: $Git"; exit 2 }
if (-not (Test-Path $Pre)) { Write-Host "FATAL: prelude not found: $Pre"; exit 2 }
if (-not (Test-Path $Out)) { New-Item -ItemType Directory -Force -Path $Out | Out-Null }
Get-ChildItem $Out -Filter '*.o' -File -ErrorAction SilentlyContinue | Remove-Item -Force -ErrorAction SilentlyContinue

# --- enumerate (identical set to compile_git.ps1) ---
$coreDirs = @('', 'builtin', 'reftable', 'xdiff', 'ewah', 'refs', 'negotiator',
              'trace2', 'sha1dc', 'sha256', 'block-sha1')
$cands = @{}
foreach ($d in $coreDirs) {
    $base = if ($d -eq '') { $Git } else { Join-Path $Git $d }
    if (-not (Test-Path $base)) { continue }
    Get-ChildItem $base -Filter '*.c' -File -Recurse -ErrorAction SilentlyContinue | ForEach-Object {
        $rel = $_.FullName.Substring($Git.Length + 1).Replace('\', '/')
        $cands[(($rel -replace '\.c$', '') -replace '/', '_') + '.o'] = $rel
    }
}
$cbase = Join-Path $Git 'compat'
if (Test-Path $cbase) {
    Get-ChildItem $cbase -Filter '*.c' -File -Recurse -ErrorAction SilentlyContinue | ForEach-Object {
        $rel = $_.FullName.Substring($Git.Length + 1).Replace('\', '/')
        $cands[(($rel -replace '\.c$', '') -replace '/', '_') + '.o'] = $rel
    }
}

$skip = @('compat_regcomp_enhanced.o', 'compat_stub_procinfo.o',
          'compat_mmap.o', 'compat_win32mmap.o', 'compat_open.o',
          'daemon.o', 'http-backend.o', 'http-fetch.o', 'http-push.o',
          'imap-send.o', 'remote-curl.o', 'scalar.o', 'shell.o',
          'sh-i18n--envsubst.o', '_x2.o')
$platSuffix = @('-darwin.o', '-linux.o', '-macos.o', '-unix.o', '-apple.o')

$list = @()
foreach ($k in ($cands.Keys | Sort-Object)) {
    if ($k.StartsWith('_')) { continue }
    if ($skip -contains $k) { continue }
    if ($k.StartsWith('compat_linux_')) { continue }
    $isPlat = $false
    foreach ($s in $platSuffix) { if ($k.EndsWith($s)) { $isPlat = $true; break } }
    if ($isPlat) { continue }
    $list += ("{0}|{1}" -f $k, $cands[$k])
}
if ($Limit -gt 0) { $list = $list[0..([Math]::Min($Limit, $list.Count) - 1)] }

Write-Host "=================================================================="
Write-Host " PARALLEL GIT CORPUS GATE"
Write-Host "=================================================================="
Write-Host (" compiler : {0}  sha256-8={1}" -f $Qcc, (Get-FileHash $Qcc -Algorithm SHA256).Hash.Substring(0, 8))
Write-Host (" files    : {0}" -f $list.Count)
Write-Host (" workers  : {0}   (machine cores: {1})" -f $Jobs, (Get-CimInstance Win32_ComputerSystem).NumberOfLogicalProcessors)
Write-Host (" objects  : {0}" -f $Out)
Write-Host ""

# --- shard round-robin ---
$tmp = Join-Path (Split-Path $Out -Parent) ('_par_' + (Get-Date -Format 'HHmmss'))
New-Item -ItemType Directory -Force -Path $tmp | Out-Null
$self = $MyInvocation.MyCommand.Path
$procs = @()
for ($i = 0; $i -lt $Jobs; $i++) {
    $shard = @()
    for ($j = $i; $j -lt $list.Count; $j += $Jobs) { $shard += $list[$j] }
    if ($shard.Count -eq 0) { continue }
    $lf = Join-Path $tmp "shard_$i.lst"
    $rf = Join-Path $tmp "res_$i"
    $shard | Out-File $lf -Encoding UTF8
    $a = @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', $self,
           '-Worker', '-ListFile', $lf, '-ResultFile', $rf,
           '-Qcc', $Qcc, '-Git', $Git, '-Pre', $Pre, '-Out', $Out, '-TimeoutSec', "$TimeoutSec")
    $p = Start-Process powershell -ArgumentList $a -PassThru -WindowStyle Hidden
    $procs += $p
}
Write-Host (" started {0} workers; waiting..." -f $procs.Count)
$procs | Wait-Process -Timeout ($TimeoutSec * 4 + 600) -ErrorAction SilentlyContinue

# --- merge ---
$ok = @(); $fail = @(); $crash = @()
for ($i = 0; $i -lt $Jobs; $i++) {
    foreach ($ext in @('ok', 'fail', 'crash')) {
        $f = Join-Path $tmp ("res_{0}.{1}" -f $i, $ext)
        if (Test-Path $f) {
            $c = @(Get-Content $f -Encoding UTF8 | Where-Object { $_.Trim() })
            switch ($ext) {
                'ok' { $ok += $c }
                'fail' { $fail += $c }
                'crash' { $crash += $c }
            }
        }
    }
}

$el = ((Get-Date) - $T0).TotalSeconds
Write-Host ""
Write-Host "=================================================================="
Write-Host (" RESULT total={0}  OK={1}  FAIL={2}  CRASH={3}  ({4:N0}s  =  {5:N1} min)" -f `
    $list.Count, $ok.Count, $fail.Count, $crash.Count, $el, ($el / 60))
Write-Host "=================================================================="
if ($fail.Count -gt 0) {
    Write-Host "--- FAIL ---"
    $fail | Sort-Object | ForEach-Object { Write-Host ("  {0}" -f $_) }
}
if ($crash.Count -gt 0) {
    Write-Host "--- CRASH ---"
    $crash | Sort-Object | ForEach-Object { Write-Host ("  {0}" -f $_) }
}

$log = Join-Path (Split-Path $Out -Parent) 'compile_par_result.txt'
$out = @()
$out += ("parallel gate: jobs={0} total={1} OK={2} FAIL={3} CRASH={4} seconds={5:N0}" -f $Jobs, $list.Count, $ok.Count, $fail.Count, $crash.Count, $el)
$out += "--- FAIL ---"
$out += $fail
$out += "--- CRASH ---"
$out += $crash
$out | Out-File $log -Encoding UTF8
Write-Host (" log: {0}" -f $log)
