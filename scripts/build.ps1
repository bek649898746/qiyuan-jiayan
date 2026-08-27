# ============================================================
# ï¿½ï¿½Ôª ï¿½ï¿½ ï¿½ï¿½ï¿½ï¿½ Ò»ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ + ï¿½Ô¾ï¿½ï¿½ï¿½Ö¤
# ï¿½Ã·ï¿½: powershell -ExecutionPolicy Bypass -File scripts/build.ps1
# ï¿½ï¿½ï¿½ï¿½: ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ ï¿½ï¿½ ï¿½Ô¾ï¿½ï¿½ï¿½ï¿½ï¿½ ï¿½ï¿½ Ð£ï¿½é²»ï¿½ï¿½ï¿½ï¿½ ï¿½ï¿½ ï¿½ï¿½ 171 ï¿½ï¿½ï¿½ï¿½
# ============================================================
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
Set-Location $root

Write-Host "==== ï¿½ï¿½Ôª ï¿½ï¿½ ï¿½ï¿½ï¿½ï¿½ ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½Ô¾ï¿½ï¿½ï¿½Ö¤ ====" -ForegroundColor Cyan

# 1. ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½
Write-Host "[1/4] ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ qcc_x86..." -ForegroundColor Yellow
gcc -O2 -Wall -Werror srclib/qcc_x86.c -o qcc_x86.exe
if ($LASTEXITCODE -ne 0) { Write-Host "[FAIL] ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½Ê§ï¿½ï¿½" -ForegroundColor Red; exit 1 }

# 2. ï¿½Ô¼ï¿½
Write-Host "[2/4] ï¿½ï¿½ï¿½ï¿½ï¿½Ô¼ï¿½ --test ..." -ForegroundColor Yellow
$t = & .\qcc_x86.exe --test
if ($t -notmatch 'PASS') { Write-Host "[FAIL] ï¿½Ô¼ï¿½Ê§ï¿½ï¿½: $t" -ForegroundColor Red; exit 1 }

# 3. ï¿½Ô¾ï¿½ï¿½ï¿½ï¿? (v1==v2==v3==v4==v5 ï¿½ï¿½ï¿½È?ï¿½ï¿½; 2026-08-20: ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ ï¿½ï¿½ ï¿½ï¿½ï¿½ï¿½ = qcc_boot.exe
#    ï¿½ï¿½ï¿½Ô¾Ù²ï¿½ï¿½ï¿½ï¿½ï¿½ 27B42CE1, Ò»ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½C ï¿½ï¿½ï¿½ï¿½ qcc_x86.exe ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½Ô´/ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½)
Write-Host "[3/4] ï¿½Ô¾ï¿½ï¿½ï¿½ï¿? (ï¿½ï¿½ï¿½ï¿½ qcc_boot.exe = ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ 27B42CE1) ..." -ForegroundColor Yellow
# ´ÓÁãÂ·¾¶ (2026-08-24 ·â´æ: ²Ö¿â²»º¬±àÒë²úÎï, Ä°ÉúÈË clone ºóÐèÏÈ×Ô¾ÙÉú³ÉÖÖ×Ó)
if (-not (Test-Path .\qcc_boot.exe)) {
    Write-Host "  [seed] qcc_boot.exe ²»´æÔÚ, ÓÃ C ËÞÖ÷±àÒëÆ÷×Ô¾ÙÉú³ÉÖÖ×Ó..." -ForegroundColor Yellow
    & .\qcc_x86.exe srclib_jiayan\qcc_work.jy -o qcc_boot.exe | Out-Null
    if ($LASTEXITCODE -ne 0 -or -not (Test-Path .\qcc_boot.exe)) {
        & .\qcc_x86.exe srclib\qcc_x86.c -o qcc_boot.exe | Out-Null
    }
    if (-not (Test-Path .\qcc_boot.exe)) { Write-Host "[FAIL] ÎÞ·¨Éú³É qcc_boot.exe" -ForegroundColor Red; exit 1 }
    Write-Host "  [seed] qcc_boot.exe Éú³ÉÍê³É" -ForegroundColor Green
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

# ï¿½ï¿½ï¿½Õ±ï¿½×¼: ï¿½Ô¾Ù±Õ»ï¿½ï¿½ï¿½ï¿½ï¿½ (2026-08-22 t46: git branch/show-ref È«ï¿½ï¿½Â·ï¿½ï¿½ï¿½Þ¸ï¿½ï¿½ï¿½ï¿½Â²ï¿½ï¿½ï¿½ï¿½ï¿½ ï¿½ï¿½
#            Ë«Ö¸ï¿½ï¿½ï¿½Ö¶Î»ï¿½ï¿½ï¿½/ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ frow/mem_addr deref fsz/static struct ï¿½Ö¶Î¸ï¿½Öµ/ï¿½ï¿½ï¿½ï¿½ struct typedef ï¿½Ö¶ï¿½;
#            ï¿½ï¿½ï¿½ï¿½ QCC_DBG_VA varargs ï¿½ï¿½ï¿½Ô²ï¿½×® ï¿½ï¿½ -Wall -Werror ï¿½ã¾¯ï¿½ï¿½):
# qcc_boot == v1==v2==v3==v4==v5 = 6D8721FF (2026-08-23 t35j: const char *arr[] p_inner + %+d ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½)
$expected = '7b91b59c'
if (($h1 -eq $h2) -and ($h2 -eq $h3) -and ($h3 -eq $h4) -and ($h4 -eq $h5) -and ($h1.Substring(0,8) -eq $expected)) {
    Write-Host "[OK] ï¿½Ô¾ï¿½ 1-cycle ï¿½ï¿½ï¿?: v1==v2==v3==v4==v5 = $($h1.Substring(0,8))" -ForegroundColor Green
} else {
    Write-Host "[FAIL] ¹þÏ£¶ÔÕËÊ§°Ü: expected=$expected Êµ¼ÊSHA256=$($h1.Substring(0,8)) (²»¶¯µã/expectedÒ»ÂÉSHA256, ¼û_BOOT.md L74)" -ForegroundColor Red
    Write-Host "       ¶Ô²»ÉÏ=²»ÐíÍ¨¹ý! ÅÜÍêÕûÁ´ÖØÐÂ¶ÔÕË»òÐÞÕýexpected" -ForegroundColor Red
    exit 1
}

# 4. ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½×¼ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½+ï¿½ï¿½ï¿½ï¿½+ï¿½ï¿½ï¿½ï¿½, ï¿½ï¿½ï¿½ï¿½ @EXPECTED compile_fail ï¿½ï¿½ ï¿½Þ¸ï¿½Ô­ï¿½ï¿½ï¿½ï¿½Ñ­ï¿½ï¿½×²ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½Ê§ï¿½Ü²ï¿½ï¿½Ô´ï¿½ï¿½ï¿? NativeCommandErrorï¿½ï¿½
Write-Host "[4/4] ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½×¼ï¿½ run_tests.py ..." -ForegroundColor Yellow
python scripts/run_tests.py
if ($LASTEXITCODE -ne 0) {
    Write-Host "[FAIL] ï¿½ï¿½ï¿½ï¿½ï¿½×¼ï¿½Ê§ï¿½ï¿½" -ForegroundColor Red
    exit 1
}

# 5. ï¿½ï¿½Îªï¿½ï¿½ï¿½Ô£ï¿½fix 2026-08-09 Êµï¿½â¸´ï¿½ï¿½ï¿½ï¿½ï¿?: ï¿½ï¿½Ç°ï¿½Ó²ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½Îªï¿½ï¿½ï¿½ï¿½, 2 Ê§ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½Öªï¿½ï¿½
Write-Host "[5/5] ï¿½ï¿½ï¿½ï¿½Îªï¿½ï¿½ï¿½ï¿½ tests/behavior ..." -ForegroundColor Yellow
python tests/behavior/run_behavior.py
if ($LASTEXITCODE -ne 0) {
    Write-Host "[WARN] ï¿½ï¿½Îªï¿½ï¿½ï¿½ï¿½Ê§ï¿½ï¿½" -ForegroundColor Red
    exit 1
}

# 6. ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½
Write-Host "[+] ï¿½ï¿½ï¿½ë¹¤ï¿½ï¿½ï¿½ï¿½ jyld / jycc ..." -ForegroundColor Yellow
gcc -O2 -Wall -Werror srclib/jyld.c -o jyld.exe
gcc -O2 -Wall -Werror srclib/jycc.c -o jycc.exe

Write-Host ""
Write-Host "==== ï¿½ï¿½É¡ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½Ô¼ï¿½ï¿½ï¿½ï¿½Ô¼ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½Ù¡ï¿? ====" -ForegroundColor Cyan
