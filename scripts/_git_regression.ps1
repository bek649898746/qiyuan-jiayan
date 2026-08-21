# 完整回归: git_jiayan_v1.exe vs 原生 git 2.47 (测试仓库 _smoke_final_ok)
$GJ = "C:\Users\Administrator\Desktop\git-2.45.2\git_jiayan_v1.exe"
$NG = "git"   # 原生 2.47.1
$repo = "C:\Users\Administrator\Desktop\_smoke_final_ok"
$pass = 0; $fail = 0

function Compare-Out {
    param([string]$label, [string[]]$cmdargs)
    $a = & $GJ @cmdargs 2>$null
    $ea = $LASTEXITCODE
    $b = & $NG @cmdargs 2>$null
    $eb = $LASTEXITCODE
    if (($ea -eq 0) -and ($eb -eq 0) -and ($a -join "`n" -eq $b -join "`n")) {
        Write-Host "PASS  $label" -ForegroundColor Green
        $script:pass++
    } else {
        Write-Host "FAIL  $label  (GJ exit=$ea NG exit=$eb)" -ForegroundColor Red
        $diff = Compare-Object $a $b
        $diff | Select-Object -First 8 | ForEach-Object { Write-Host "      diff: $($_.SideIndicator) $($_.InputObject)" }
        $script:fail++
    }
}

Set-Location $repo

# 基础格式
Compare-Out "default log" @("log","-1")
Compare-Out "fuller" @("log","--format=fuller","-1")
Compare-Out "email" @("log","--format=email","-1")
Compare-Out "medium" @("log","--format=medium","-1")
Compare-Out "oneline" @("log","--format=oneline","-1")
Compare-Out "short" @("log","--format=short","-1")

# 日期占位符
Compare-Out "%ad" @("log","--format=%h %ad","-1")
Compare-Out "%cI" @("log","--format=%h %cI","-1")
Compare-Out "%aI" @("log","--format=%h %aI","-1")
Compare-Out "%aD" @("log","--format=%h %aD","-1")
Compare-Out "%cr" @("log","--format=%h %cr","-1")
Compare-Out "%cD" @("log","--format=%h %cD","-1")
Compare-Out "%at" @("log","--format=%h %at","-1")

# --date= 模式
Compare-Out "--date=iso" @("log","--date=iso","-1")
Compare-Out "--date=raw" @("log","--date=raw","-1")
Compare-Out "--date=short" @("log","--date=short","-1")
Compare-Out "--date=unix" @("log","--date=unix","-1")
Compare-Out "--date=human" @("log","--date=human","-1")
Compare-Out "--date=relative" @("log","--date=relative","-1")
Compare-Out "--date=rfc" @("log","--date=rfc","-1")

# 组合/复杂
Compare-Out "%w(50,4,4)" @("log","--format=%w(50,4,4)commit %h%n%an <%ae>%n%ad%n%b","-1")
Compare-Out "cr+ad" @("log","--format=%cr | %ad","-1")
Compare-Out "an ae b s" @("log","--format=%an <%ae> %b %s","-1")

# 其他命令
Compare-Out "rev-parse HEAD" @("rev-parse","HEAD")
Compare-Out "cat-file -p HEAD" @("cat-file","-p","HEAD")

Write-Host ""
Write-Host "==== 回归结果: PASS=$pass FAIL=$fail ====" -ForegroundColor Cyan
if ($fail -gt 0) { exit 1 }
