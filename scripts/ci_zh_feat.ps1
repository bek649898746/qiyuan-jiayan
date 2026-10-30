# ci_zh_feat.ps1 - CI 用：先用种子生成 v1，再用 v1 跑全量中文特性锚
#
# 为什么单独成文件（2026-10-25 启元审计，踩坑后修正）：
#   原先直接写在 .github/workflows/ci.yml 的 `run: |` 块里，里面含中文注释与中文输出。
#   GitHub Actions 会把该块写成**一个无 BOM 的临时 .ps1**，而 windows-latest runner 的
#   PowerShell 5.1 在英文 locale 下按 ANSI(CP1252) 解码 ⟹ 中文乱码 ⟹ ParserError:
#   UnexpectedToken ⟹ 步骤直接崩，连第一行都没执行到。
#   （同一坑 AGENTS.md 已记过一次：'ci.yml 里的中文被 GitHub 写成无 BOM 临时 .ps1'）
#
#   ⟹ 铁律：**ci.yml 的 run: 块里不写任何中文**；要中文逻辑就抽成本文件
#      （本文件存为 UTF-8 with BOM，PS5.1 才能正确解码）。
#
# 为什么需要「先生成 v1」而不是直接用种子：
#   种子是上个里程碑的编译器（不动点随主源推进而变），主源已是新版本。
#   实测：同一锚 feat_zh_specialization50.c
#     用 v1（当前主源产物）-> PASS
#     用种子（旧编译器）    -> FAIL
#   ⟹ 用不配套的编译器测锚会**假红**，不是锚坏了。

$ErrorActionPreference = 'Continue'

function Fail($msg) {
    Write-Host "::error::$msg"
    exit 1
}

# ---------- [1/2] 用种子生成 v1 ----------
Write-Host "[1/2] Building v1 from seed (this is the product of current main source)..."
$sw = [Diagnostics.Stopwatch]::StartNew()
& .\qcc_zh_seed.exe srclib_jiayan\qcc_zh.jy -o qcc_zh_v1.exe 2>&1 | Write-Host
$rc1 = $LASTEXITCODE
$sw.Stop()

if ($rc1 -ne 0) {
    Fail "v1 generation failed (rc=$rc1) - bootstrap chain first generation is broken"
}
if (-not (Test-Path 'qcc_zh_v1.exe')) {
    Fail "v1 generation produced no qcc_zh_v1.exe (rc=$rc1)"
}

$v1sha = (Get-FileHash 'qcc_zh_v1.exe' -Algorithm SHA256).Hash.Substring(0,8)
Write-Host "[1/2] v1 = $v1sha  ($([math]::Round($sw.Elapsed.TotalSeconds,1))s)"

# ---------- [2/2] 用 v1 跑全量中文特性锚 ----------
#
# ⚠️ 必须**显式**传 --parallel（2026-10-26 启元审计 · CI zh_chain 红的第二个根因）
#   run_tests_zh.py 的 --parallel 默认 6，内存守卫算 6 × 1.5GB = 9.0GB，
#   要求 (可用物理 - 6GB 预留) >= 9.0GB。
#   windows-latest 可用物理约 13.3GB ⟹ 13.3 - 6.0 = 7.3GB < 9.0GB
#   ⟹ 守卫 **响亮拒绝**（exit 2）⟹ 1,447 个锚 **一个都没跑**，job 却报「失败」。
#   这不是放宽判据：4 路 = runner 的 vCPU 数，预算 6.0GB，守卫逻辑照旧生效。
$PARALLEL = 4
Write-Host "[2/2] Running full zh-feat anchor suite with v1 (parallel=$PARALLEL)..."
$env:QCC_TEST_BIN = (Join-Path $PWD 'qcc_zh_v1.exe')
$env:PYTHONIOENCODING = 'utf-8'
Write-Host "[2/2] QCC_TEST_BIN = $env:QCC_TEST_BIN"

$log = Join-Path $PWD '_ci_zh_feat.log'
python scripts/run_tests_zh.py --zh-feat --parallel $PARALLEL *> $log
$rc2 = $LASTEXITCODE
Get-Content $log -Encoding UTF8 -ErrorAction SilentlyContinue | Write-Host
Write-Host "[2/2] anchor suite rc=$rc2"

if ($rc2 -ne 0) {
    # 把失败原因**外化**成 annotation。
    # 判据行（[FAIL] / ❌ / [MEM] 拒绝 / [SKIP]）走公开 API 匿名可读，
    # 审计方不必再要 token 才能判断红在哪一关 —— 这正是上一轮把 verify_90s
    # 改成 ::error:: 外化的同一条理由，本轮轮到本文件。
    $bad = (Get-Content $log -Encoding UTF8 -ErrorAction SilentlyContinue |
            Where-Object { $_ -match '\[FAIL\]|❌|\[MEM\] 拒绝|\[SKIP\] 环境' } |
            Select-Object -First 4) -join ' ~ '
    if (-not $bad) {
        $bad = (Get-Content $log -Encoding UTF8 -ErrorAction SilentlyContinue |
                Where-Object { $_.Trim() -ne '' } | Select-Object -Last 4) -join ' ~ '
    }
    Write-Host "::error::zh-feat anchor suite failed rc=$rc2 :: $bad"
}
exit $rc2
