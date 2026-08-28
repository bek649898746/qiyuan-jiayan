# ============================================================
# 锟斤拷元 锟斤拷 锟斤拷锟斤拷 一锟斤拷锟斤拷锟斤拷 + 锟皆撅拷锟斤拷证
# 锟矫凤拷: powershell -ExecutionPolicy Bypass -File scripts/build.ps1
# 锟斤拷锟斤拷: 锟斤拷锟斤拷锟斤拷锟斤拷 锟斤拷 锟皆撅拷锟斤拷锟斤拷 锟斤拷 校锟介不锟斤拷锟斤拷 锟斤拷 锟斤拷 171 锟斤拷锟斤拷
# ============================================================
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
Set-Location $root

Write-Host "==== 锟斤拷元 锟斤拷 锟斤拷锟斤拷 锟斤拷锟斤拷锟斤拷锟皆撅拷锟斤拷证 ====" -ForegroundColor Cyan

# 1. 锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷
Write-Host "[1/4] 锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷 qcc_x86..." -ForegroundColor Yellow
gcc -O2 -Wall -Werror srclib/qcc_x86.c -o qcc_x86.exe
if ($LASTEXITCODE -ne 0) { Write-Host "[FAIL] 锟斤拷锟斤拷锟斤拷锟斤拷失锟斤拷" -ForegroundColor Red; exit 1 }

# 2. 锟皆硷拷
Write-Host "[2/4] 锟斤拷锟斤拷锟皆硷拷 --test ..." -ForegroundColor Yellow
$t = & .\qcc_x86.exe --test
if ($t -notmatch 'PASS') { Write-Host "[FAIL] 锟皆硷拷失锟斤拷: $t" -ForegroundColor Red; exit 1 }

# 3. 锟皆撅拷锟斤拷锟? (v1==v2==v3==v4==v5 锟斤拷锟饺?锟斤拷; 2026-08-20: 锟斤拷锟斤拷锟斤拷锟斤拷 锟斤拷 锟斤拷锟斤拷 = qcc_boot.exe
#    锟斤拷锟皆举诧拷锟斤拷锟斤拷 27B42CE1, 一锟斤拷锟斤拷锟斤拷锟斤拷C 锟斤拷锟斤拷 qcc_x86.exe 锟斤拷锟斤拷锟斤拷源/锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷)
Write-Host "[3/4] 锟皆撅拷锟斤拷锟? (锟斤拷锟斤拷 qcc_boot.exe = 锟斤拷锟斤拷锟斤拷 27B42CE1) ..." -ForegroundColor Yellow
# 从零路径 (2026-08-24 封存: 仓库不含编译产物, 陌生人 clone 后需先自举生成种子)
if (-not (Test-Path .\qcc_boot.exe)) {
    Write-Host "  [seed] qcc_boot.exe 不存在, 用 C 宿主编译器自举生成种子..." -ForegroundColor Yellow
    & .\qcc_x86.exe srclib_jiayan\qcc_work.jy -o qcc_boot.exe | Out-Null
    if ($LASTEXITCODE -ne 0 -or -not (Test-Path .\qcc_boot.exe)) {
        & .\qcc_x86.exe srclib\qcc_x86.c -o qcc_boot.exe | Out-Null
    }
    if (-not (Test-Path .\qcc_boot.exe)) { Write-Host "[FAIL] 无法生成 qcc_boot.exe" -ForegroundColor Red; exit 1 }
    Write-Host "  [seed] qcc_boot.exe 生成完成" -ForegroundColor Green
}
& .\qcc_boot.exe srclib_jiayan\qcc_work.jy -o v1.exe | Out-Null
& .\v1.exe srclib_jiayan\qcc_work.jy -o v2.exe | Out-Null
& .\v2.exe srclib_jiayan\qcc_work.jy -o v3.exe | Out-Null
& .\v3.exe srclib_jiayan\qcc_work.jy -o v4.exe | Out-Null
& .\v4.exe srclib_jiayan\qcc_work.jy -o v5.exe | Out-Null

$h1 = (Get-FileHash v1.exe -Algorithm SHA256).Hash
$h2 = (Get-FileHash v2.exe -Algorithm SHA256).Hash
$h3 = (Get-FileHash v3.exe -Algorithm SHA256).Hash
$h4 = (Get-FileHash v4.exe -Algorithm SHA256).Hash
$h5 = (Get-FileHash v5.exe -Algorithm SHA256).Hash
Write-Host "  v1: $h1"
Write-Host "  v2: $h2"
Write-Host "  v3: $h3"
Write-Host "  v4: $h4"
Write-Host "  v5: $h5"

# 锟斤拷锟秸憋拷准: 锟皆举闭伙拷锟斤拷锟斤拷 (2026-08-22 t46: git branch/show-ref 全锟斤拷路锟斤拷锟睫革拷锟斤拷锟铰诧拷锟斤拷锟斤拷 锟斤拷
#            双指锟斤拷锟街段伙拷锟斤拷/锟斤拷锟斤拷锟斤拷锟斤拷 frow/mem_addr deref fsz/static struct 锟街段革拷值/锟斤拷锟斤拷 struct typedef 锟街讹拷;
#            锟斤拷锟斤拷 QCC_DBG_VA varargs 锟斤拷锟皆诧拷桩 锟斤拷 -Wall -Werror 锟姐警锟斤拷):
# qcc_boot == v1==v2==v3==v4==v5 = 6D8721FF (2026-08-23 t35j: const char *arr[] p_inner + %+d 锟斤拷锟斤拷锟斤拷)
$expected = '11bbcc6d'  # 后置回归修复 + 特性验证入自动链 (还技术债)  # 契约表可重入泄漏修复 + AI端到端 (真接入⑨)  # RAII真修复+机制层吃狗粮 (真接入⑥)  # 顶层契约污染修复 (真接入④)  # jyc客户端+精确修复建议 (真接入②③)  # serve真接入 (缓存共享+载荷模式, 真闭环)  # 编译期常量前置审判 (可吞清单 P1, 证明器查)  # 引用收集 --refs (可吞清单 P1, 导航补引用)  # 可重入修复+编译服务器 (nll残留根因, --repeat首vs次对账)  # 编译黑匣子 --recorder (可吞清单 P1, 分歧逐帧定位)  # 接口文件+语义导航 (函数定义行号, 可吞清单 P1)  # 接口文件 --interface (可吞清单 P1, MoonBit #4)  # 编译DAG重放证明 (可吞清单 P0-9, AI原生 #9 — P0 收官)  # 结构契约审判 契约哈希 (可吞清单 P0-8, AI原生 #8)  # 契约审判 前置/后置 (可吞清单 P0-7, AI原生 #7)  # 编译检查点 (可吞清单 P0-6, AI原生 #6: --checkpoint 崩溃尸检)  # 编译器资源护栏 (可吞清单 P0-9b, AI原生 #9b)  # 文件级哈希增量编译 (可吞清单 P0-5, AI原生 #5)  # 借用自动化+移入dst修复 (2026-08-28 完善第2项)  # loc完善: line真值+codeLine (大哥评估) 新不动点
if (($h1 -eq $h2) -and ($h2 -eq $h3) -and ($h3 -eq $h4) -and ($h4 -eq $h5) -and ($h1.Substring(0,8) -eq $expected)) {
    Write-Host "[OK] 锟皆撅拷 1-cycle 锟斤拷锟?: v1==v2==v3==v4==v5 = $($h1.Substring(0,8))" -ForegroundColor Green
} else {
    Write-Host "[FAIL] 哈希对账失败: expected=$expected 实际SHA256=$($h1.Substring(0,8)) (不动点/expected一律SHA256, 见_BOOT.md L74)" -ForegroundColor Red
    Write-Host "       对不上=不许通过! 跑完整链重新对账或修正expected" -ForegroundColor Red
    exit 1
}

# 4. AI原生 #5 (可吞清单 P0-5): 文件级哈希增量编译 — 不动点链复验 (增量产物==新鲜编译==不动点; --cache miss→hit→verify)
Write-Host "[3.5/5] 增量缓存不动点复验 (--cache + --cache-verify) ..." -ForegroundColor Yellow
New-Item -ItemType Directory -Force -Path scratch_test | Out-Null
Remove-Item scratch_test\_cache_fp.exe, scratch_test\_cache_fp.exe.qcckey, scratch_test\_cache_fp.err -ErrorAction SilentlyContinue
$h1l = $h1.ToLower()
# cmd /c 包原生 stderr: PS5.1 EAP=Stop 下原生 stderr 写 → NativeCommandError 终止 (2>&1/2>file 均触发), cmd 无此问题
cmd /c "qcc_x86.exe --cache --selfcheck $h1l srclib_jiayan\qcc_work.jy -o scratch_test\_cache_fp.exe 2>scratch_test\_cache_fp.err"
if ($LASTEXITCODE -ne 0) { Write-Host "[FAIL] 增量缓存 miss 编译失败" -ForegroundColor Red; exit 1 }
cmd /c "qcc_x86.exe --cache-verify --selfcheck $h1l srclib_jiayan\qcc_work.jy -o scratch_test\_cache_fp.exe 2>scratch_test\_cache_fp.err"
if ($LASTEXITCODE -ne 0) { Write-Host "[FAIL] 增量缓存 verify 失败" -ForegroundColor Red; exit 1 }
$cv = Get-Content scratch_test\_cache_fp.err -Raw
if ($cv -notmatch 'verify OK') { Write-Host "[FAIL] 增量缓存复验未过: $cv" -ForegroundColor Red; exit 1 }
Write-Host "[OK] 增量缓存不动点复验: 增量产物==新鲜编译==$($h1.Substring(0,8))" -ForegroundColor Green

# 4. 锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟阶硷拷锟斤拷锟斤拷锟斤拷+锟斤拷锟斤拷+锟斤拷锟斤拷, 锟斤拷锟斤拷 @EXPECTED compile_fail 锟斤拷 锟睫革拷原锟斤拷锟斤拷循锟斤拷撞锟斤拷锟斤拷锟斤拷锟绞э拷懿锟斤拷源锟斤拷锟? NativeCommandError锟斤拷
Write-Host "[4/4] 锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟阶硷拷 run_tests.py ..." -ForegroundColor Yellow
python scripts/run_tests.py
if ($LASTEXITCODE -ne 0) {
    Write-Host "[FAIL] 锟斤拷锟斤拷锟阶硷拷失锟斤拷" -ForegroundColor Red
    exit 1
}

# 5. 锟斤拷为锟斤拷锟皆ｏ拷fix 2026-08-09 实锟解复锟斤拷锟斤拷锟?: 锟斤拷前锟接诧拷锟斤拷锟斤拷锟斤拷为锟斤拷锟斤拷, 2 失锟斤拷锟斤拷锟斤拷知锟斤拷
Write-Host "[5/5] 锟斤拷锟斤拷为锟斤拷锟斤拷 tests/behavior ..." -ForegroundColor Yellow
python tests/behavior/run_behavior.py
if ($LASTEXITCODE -ne 0) {
    Write-Host "[WARN] 锟斤拷为锟斤拷锟斤拷失锟斤拷" -ForegroundColor Red
    exit 1
}

# 6. 锟斤拷锟斤拷锟斤拷
Write-Host "[+] 锟斤拷锟诫工锟斤拷锟斤拷 jyld / jycc ..." -ForegroundColor Yellow
# 5.5 特性验证 (还技术债 2026-08-28: 特性测试不入自动链 → 移入dst回归/RAII提前return泄漏漏网; 现在全部并入门禁)
Write-Host "[5.5/5] 特性验证 (18 个 _verify_*.py: 借用/契约/RAII/吃狗粮/端到端) ..." -ForegroundColor Yellow
$vfails = 0
Get-ChildItem scripts\_verify_*.py | ForEach-Object {
    python $_.FullName *> $null
    if ($LASTEXITCODE -ne 0) { Write-Host "  [FAIL] $($_.Name)" -ForegroundColor Red; $vfails++ }
}
if ($vfails -gt 0) { Write-Host "[FAIL] $vfails 个特性验证失败" -ForegroundColor Red; exit 1 }
Write-Host "[OK] 特性验证 18 个全过 (借用/契约/RAII/吃狗粮/端到端已入自动链)" -ForegroundColor Green
gcc -O2 -Wall -Werror srclib/jyld.c -o jyld.exe
gcc -O2 -Wall -Werror srclib/jycc.c -o jycc.exe

Write-Host ""
Write-Host "==== 锟斤拷伞锟斤拷锟斤拷锟斤拷约锟斤拷锟斤拷约锟斤拷锟斤拷锟斤拷佟锟? ====" -ForegroundColor Cyan
