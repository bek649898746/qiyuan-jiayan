# ============================================================
# ��Ԫ �� ���� һ������ + �Ծ���֤
# �÷�: powershell -ExecutionPolicy Bypass -File scripts/build.ps1
# ����: �������� �� �Ծ����� �� У�鲻���� �� �� 171 ����
# ============================================================
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
Set-Location $root

Write-Host "==== ��Ԫ �� ���� �������Ծ���֤ ====" -ForegroundColor Cyan

# 1. ��������������
Write-Host "[1/4] �������������� qcc_x86..." -ForegroundColor Yellow
gcc -O2 -Wall -Werror srclib/qcc_x86.c -o qcc_x86.exe
if ($LASTEXITCODE -ne 0) { Write-Host "[FAIL] ��������ʧ��" -ForegroundColor Red; exit 1 }

# 2. �Լ�
Write-Host "[2/4] �����Լ� --test ..." -ForegroundColor Yellow
$t = & .\qcc_x86.exe --test
if ($t -notmatch 'PASS') { Write-Host "[FAIL] �Լ�ʧ��: $t" -ForegroundColor Red; exit 1 }

# 3. �Ծ���� (v1==v2==v3==v4==v5 ���ȫ��; 2026-08-20: �������� �� ���� = qcc_boot.exe
#    ���Ծٲ����� 27B42CE1, һ��������C ���� qcc_x86.exe ������Դ/������������)
Write-Host "[3/4] �Ծ���� (���� qcc_boot.exe = ������ 27B42CE1) ..." -ForegroundColor Yellow
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

# ���ձ�׼: �Ծٱջ����� (2026-08-22 t46: git branch/show-ref ȫ��·���޸����²����� ��
#            ˫ָ���ֶλ���/�������� frow/mem_addr deref fsz/static struct �ֶθ�ֵ/���� struct typedef �ֶ�;
#            ���� QCC_DBG_VA varargs ���Բ�׮ �� -Wall -Werror �㾯��):
# qcc_boot == v1==v2==v3==v4==v5 = 6D8721FF (2026-08-23 t35j: const char *arr[] p_inner + %+d ������)
$expected = '7a3a414d'
if (($h1 -eq $h2) -and ($h2 -eq $h3) -and ($h3 -eq $h4) -and ($h4 -eq $h5) -and ($h1.Substring(0,8) -eq $expected)) {
    Write-Host "[OK] �Ծ� 1-cycle ���: v1==v2==v3==v4==v5 = $($h1.Substring(0,8))" -ForegroundColor Green
} else {
    Write-Host "[WARN] �Ծ�����ֿ��¼��ͬ��odd=$h1 even=$h2" -ForegroundColor Yellow
    Write-Host "       ����Դ���кϷ��޸ģ���Ϊ�µĲ����㣬����� README ��¼��"
}

# 4. �����������׼�������+����+����, ���� @EXPECTED compile_fail �� �޸�ԭ����ѭ��ײ�������ʧ�ܲ��Դ��� NativeCommandError��
Write-Host "[4/4] �����������׼� run_tests.py ..." -ForegroundColor Yellow
python scripts/run_tests.py
if ($LASTEXITCODE -ne 0) {
    Write-Host "[FAIL] �����׼�ʧ��" -ForegroundColor Red
    exit 1
}

# 5. ��Ϊ���ԣ�fix 2026-08-09 ʵ�⸴�����: ��ǰ�Ӳ�������Ϊ����, 2 ʧ������֪��
Write-Host "[5/5] ����Ϊ���� tests/behavior ..." -ForegroundColor Yellow
python tests/behavior/run_behavior.py
if ($LASTEXITCODE -ne 0) {
    Write-Host "[WARN] ��Ϊ����ʧ��" -ForegroundColor Red
    exit 1
}

# 6. ������
Write-Host "[+] ���빤���� jyld / jycc ..." -ForegroundColor Yellow
gcc -O2 -Wall -Werror srclib/jyld.c -o jyld.exe
gcc -O2 -Wall -Werror srclib/jycc.c -o jycc.exe

Write-Host ""
Write-Host "==== ��ɡ������Լ����Լ������١� ====" -ForegroundColor Cyan
