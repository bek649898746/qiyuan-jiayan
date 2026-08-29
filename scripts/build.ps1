# ============================================================
#                                                   +                 
#         : powershell -ExecutionPolicy Bypass -File scripts/build.ps1
#             :                                                                                                171             
# ============================================================
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
Set-Location $root

Write-Host "====                                                                 ====" -ForegroundColor Cyan

# 1.                                           
Write-Host "[1/4]                                            qcc_x86..." -ForegroundColor Yellow
gcc -O2 -Wall -Werror srclib/qcc_x86.c -o qcc_x86.exe
if ($LASTEXITCODE -ne 0) { Write-Host "[FAIL]                                 " -ForegroundColor Red; exit 1 }

# 2.         
Write-Host "[2/4]                      --test ..." -ForegroundColor Yellow
$t = & .\qcc_x86.exe --test
if ($t -notmatch 'PASS') { Write-Host "[FAIL]                 : $t" -ForegroundColor Red; exit 1 }

# 3.                  ? (v1==v2==v3==v4==v5             ?      ; 2026-08-20:                                              = qcc_boot.exe
#                                 27B42CE1,                           C              qcc_x86.exe                     /                                    )
Write-Host "[3/4]                  ? (             qcc_boot.exe =                    27B42CE1) ..." -ForegroundColor Yellow
#                      (2026-08-24          :                                    ,                clone                                                   )
if (-not (Test-Path .\qcc_boot.exe)) {
    Write-Host "  [seed] qcc_boot.exe                   ,        C                                                               ..." -ForegroundColor Yellow
    & .\qcc_x86.exe srclib_jiayan\qcc_work.jy -o qcc_boot.exe | Out-Null
    if ($LASTEXITCODE -ne 0 -or -not (Test-Path .\qcc_boot.exe)) {
        & .\qcc_x86.exe srclib\qcc_x86.c -o qcc_boot.exe | Out-Null
    }
    if (-not (Test-Path .\qcc_boot.exe)) { Write-Host "[FAIL]                      qcc_boot.exe" -ForegroundColor Red; exit 1 }
    Write-Host "  [seed] qcc_boot.exe                      " -ForegroundColor Green
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

#                 :                          (2026-08-22 t46: git branch/show-ref                                                          
#                                      /                         frow/mem_addr deref fsz/static struct             /             struct typedef         ;
#                         QCC_DBG_VA varargs                         -Wall -Werror             ):
# qcc_boot == v1==v2==v3==v4==v5 = 6D8721FF (2026-08-23 t35j: const char *arr[] p_inner + %+d                   )
$expected = '39722ddd'  # 2026-08-29 P2 batch11: bounds-check (越界检查 a,i,n → if(i<0||i>=n) printf+exit1, stmt-front branch + blk exclusion)  # 2026-08-28 P2 batch10: ref-pattern (对照 引用 v → arm-local binding, guard inside block)  # 2026-08-28 P2 batch9: valued break (break 标签, 值 → __brk_标签 = 值; goto)  # 2026-08-28 P2 batch8: deref overload (重载 * 类型 = fn → *obj) + call operator overload (重载 () 类型 = fn → obj(args))  # 2026-08-28 P2 batch5: zero-size type (空类型 T sizeof=0) + doc collection (文档/文档数)  # 2026-08-28 P2 batch4: target-arch detection (目标架构/目标字长) + source location (源位置 line const)  # 2026-08-28 P2 batch3: panic hook (恐慌钩子 fn → panic先调钩子) + range-pattern (对照 范围 lo 到 hi)  # 2026-08-28 P2 batch2: Cell interior mutability (单元/放单元/取单元 runtime cell bypassing borrow)  # 2026-08-28 P2 batch1: forget (忘记 x → dtor skipped + moved, mem::forget)  # 2026-08-28 P1 string group: dynamic array Vec (动态数组/追加/长度/取元素 global-table grow)  # 2026-08-28 P1 operator group: compound-assign overload (a op= b → 重载函数(a,b))  # 2026-08-28 P1 interop group: dynamic library loading (kernel32 IAT 15→17 LoadLibraryA/GetProcAddress + 加载库/取函数 + msvcrt slot shift 17+base)  # 2026-08-28 P1 layout group: memory pool via global-table (no struct, avoids top-level struct-parse residue bug) + arena/refl/bitfield/union eval locks  # 2026-08-28 P1 func group: immutable locals (不变 x=expr → assign A9006) + pipe/HOF/map/comprehension eval locks  # 2026-08-28 P1 oop group: iface default methods (接口 名 { 方法 = 默认函数 } → impl fallback)  # 2026-08-28 P1 meta group: compile-time assert (编译期断言 expr → CTFE check) + macro/CTFE/generic eval locks  # 2026-08-28 P1 control group: labeled loops (标签: while + break/continue 标签) via goto-wrapper  # 2026-08-28 P1 pattern group: or-pattern (v1|v2) + guard-pattern (v 若 cond) + redundant-pattern check (matched-flag)  # 2026-08-28 P1 type group: local type inference (自动 x=expr) infer int/double/ll/char/ptr + ADT/recursive-type/cast eval locks  # 2026-08-28 P1 error group: panic (恐慌 msg → printf+exit1) + error-code/rwlock/exception/assert/determinism eval locks  # 2026-08-28 P1 concurrency full: + barrier (栅栏/汇合) multi-thread rendezvous  # 2026-08-28 P1 concurrency: user-mode sem/rwlock/condvar (futex-style) + top-level sync-decl global handler  # 2026-08-28 P1 borrow group: split-borrow field/elem paths + closure capture modes (by-move/by-mut) + member-path read check (lv_path buffer fix)  # 2026-08-28 P1 #24/#25 return-ownership-transfer + discard: return-local skips its guard dtor + discard x; consume-value (guard-dtor-now + moved)  # 2026-08-28 P1 partial-move #17: move source field-path registry + field-read A3101 check  # 2026-08-28 P1 drop-mark #15: guard_ready uninit-skip-dtor + mark-ready-on-assign  # 2026-08-28 P1 dangling #11: return-addr-of-local A3105 check (value-type local root)  # 2026-08-28 P1 arc #7: atomic_fetch_add movsxd sign-ext + Arc gongxiang/kelong/shifang  # 2026-08-28 P1 rc #6: qcc_rt Rc shared-ownership refcount (gongxiang/kelong/shifang/qu)  # 2026-08-28 type-root-16 ctfe #76: comptime fn eval stmt-level interpreter decl/assign/if/while/for/return  # 2026-08-28 type-root-15 coro #41: kernel32 IAT 15 slot fiber CreateFiber/SwitchToFiber + DATA_RVA_OFF 0x900  # 2026-08-28 type-root-14 thread #45: kernel32 IAT 8-11 CreateThread/WaitForSingleObject/CloseHandle  # 2026-08-28 type-root-13 scope #277: block-scope shadowing + for-init var scoped to loop  # 2026-08-28 type-root-12 macro #167: jm AST-level token expansion A9007 arity + A9008 overflow  # 2026-08-28 type-root-11 reflection #166: refl field-count/size/offset  # 2026-08-28 type-root-10 const #278: const-name = expr compile-time + immutable A9006  # 2026-08-28 type-root-9 closure #211: closure fn cap(vars) by-ref capture  # 2026-08-28 type-root-8 trait-iface #114: iface/impl/call static dispatch A9004/A9005  # 2026-08-28 type-root-7 range-type #101: range-type A9003 out-of-bounds  # 2026-08-28 type-root-6 iwidth #99: i8/i16/i32/i64/u32/u64 explicit width types  # 2026-08-28 type-root-5 try_lock #55: trylock expr-level single CAS (1/0)  # 2026-08-28 type-root-4 no-implicit-cast #74: zhuan T(x) op + double->int narrowing A9002  # 2026-08-28 type-root-3 NLL borrow: last-use auto-yield (read-only -> mut borrow)  # 2026-08-28 type-root-2 error-union: A9001 unchecked-result check (try/err-fn-return/explicit-cmp pass, 4 consume points) + file now strict UTF-8  # 2026-08-28 nullable type-root-fix: A8001 unwrap-check + pending tok-pos + fn-decl cleanup + var_offset_ptr/var_static register
if (($h1 -eq $h2) -and ($h2 -eq $h3) -and ($h3 -eq $h4) -and ($h4 -eq $h5) -and ($h1.Substring(0,8) -eq $expected)) {
    Write-Host "[OK]          1-cycle          ?: v1==v2==v3==v4==v5 = $($h1.Substring(0,8))" -ForegroundColor Green
} else {
    Write-Host "[FAIL]                             : expected=$expected         SHA256=$($h1.Substring(0,8)) (                  /expected        SHA256,       _BOOT.md L74)" -ForegroundColor Red
    Write-Host "                     =                    !                                                           expected" -ForegroundColor Red
    exit 1
}

# 4. AI         #5 (                   P0-5):                                                                                            (                        ==                    ==                  ; --cache miss      hit      verify)
Write-Host "[3.5/5]                                            (--cache + --cache-verify) ..." -ForegroundColor Yellow
New-Item -ItemType Directory -Force -Path scratch_test | Out-Null
Remove-Item scratch_test\_cache_fp.exe, scratch_test\_cache_fp.exe.qcckey, scratch_test\_cache_fp.err -ErrorAction SilentlyContinue
$h1l = $h1.ToLower()
# cmd /c                stderr: PS5.1 EAP=Stop                stderr           NativeCommandError          (2>&1/2>file                   ), cmd                     
cmd /c "qcc_x86.exe --cache --selfcheck $h1l srclib_jiayan\qcc_work.jy -o scratch_test\_cache_fp.exe 2>scratch_test\_cache_fp.err"
if ($LASTEXITCODE -ne 0) { Write-Host "[FAIL]                          miss                     " -ForegroundColor Red; exit 1 }
cmd /c "qcc_x86.exe --cache-verify --selfcheck $h1l srclib_jiayan\qcc_work.jy -o scratch_test\_cache_fp.exe 2>scratch_test\_cache_fp.err"
if ($LASTEXITCODE -ne 0) { Write-Host "[FAIL]                          verify         " -ForegroundColor Red; exit 1 }
$cv = Get-Content scratch_test\_cache_fp.err -Raw
if ($cv -notmatch 'verify OK') { Write-Host "[FAIL]                                       : $cv" -ForegroundColor Red; exit 1 }
Write-Host "[OK]                                           :                         ==                    ==$($h1.Substring(0,8))" -ForegroundColor Green

# 4.                                                         +            +            ,              @EXPECTED compile_fail                                                                                     ? NativeCommandError      
Write-Host "[4/4]                                        run_tests.py ..." -ForegroundColor Yellow
python scripts/run_tests.py
if ($LASTEXITCODE -ne 0) {
    Write-Host "[FAIL]                             " -ForegroundColor Red
    exit 1
}

# 5.                       fix 2026-08-09                        ?:                                                 , 2                             
Write-Host "[5/5]                            tests/behavior ..." -ForegroundColor Yellow
python tests/behavior/run_behavior.py
if ($LASTEXITCODE -ne 0) {
    Write-Host "[WARN]                             " -ForegroundColor Red
    exit 1
}

# 6.                   
Write-Host "[+]                          jyld / jycc ..." -ForegroundColor Yellow
# 5.5                      (                     2026-08-28:                                                               dst        /RAII        return            ;                                         )
Write-Host "[5.5/5]                      (18        _verify_*.py:             /        /RAII/              /              ) ..." -ForegroundColor Yellow
$vfails = 0
Get-ChildItem scripts\_verify_*.py | ForEach-Object {
    python $_.FullName *> $null
    if ($LASTEXITCODE -ne 0) { Write-Host "  [FAIL] $($_.Name)" -ForegroundColor Red; $vfails++ }
}
if ($vfails -gt 0) { Write-Host "[FAIL] $vfails                                   " -ForegroundColor Red; exit 1 }
Write-Host "[OK]                      18                (            /        /RAII/              /                                        )" -ForegroundColor Green
gcc -O2 -Wall -Werror srclib/jyld.c -o jyld.exe
gcc -O2 -Wall -Werror srclib/jycc.c -o jycc.exe

Write-Host ""
Write-Host "====                                                                  ? ====" -ForegroundColor Cyan
