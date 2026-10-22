# -*- coding: utf-8 -*-
"""甲言 行为断言测试运行器 (Python 版, v2 支持快速集/并行)
用法:
  python scripts/run_tests.py                 全量 325 串行 (~12 分钟, 原行为)
  python scripts/run_tests.py --fast         快速集 30 关键用例 (~1 分钟, 修 bug 时跑)
  python scripts/run_tests.py --parallel 4   全量并行 4 核 (~3-4 分钟)
  python scripts/run_tests.py --only a,b,c   只跑指定测试 (逗号分隔, 相对 tests/ 路径)
断言格式（测试 .c 头部注释）:
  // @EXPECTED exit:0           退出码 == 0
  // @EXPECTED exit:42          退出码 == 42
  // @EXPECTED exit:nonzero     退出码 != 0 (探针/崩溃)
  // @EXPECTED out:<str>        stdout 包含该字符串 (可选)
无 @EXPECTED 时默认 exit:0
"""
import sys, os, subprocess, re, argparse, time, shlex
sys.stdout.reconfigure(encoding='utf-8')

root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
os.chdir(root)

# 快速集 (30 个关键用例, 覆盖类型/布局/变参/控制流/指针/字符串/数组)
FAST_TESTS = [
    'qcc/regress_char_incdec.c', 'qcc/regress_uchar_global.c', 'qcc/regress_typedef_size.c',
    'qcc/regress_size_t_layout.c', 'qcc/regress_unsigned_cmp.c', 'qcc/regress_ll_neg.c',
    'qcc/regress_struct_align.c', 'qcc/regress_union.c', 'qcc/regress_bitfield_key.c',
    'qcc/regress_nested_struct_field.c', 'qcc/regress_anon_union.c',
    'qcc/regress_variadic_call.c', 'qcc/regress_vararg_promote.c',
    'qcc/fib0.c', 'qcc/regress_switch_fallthrough.c', 'qcc/regress_switch_case_call.c',
    'qcc/regress_struct_ptr_arrow.c', 'qcc/regress_fnptr_call.c', 'qcc/arg5.c',
    'qcc/regress_strlen.c', 'qcc/regress_multi_dim_arr.c', 'qcc/regress_static_structarr_init.c',
    'qcc/regress_volatile.c', 'qcc/regress_enum.c', 'qcc/regress_preproc_ifdef.c',
    'qcc/regress_fn_macro_nest.c', 'qcc/regress_compound_scalar.c', 'qcc/regress_utf8_bom.c',
    'behavior/b_printf.c', 'behavior/b_struct.c',
]

def build_tests(args):
    if args.fast:
        return list(FAST_TESTS)
    if args.only:
        return [t.strip() for t in args.only.split(',') if t.strip()]
    tests = sorted('qcc/' + f for f in os.listdir('tests/qcc') if f.endswith('.c'))
    tests += sorted('behavior/' + f for f in os.listdir('tests/behavior') if f.endswith('.c'))
    return tests


def run_one_test(name):
    """单个测试: 编译+运行+断言. 返回 (name, ok, msg)"""
    # ── 2026-10-21 修 CI 跑 4 小时 20 分的根因 ──────────────────────────
    # 原来写死 'qcc_x86.exe'（英文链，停更于 2026-09-06）。
    # 实测 qcc_x86.exe 编 tests/qcc/regress_adt_typed.c **挂死**（25 秒+ 不返回，
    # CPU≈0 纯阻塞），而同一文件 qcc_zh_seed.exe 3 秒编过并输出 OK。
    # 该测试（#97 判别类型化载荷）是新特性，只存在于中文端 ⟹ 英文链根本没有这条路径。
    # 故改为优先中文主链种子；英文链仅在种子缺失时兜底。
    qcc = os.path.join(root, 'qcc_zh_seed.exe')
    if not os.path.exists(qcc):
        qcc = os.path.join(root, 'qcc_x86.exe')

    src = os.path.join('tests', name)
    expected = '0'; expected_out = None; expected_err = None; expect_cfail = False; stdin_in = None
    # 2026-10-21 补: @ARG <编译器选项> —— 原框架只会传固定 [qcc, src, -o, out],
    #   带 @ARG 的测试拿不到它的编译选项 ⟹ 编译出的是另一种配置 ⟹ 断言必然失配。
    #   实测受害者: tests/qcc/regress_stat_layout.c 头 `// @ARG -I compat_prelude.h`,
    #   缺 -I ⟹ struct stat 走默认布局 ⟹ st_atime_off=24 而期望 32 (该目录仅此 1 个带 @ARG)。
    #   实现照 run_tests_zh.py L3738（shlex 切分，`-D A=1` 与 `-DA=1` 两形皆可）。
    arg_extra = []
    try:
        # R561 (2026-09-30 虾米 · 第 429 站): 同 `run_tests_zh.py` —— **read(600) 截断 ⟹ 长头注释锚的
        #   `@EXPECTED` 静默失效**（实测: tests/qcc 下 307 个含断言锚里 2 个落 600 之后）。改读全文。
        head = open(src, encoding='utf-8', errors='replace').read()
    except Exception:
        head = ''
    for line in head.split('\n'):
        m = re.search(r'//\s*@EXPECTED\s+exit:\s*(\S+)', line)
        if m: expected = m.group(1)
        m = re.search(r'//\s*@EXPECTED\s+out:(.+)$', line)
        if m: expected_out = m.group(1).strip()
        # 2026-10-23 启元审计: 新增 `@EXPECTED err:<str>` —— 断言 **stderr** 含该字符串。
        #   起因: `tests/qcc/regress_star_precision.c`（2026-08-18 8ca9cfe7 引入）用
        #   `fprintf(stderr, "%shint:%s%.*s%s\n", ...)` 输出，断言却写 `@EXPECTED out:...`
        #   ⟹ 只查 stdout 永远匹配不到 ⟹ 恒红。该测试只在 nightly_full（schedule 触发）
        #   里跑，长期被掩盖（run #186 实测 377 个里唯一 FAIL）。
        #   编译器行为本身**完全正确**（stdout='OK'，stderr='hint: Using \'master\' as the name'），
        #   所以不该改测试源去迁就框架 —— 而是让框架能断言 stderr。
        m = re.search(r'//\s*@EXPECTED\s+err:(.+)$', line)
        if m: expected_err = m.group(1).strip()
        m = re.search(r'//\s*@EXPECTED\s+in:(.+)$', line)
        if m: stdin_in = m.group(1).strip()
        if '@EXPECTED compile_fail' in line: expect_cfail = True
        m = re.search(r'//\s*@ARG\s+(.+)$', line)
        if m: arg_extra += shlex.split(m.group(1).strip())

    # 编译 (输出名用 basename, 兼容 behavior/ 子目录; 并行安全: 每测试独立 exe 名)
    h1 = os.path.join('scratch_test', os.path.basename(name)[:-2] + '_H1.exe')
    try:
        os.remove(h1)
    except OSError:
        pass
    r = subprocess.run([qcc, src, '-o', h1] + arg_extra, capture_output=True, text=True,
                       encoding='utf-8', errors='replace', timeout=60)
    if r.returncode != 0 or not os.path.exists(h1):
        if expect_cfail:
            return (name, True, 'compile_fail(期望)')
        return (name, False, '编译失败')

    # 运行
    out_file = os.path.join('scratch_test', os.path.basename(name)[:-2] + '_out.txt')
    err_file = os.path.join('scratch_test', os.path.basename(name)[:-2] + '_err.txt')
    try:
        os.remove(out_file); os.remove(err_file)
    except OSError:
        pass
    try:
        # cmd /c 启动: Python 直接 CreateProcess qcc 生成 PE 会 0xC0000005 (fix 2026-08-08)
        # ── 2026-10-21 修死锁 + te.pid ──────────────────────────────────────
        # 原写法 subprocess.run(..., timeout=15) 有两处缺陷:
        #   ① subprocess.TimeoutExpired **没有 pid 属性** ⟹ 超时处理里 str(te.pid) 抛
        #      AttributeError，而 except 子句内抛的异常不会被同一 try 的后续 except 捕获
        #      ⟹ 整条断言崩掉，而非记 TIMEOUT。
        #   ② run() 超时后内部仍要 communicate() 收管道；若 cmd 派生的孙进程继承了管道
        #      句柄且未退出，communicate 会一直等 ⟹ **超时形同虚设，进程静默挂死**。
        #      这正是 CI `tests` job 跑 4 小时 20 分的形态（CPU 占用≈0 的纯阻塞）。
        # 改法: Popen 显式持有 pid → 超时先 taskkill /F /T 杀整棵进程树 → 再 kill 兜底，
        #       之后 communicate(timeout=5) 收尾，拿不到就当作空输出，绝不无限等。
        p = subprocess.Popen(['cmd', '/c', h1], stdin=subprocess.PIPE,
                             stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                             text=True, encoding='utf-8', errors='replace')
        try:
            out_s, err_s = p.communicate(input=stdin_in, timeout=15)
            rc = p.returncode
            out = (out_s or '').strip()
            err = (err_s or '').strip()
        except subprocess.TimeoutExpired:
            rc = 'TIMEOUT'; out = ''; err = ''
            try:
                subprocess.run(['taskkill', '/F', '/T', '/PID', str(p.pid)],
                               capture_output=True, timeout=10)
            except Exception:
                pass
            try:
                p.kill()
            except Exception:
                pass
            try:
                p.communicate(timeout=5)
            except Exception:
                pass
    except Exception as e:
        rc = 'ERR'; out = str(e); err = ''
    try:
        os.remove(h1); os.remove(out_file); os.remove(err_file)
    except OSError:
        pass

    ok = False
    if expected == 'nonzero':
        ok = (rc != 0)
    else:
        ok = (str(rc) == expected)
    if ok and expected_out:
        ok = expected_out in out
    if ok and expected_err:
        ok = expected_err in err

    if ok:
        return (name, True, '')
    return (name, False, f"期望exit={expected} 实际={rc} out='{out[:40]}' err='{err[:40]}'")


def main():
    ap = argparse.ArgumentParser(description='甲言行为断言测试')
    ap.add_argument('--fast', action='store_true', help='快速集 30 用例 (~1 分钟)')
    ap.add_argument('--parallel', type=int, default=0, help='并行 worker 数 (0=串行)')
    ap.add_argument('--only', help='只跑指定测试 (逗号分隔)')
    args = ap.parse_args()

    # 确保编译器 —— 2026-10-21 改：优先中文主链种子（理由见 run_one_test 注释）
    qcc = os.path.join(root, 'qcc_zh_seed.exe')
    if os.path.exists(qcc):
        print(f'[编译器] 中文主链种子: qcc_zh_seed.exe', flush=True)
    else:
        qcc = os.path.join(root, 'qcc_x86.exe')
        if not os.path.exists(qcc):
            print('[1] 无中文种子，编译英文链宿主 qcc_x86 ...')
            r = subprocess.run(['gcc', '-O2', '-Wall', '-Werror', 'srclib/qcc_x86.c', '-o', qcc])
            if r.returncode != 0:
                print('[FAIL] 编译宿主失败'); sys.exit(1)
        else:
            print(f'[编译器] 英文链兜底: qcc_x86.exe', flush=True)

    # 清理 scratch_test (并行/串行都只清一次)
    import shutil
    st_dir = os.path.join(root, 'scratch_test')
    subprocess.run(['powershell', '-NoProfile', '-Command',
                    'Get-Process -Name "*_H1*" -ErrorAction SilentlyContinue | Stop-Process -Force'],
                   capture_output=True)
    if os.path.isdir(st_dir):
        for f in os.listdir(st_dir):
            try:
                os.remove(os.path.join(st_dir, f))
            except OSError:
                pass
    else:
        os.makedirs(st_dir, exist_ok=True)

    tests = build_tests(args)
    print(f'[RUN] {len(tests)} 个测试 (fast={args.fast} parallel={args.parallel})', flush=True)

    if args.parallel > 1:
        from multiprocessing import Pool
        with Pool(args.parallel) as pool:
            results = pool.map(run_one_test, tests, chunksize=1)
    else:
        # 2026-10-21: 加进度输出。原写法 `[run_one_test(t) for t in tests]` 全程静默 ——
        # 377 个测试跑几分钟只印一行开头，一旦卡住完全无法定位（CI 那次 4 小时就是这样白等）。
        # 现每 25 个印一次进度，且任何 FAIL / TIMEOUT 立刻印，卡住时能一眼看出停在哪一个。
        results = []
        _t0 = time.time()
        for _idx, _t in enumerate(tests, 1):
            _r = run_one_test(_t)
            results.append(_r)
            _name, _ok, _msg = _r
            if not _ok:
                print(f'  [{_idx}/{len(tests)}] FAIL {_name}  ({_msg})', flush=True)
            if _idx % 25 == 0 or _idx == len(tests):
                print(f'  [{_idx}/{len(tests)}] 累计 {time.time()-_t0:.0f}s', flush=True)

    pass_n = sum(1 for _, ok, _ in results if ok)
    fail_n = len(results) - pass_n
    fails = [f"{name} ({msg})" for name, ok, msg in results if not ok]

    # 防环境偶发 (2026-08-28 大哥指令): 失败测试自动重试一次 —
    # 真回归重跑必再挂 (不放过), Windows 环境抖动 (杀软扫新 exe/启动时序) 重跑即过 (不误伤)
    if fail_n > 0:
        retry_names = [n for n, ok, _ in results if not ok]
        print(f'--- {len(retry_names)} 个失败, 自动重试一次 ---', flush=True)
        retried = [run_one_test(n) for n in retry_names]
        rmap = {n: (ok2, msg2) for n, ok2, msg2 in retried}
        results = [(n, rmap[n][0] if n in rmap else ok, rmap[n][1] if n in rmap else msg)
                   for n, ok, msg in results]
        pass_n = sum(1 for _, ok, _ in results if ok)
        fail_n = len(results) - pass_n
        fails = [f"{name} ({msg})" for name, ok, msg in results if not ok]
        if fail_n > 0:
            print(f'--- 重试后仍失败 {fail_n} 个 (判定为真回归) ---')

    print(f'\n行为断言: PASS={pass_n} FAIL={fail_n}')
    if fails:
        print('--- 失败 ---')
        for f in fails[:30]:
            print(f'  {f}')
        sys.exit(1)
    print('==== 全绿 ====')


if __name__ == '__main__':
    main()
