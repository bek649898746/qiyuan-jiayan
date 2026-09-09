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
import sys, os, subprocess, re, argparse
sys.stdout.reconfigure(encoding='utf-8')

root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
os.chdir(root)

# 快速集 (30 个关键用例, 覆盖类型/布局/变参/控制流/指针/字符串/数组)
FAST_TESTS = [    'qcc/regress_char_incdec.c', 'qcc/regress_uchar_global.c', 'qcc/regress_typedef_size.c',
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

# 核心确定性中文特性 (2026-09-05 测试挂载): 所有权/借用/多态/copy 族 — 独立有主测试, --zh-feat 默认跑 (自动盯真回归)
CORE_FEAT = [
    'scripts/feat_move_zh.c', 'scripts/feat_move2_zh.c', 'scripts/feat_move13_zh.c',
    'scripts/feat_zh_copy_semantics9.c', 'scripts/feat_zh_copy_semantics9_neg.c',
    'scripts/feat_zh_linear51.c', 'scripts/feat_zh_linear51_neg.c',
    'scripts/feat_ownership.c', 'scripts/feat_drop_zh.c',
    'scripts/feat_borrow3_zh.c', 'scripts/feat_borrow3_neg.c', 'scripts/feat_borrow_auto_zh.c',
    'scripts/feat_zh_own_move.c', 'scripts/feat_zh_own_move_neg.c', 'scripts/feat_zh_own_param_neg.c',
    'scripts/feat_zh_copy_auto.c', 'scripts/feat_zh_copy_auto_neg.c',
    'scripts/feat_zh_copy_movein_auto.c',
    'scripts/feat_zh_own_branch.c', 'scripts/feat_zh_own_branch_neg.c',
    'scripts/feat_zh_own_guardmove.c',
    'scripts/feat_zh_own_partial.c', 'scripts/feat_zh_own_partial_neg.c',
    'scripts/feat_zh_own_partial_reassign.c', 'scripts/feat_zh_own_partial_shadow.c',
    'scripts/feat_zh_own_partial_shadowwrite_neg.c', 'scripts/feat_zh_own_partial_shadowrestore.c',
    'scripts/feat_zh_own_partial_arrshadow.c', 'scripts/feat_zh_own_partial_arrshadow_neg.c',
    'scripts/feat_zh_own_partial_branchshadow.c', 'scripts/feat_zh_own_partial_branchshadow_neg.c',
    'scripts/feat_zh_uninit_shadow.c', 'scripts/feat_zh_uninit_shadow_neg.c',
    'scripts/feat_zh_const_shadow.c', 'scripts/feat_zh_enum_shadow.c',
    'scripts/feat_zh_own_cap300_neg.c', 'scripts/feat_zh_uninit_cap300_neg.c',
    'scripts/feat_zh_borrow_cap300_neg.c',
    'scripts/feat_zh_own_dynidx_move_neg.c', 'scripts/feat_zh_own_dynidx_field_neg.c',
    'scripts/feat_zh_own_overwrite.c', 'scripts/feat_zh_own_branchdrop_else.c',
    'scripts/feat_zh_own_branchdrop_then.c', 'scripts/feat_zh_own_uninitguard.c',
    'scripts/feat_zh_own_uninit_movein.c', 'scripts/feat_zh_own_drop_reassign.c',
    'scripts/feat_zh_own_guard_partial_neg.c', 'scripts/feat_zh_own_guard_param.c',
    'scripts/feat_zh_guard_shadow_partial.c', 'scripts/feat_zh_guard_shadow_dup.c',
    'scripts/feat_zh_guard_shadow_discard.c',
    'scripts/feat_zh_own_guard_fieldout_neg.c', 'scripts/feat_zh_own_loopbreak.c',
    'scripts/feat_zh_own_loopcont.c', 'scripts/feat_zh_own_loop_assign.c',
    'scripts/feat_zh_own_for_break.c', 'scripts/feat_zh_own_switch_break.c',
    'scripts/feat_zh_own_switch_case_guard.c', 'scripts/feat_zh_own_guard_nested_neg.c',
    'scripts/feat_zh_own_labeled_break.c', 'scripts/feat_zh_own_guard_defer.c',
    'scripts/feat_zh_typedrop_scope.c', 'scripts/feat_zh_typedrop_overwrite.c',
    'scripts/feat_zh_typedrop_param.c', 'scripts/feat_zh_typedrop_branch.c',
    'scripts/feat_zh_typedrop_retmove.c', 'scripts/feat_zh_typedrop_loop.c',
    'scripts/feat_zh_typedrop_movein.c', 'scripts/feat_zh_typedrop_decl_assign.c',
    'scripts/feat_zh_typedrop_array.c', 'scripts/feat_zh_typedrop_capacity.c',
    'scripts/feat_zh_typedrop_fieldglue.c', 'scripts/feat_zh_typedrop_recglue.c',
    'scripts/feat_zh_typedrop_paramglue.c', 'scripts/feat_zh_typedrop_expr_neg.c',
    'scripts/feat_zh_typedrop_partial_neg.c', 'scripts/feat_zh_typedrop_borrow_ok.c',
    'scripts/feat_zh_typedrop_copy_neg.c', 'scripts/feat_zh_typedrop_arrcomposite.c',
    'scripts/feat_zh_typedrop_initexpr_neg.c', 'scripts/feat_zh_disc_top.c', 'scripts/feat_zh_struct_uninit_read_neg.c', 'scripts/feat_zh_discrim_kw_neg.c', 'scripts/feat_zh_discrim_decon_neg.c', 'scripts/feat_zh_block_packed.c',
    'scripts/feat_zh_typedrop_order.c', 'scripts/feat_zh_typedrop_order2.c',
    'scripts/feat_zh_typedrop_discard.c', 'scripts/feat_zh_typedrop_elemset.c',
    'scripts/feat_zh_typedrop_fieldarr.c', 'scripts/feat_zh_typedrop_fieldarrcomp.c',
    'scripts/feat_zh_typedrop_elemdyn.c',
    'scripts/feat_zh_borrow_cfg.c', 'scripts/feat_zh_borrow_cfg_neg.c',
    'scripts/feat_zh_borrow_cfg2_neg.c', 'scripts/feat_zh_borrow_cfg3.c',
    'scripts/feat_zh_borrow_shadow.c',
    'scripts/feat_zh_alias_dangle_neg.c', 'scripts/feat_zh_alias_deref_dangle_neg.c',
    'scripts/feat_zh_alias_ok.c', 'scripts/feat_zh_alias_ok2.c',
    'scripts/feat_zh_alias_decl_deref_dangle_neg.c', 'scripts/feat_zh_alias_decl_arrow_dangle_neg.c',
    'scripts/feat_zh_alias_arrow_write_dangle_neg.c', 'scripts/feat_zh_alias_decl_deref_write_dangle_neg.c',
    'scripts/feat_zh_alias_decl_arrow_write_dangle_neg.c',
    'scripts/feat_zh_alias_member_move_neg.c',
    'scripts/feat_zh_borrow_nest_neg.c',
    'scripts/feat_zh_borrow_alias_overwrite_neg.c',
    'scripts/feat_zh_borrow_errparam_move_neg.c',
    'scripts/feat_zh_borrow_en_struct_move_neg.c',
    'scripts/feat_zh_lifetime_outlives.c', 'scripts/feat_zh_lifetime_outlives_neg.c',
    'scripts/feat_zh_lifetime_elision.c', 'scripts/feat_zh_lifetime_elision_neg.c',
    'scripts/feat_zh_refcell.c', 'scripts/feat_zh_refcell_conflict.c',
    'scripts/feat_zh_outparam.c',
    'scripts/feat_zh_reborrow.c', 'scripts/feat_zh_reborrow_neg.c',
    'scripts/feat_zh_err_union.c', 'scripts/feat_zh_err_union_neg.c',
    'scripts/feat_zh_err_union_value.c', 'scripts/feat_zh_err_union_multi_neg.c',
    'scripts/feat_zh_err_union_multicode.c', 'scripts/feat_zh_err_union_tag.c',
    'scripts/feat_zh_err_union_decons.c', 'scripts/feat_zh_err_union_decons_neg.c',
    'scripts/feat_zh_err_union_ll8.c', 'scripts/feat_zh_err_union_ptr8.c',
    'scripts/feat_zh_err_union_ptrbig8.c',
    'scripts/feat_zh_err_union_struct_neg.c', 'scripts/feat_zh_multiret_struct_neg.c',
    'scripts/feat_zh_unsigned_signed_neg.c',
    'scripts/feat_zh_field_elem_narrow_neg.c',
    'scripts/feat_zh_err_try_in_normal_neg.c',
    'scripts/feat_zh_s2a_multi_nullable.c',
    'scripts/feat_zh_nullable_value_op_neg.c', 'scripts/feat_zh_nullable_value_op_ok.c',
    'scripts/feat_zh_s2b3_static_value_nullable.c',
    'scripts/feat_zh_s2b4_copy.c',
    'scripts/feat_zh_s2c_drop_nonint.c',
    'scripts/feat_zh_s2b_field_nullable_neg.c',
    'scripts/feat_null75_zh.c', 'scripts/feat_init78_zh.c',
    'scripts/feat_unwrap.c', 'scripts/feat_unwrap2.c',
    'scripts/feat_optchain_zh.c', 'scripts/feat_optchain2_zh.c',
    'scripts/feat_option.c',
    'scripts/feat_zh_overflow33.c', 'scripts/feat_zh_restrict36.c', 'scripts/feat_zh_restrict36_neg.c',
    'scripts/feat_zh_conv_ret.c', 'scripts/feat_zh_conv_ret_neg.c',
    'scripts/feat_zh_conv_arg.c', 'scripts/feat_zh_conv_arg_neg.c',
    'scripts/feat_zh_conv_compound.c', 'scripts/feat_zh_conv_compound_neg.c',
    'scripts/feat_zh_bound_div.c', 'scripts/feat_zh_bound_divzero.c',
    'scripts/feat_zh_opt_nullcheck.c',
    'scripts/feat_zh_opt_nullcheck_stmt.c',
    'scripts/feat_zh_opt_flow_narrow.c',
    'scripts/feat_zh_opt_flow_narrow_while.c',
    'scripts/feat_zh_opt_uninit.c', 'scripts/feat_zh_opt_uninit_neg.c',
    'scripts/feat_zh_opt_pending_neg.c', 'scripts/feat_zh_opt_static_nullable.c',
    'scripts/feat_zh_opt_static_nullable_neg.c',
    'scripts/feat_zh_float_dbl.c',
    'scripts/feat_zh_float4.c',
    'scripts/feat_zh_float_field.c',
    'scripts/feat_zh_float_fn.c',
    'scripts/feat_zh_vtable.c', 'scripts/feat_zh_vtable_neg.c', 'scripts/feat_zh_vtable_inherit.c',
    'scripts/feat_zh_vtable_multi.c', 'scripts/feat_zh_vtable_mparam.c', 'scripts/feat_zh_vtable_param.c',
    'scripts/feat_zh_vtable_rebind.c', 'scripts/feat_zh_vtable_rebind_neg.c',
    'scripts/feat_zh_vtable_inline.c', 'scripts/feat_zh_vtable_inline_neg.c',
    'scripts/feat_zh_vtable_copyinit.c', 'scripts/feat_zh_vtable_copyinit_neg.c',
    'scripts/feat_zh_vtable_ifaceparent.c',
    'scripts/feat_zh_fieldkw_data.c', 'scripts/feat_zh_fieldkw_long_neg.c',
    'scripts/feat_zh_field_unknown_neg.c', 'scripts/feat_zh_field_nested_unknown_neg.c',
    'scripts/feat_zh_field_scalar_neg.c', 'scripts/feat_zh_iface_field_layout.c',
    'scripts/feat_zh_iface_field_assign.c',
    'scripts/feat_zh_iface_field_call.c', 'scripts/feat_zh_iface_field_call_neg.c',
    'scripts/feat_zh_field_decl_swallow_neg.c', 'scripts/feat_zh_field_decl_swallow_nested_neg.c',
    'scripts/feat_zh_iface_field_arg.c', 'scripts/feat_zh_iface_field_arg_neg.c',
    'scripts/feat_zh_iface_field_copyout.c', 'scripts/feat_zh_iface_field_copyout_neg.c',
    'scripts/feat_zh_iface_field_container.c',
    'scripts/feat_zh_iface_field_arr.c',
    'scripts/feat_zh_iface_field_nested.c',
    'scripts/feat_zh_iface_target_write_neg.c',
    'scripts/feat_zh_inherit193.c', 'scripts/feat_zh_inherit193_edge.c',
    'scripts/feat_zh_contract_struct_zh.c',
    'scripts/feat_guard.c', 'scripts/feat_guard2.c', 'scripts/feat_raii_zh.c', 'scripts/feat_unique_zh.c',
]

def build_tests(args):
    if args.zh_feat:
        return list(CORE_FEAT)   # 核心确定性中文特性 (独立 有主 测试, 盯真崩)
    if args.zh_feat_all:
        tests = sorted('scripts/' + f for f in os.listdir('scripts')
                       if (f.startswith('feat_zh_') or f.endswith('_zh.c')) and f.endswith('.c'))
        return tests
    if args.fast:
        return list(FAST_TESTS)
    if args.only:
        return [t.strip() for t in args.only.split(',') if t.strip()]
    tests = sorted('qcc/' + f for f in os.listdir('tests/qcc') if f.endswith('.c'))
    tests += sorted('behavior/' + f for f in os.listdir('tests/behavior') if f.endswith('.c'))
    return tests


def find_qcc():
    """最新自举产物: qcc_zh_v7 → ... → v1 (审计 2026-09-07: 原硬编码 v7, 仅烟测 v1 时 fallback gcc)"""
    for n in range(7, 0, -1):
        p = os.path.join(root, 'qcc_zh_v%d.exe' % n)
        if os.path.exists(p):
            return p
    return os.path.join(root, 'qcc_x86.exe')

def run_one_test(name):
    """单个测试: 编译+运行+断言. 返回 (name, ok, msg)"""
    qcc = find_qcc()
    if name.startswith('scripts/'):
        src = os.path.join(root, name)
    else:
        src = os.path.join('tests', name)
    expected = '0'; expected_out = None; expect_cfail = False; stdin_in = None
    dep = False; multi = False
    try:
        head = open(src, encoding='utf-8', errors='replace').read(600)
    except Exception:
        head = ''
    for line in head.split('\n'):
        m = re.search(r'//\s*@EXPECTED\s+exit:\s*(\S+)', line)
        if m: expected = m.group(1)
        m = re.search(r'//\s*@EXPECTED\s+out:(.+)$', line)
        if m: expected_out = m.group(1).strip()
        m = re.search(r'//\s*@EXPECTED\s+in:(.+)$', line)
        if m: stdin_in = m.group(1).strip()
        if '@EXPECTED compile_fail' in line or '_neg' in os.path.basename(name): expect_cfail = True
        if '@DEP' in line: dep = True          # 显式声明: 依赖配套文件/外部符号 → 单文件编译失败 = 许可 skip (2026-09-07 收紧)
        if '@MULTI' in line: multi = True      # 显式声明: 需 -c + jyld 跨 .o 链接 → 单文件无主/外链失败 = 许可 skip

    # 编译 (输出名用 basename, 兼容 behavior/ 子目录; 并行安全: 每测试独立 exe 名)
    h1 = os.path.join('scratch_test', os.path.basename(name)[:-2] + '_H1.exe')
    try:
        os.remove(h1)
    except OSError:
        pass
    r = subprocess.run([qcc, src, '-o', h1], capture_output=True, text=True,
                       encoding='utf-8', errors='replace')
    if r.returncode != 0 or not os.path.exists(h1):
        if expect_cfail:
            return (name, True, 'compile_fail(期望)')
        err = (r.stderr or '') + (r.stdout or '')
        # 收紧 2026-09-07 (启元递刀 RefCell伪PASS): 不再按错误文本嗅探 '未声明/未定义函数' 等当依赖 skip —
        # 内建词漏链运行时/真编译回归 = 真 FAIL; 只有测试文件显式 @DEP/@MULTI 标注才许可 skip
        if dep:
            return (name, True, 'SKIP:依赖库(@DEP 显式标注, 需配套文件/外部符号, 单文件不链)')
        if multi:
            return (name, True, 'SKIP:无主多文件(@MULTI 显式标注, 需 -c + jyld 链接)')
        return (name, False, '编译失败: ' + (err or '').strip()[:220])   # 其它真回归 → FAIL

    # 运行
    out_file = os.path.join('scratch_test', os.path.basename(name)[:-2] + '_out.txt')
    err_file = os.path.join('scratch_test', os.path.basename(name)[:-2] + '_err.txt')
    try:
        os.remove(out_file); os.remove(err_file)
    except OSError:
        pass
    try:
        # cmd /c 启动: Python 直接 CreateProcess qcc 生成 PE 会 0xC0000005 (fix 2026-08-08)
        p = subprocess.run(['cmd', '/c', h1], capture_output=True, text=True,
                           encoding='utf-8', errors='replace', timeout=15, input=stdin_in)
        rc = p.returncode
        out = (p.stdout or '').strip()
    except subprocess.TimeoutExpired as te:
        try:
            te.kill()
        except Exception:
            pass
        rc = 'TIMEOUT'; out = ''
        subprocess.run(['taskkill', '/F', '/T', '/PID', str(te.pid)], capture_output=True, timeout=10)
    except Exception as e:
        rc = 'ERR'; out = str(e)
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

    if ok:
        return (name, True, '')
    return (name, False, f"期望exit={expected} 实际={rc} out='{out[:40]}'")


def main():
    ap = argparse.ArgumentParser(description='甲言行为断言测试')
    ap.add_argument('--fast', action='store_true', help='快速集 30 用例 (~1 分钟)')
    ap.add_argument('--zh-feat', action='store_true', help='核心确定性中文特性 (scripts/ CORE_FEAT 白名单: 所有权/借用/多态/copy 族)')
    ap.add_argument('--zh-feat-all', action='store_true', help='全量中文特性 (scripts/feat_zh_*; 依赖/无主库 skip, 其它诚实 FAIL)')
    ap.add_argument('--parallel', type=int, default=0, help='并行 worker 数 (0=串行)')
    ap.add_argument('--only', help='只跑指定测试 (逗号分隔)')
    args = ap.parse_args()

    # 确保编译器
    qcc = find_qcc()
    if not os.path.exists(qcc):
        print('[1] 编译宿主 qcc_x86 ...')
        r = subprocess.run(['gcc', '-O2', '-Wall', '-Werror', 'srclib/qcc_x86.c', '-o', qcc])
        if r.returncode != 0:
            print('[FAIL] 编译宿主失败'); sys.exit(1)

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
        results = [run_one_test(t) for t in tests]

    def summarize():
        pass_n = sum(1 for (n2, ok2, m2) in results if ok2 and not m2.startswith('SKIP:'))
        skip_l = [(n2, m2) for (n2, ok2, m2) in results if m2.startswith('SKIP:')]
        fail_l = [f"{name} ({msg})" for name, ok, msg in results if not ok]
        return pass_n, skip_l, fail_l

    pass_n, skip_l, fail_l = summarize()

    # 防环境偶发 (2026-08-28 大哥指令): 失败测试自动重试一次 —
    # 真回归重跑必再挂 (不放过), Windows 环境抖动 (杀软扫新 exe/启动时序) 重跑即过 (不误伤)
    if fail_l:
        retry_names = [n for n, ok, _ in results if not ok]
        print(f'--- {len(retry_names)} 个失败, 自动重试一次 ---', flush=True)
        retried = [run_one_test(n) for n in retry_names]
        rmap = {n: (ok2, msg2) for n, ok2, msg2 in retried}
        results = [(n, rmap[n][0] if n in rmap else ok, rmap[n][1] if n in rmap else msg)
                   for n, ok, msg in results]
        pass_n, skip_l, fail_l = summarize()
        if fail_l:
            print(f'--- 重试后仍失败 {len(fail_l)} 个 (判定为真回归) ---')

    print(f'\n行为断言: PASS={pass_n} SKIP={len(skip_l)} FAIL={len(fail_l)}')
    if skip_l:
        print('--- 显式 SKIP (标注 @DEP/@MULTI, 非核心覆盖) ---')
        for n, m in skip_l:
            print(f'  {n}  {m}')
    if fail_l:
        print('--- 失败 ---')
        for f in fail_l[:30]:
            print(f'  {f}')
    # 门槛纪律 (2026-09-07 收紧): 核心门槛 (fast/zh-feat/en 默认全量) 出现任何 SKIP = 红 (特性必须真测);
    # --zh-feat-all (诊断全量) 与 --only (单测) 只展示, 不硬红 (含显式标注的配套/多文件测试)
    gate = not args.zh_feat_all and not args.only
    if fail_l or (gate and skip_l):
        sys.exit(1)
    print('==== 全绿 ====')


if __name__ == '__main__':
    main()

