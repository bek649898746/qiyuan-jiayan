# gen_site_docs.ps1 — 生成文档站源目录（只含文档，不含 exe / 语料 / .git）
# 用途：把 manual/ 与顶层关键文档汇集到 site_docs/，供 mkdocs build 使用
# 不改任何产品代码，只读仓库里的 .md 与 assets/

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
Set-Location $root

$dst = Join-Path $root 'site_docs'
if (Test-Path $dst) { Remove-Item $dst -Recurse -Force }
New-Item -ItemType Directory -Force -Path $dst | Out-Null

# 首页
Copy-Item 'SITE_HOME.md' (Join-Path $dst 'index.md') -Force

# manual/（排除内部材料 OpenSSF_*）
$mdst = Join-Path $dst 'manual'
New-Item -ItemType Directory -Force -Path $mdst | Out-Null
Get-ChildItem 'manual' -File -Filter '*.md' |
    Where-Object { $_.Name -notlike 'OpenSSF_*' } |
    ForEach-Object { Copy-Item $_.FullName (Join-Path $mdst $_.Name) -Force }

# 顶层对外文档（含 GitHub 展示用的 README，避免站内相对链接断掉）
$top = @(
    'README.md', 'README_WHY.md', 'README_DEV.md', 'SUPPORT_MATRIX.md',
    'KNOWN_ISSUES_v1.0.md', 'WHITE_PAPER_EN.md', 'GIT_RUN_ADAPTATIONS.md',
    'SECURITY.md', 'CONTRIBUTING.md', 'CODE_OF_CONDUCT.md', 'CHANGELOG.md',
    'llms.txt', 'llms-full.txt', 'LICENSE', 'LICENSE-LANG.txt'
)
foreach ($f in $top) {
    if (Test-Path $f) { Copy-Item $f (Join-Path $dst $f) -Force }
}

# jylib 标准库说明（文档里引用了 jylib/README.md）
if (Test-Path 'jylib/README.md') {
    New-Item -ItemType Directory -Force -Path (Join-Path $dst 'jylib') | Out-Null
    Copy-Item 'jylib/README.md' (Join-Path $dst 'jylib/README.md') -Force
}

# 截图
if (Test-Path 'assets') { Copy-Item 'assets' (Join-Path $dst 'assets') -Recurse -Force }

$n = (Get-ChildItem $dst -Recurse -File | Measure-Object).Count
Write-Host "[OK] site_docs 已生成：$n 个文件"
