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
Write-Host "[2/2] Running full zh-feat anchor suite with v1..."
$env:QCC_TEST_BIN = (Join-Path $PWD 'qcc_zh_v1.exe')
Write-Host "[2/2] QCC_TEST_BIN = $env:QCC_TEST_BIN"

python scripts/run_tests_zh.py --zh-feat
$rc2 = $LASTEXITCODE
Write-Host "[2/2] anchor suite rc=$rc2"
exit $rc2
