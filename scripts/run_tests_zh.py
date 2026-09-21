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
import sys, os, subprocess, re, argparse, shlex
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
    'scripts/feat_zh_prelude_regmatch_ok.c',   # C66 (2026-09-17 第 66 站): 口径回归锚 — 兼容层**不得用对象宏做类型别名**; 注回 `#define regmatch_t int` 即变红 (台账 §66)
    'scripts/feat_zh_paren_ptr_array_ok.c',    # C72 (2026-09-17 第 72 站): `结构体 T *(名[N]);` 括号指针数组声明 — 漏设 型序标 ⟹ 成员访问整段不发射; 真实载体 pack-objects.c L1695 (台账 §72)
    'scripts/feat_zh_paren_ptr_array_local_ok.c',  # C74 (2026-09-17 第 74 站): **局部** 括号指针数组 — 漏设 指元素尺寸 ⟹ 元素赋值走结构体深拷贝 ⟹ 崩 0xC0000005 (仅 8 字节结构体触发); 真实载体 builtin/diff.c L175 / grep.c L707 (台账 §74)
    'scripts/feat_zh_paren_ptr_array_local_typedef_ok.c',  # C75+C76 (2026-09-17 第 75/76 站): **typedef 形态** 指针数组 — 括号 漏 型序标 (静默 槽4/槽6) + 无括号 1D 漏 指元素尺寸 (崩); 一族四条路径对齐 (台账 §75)
    'scripts/feat_zh_nested_field_type_ok.c',  # C77 (2026-09-17 第 77 站): **结构体字段 类型序(毛利)** — 嵌套结构体路径 漏登记 + 逗号续行漏继承; 真实载体 xdiff/xpatience.c L49 (台账 §76)
    'scripts/feat_zh_field_layout_matrix_ok.c',
    'scripts/feat_zh_ret_scalar_loop_ok.c',
    'scripts/feat_zh_nullable_decl_bind_ok.c',
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
    'scripts/feat_zh_typedrop_initexpr_neg.c', 'scripts/feat_zh_disc_top.c', 'scripts/feat_zh_anon_comma_inherit.c', 'scripts/feat_zh_nested_partial_read_neg.c', 'scripts/feat_zh_nested_partial_ok.c', 'scripts/feat_zh_arr_elem_uninit_neg.c', 'scripts/feat_zh_arr_elem_ok.c', 'scripts/feat_zh_nullable_ret_neg.c', 'scripts/feat_zh_struct_uninit_read_neg.c', 'scripts/feat_zh_struct_partial_read_neg.c', 'scripts/feat_zh_struct_partial_copy_neg.c', 'scripts/feat_zh_struct_partial_samefield_ok.c', 'scripts/feat_zh_struct_partial_ok.c', 'scripts/feat_zh_struct_reinit_ok.c', 'scripts/feat_zh_for_loop_init_neg.c', 'scripts/feat_zh_discrim_kw_neg.c', 'scripts/feat_zh_discrim_decon_neg.c', 'scripts/feat_zh_disc_scope_neg.c', 'scripts/feat_zh_block_packed.c',
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
    'scripts/feat_zh_nullable_struct_ok.c', 'scripts/feat_zh_nullable_struct_unwrap_neg.c',
    'scripts/feat_zh_nullable_field_ptr_ok.c', 'scripts/feat_zh_nullable_field_ptr_judge_ok.c',
    'scripts/feat_zh_nullable_field_deref_ok.c', 'scripts/feat_zh_nullable_field_deref_neg.c',
    'scripts/feat_zh_nullable_deep_path_ok.c', 'scripts/feat_zh_nullable_deep_path_neg.c',
    'scripts/feat_zh_nullable_index_name_ok.c', 'scripts/feat_zh_nullable_index_mixed_neg.c',
    'scripts/feat_zh_value_param_nest_ok.c',
    'scripts/feat_zh_ret_member_ok.c',
    'scripts/feat_zh_struct_ternary_neg.c', 'scripts/feat_zh_struct_ternary_ok.c',
    'scripts/feat_zh_sugar_wide_ok.c', 'scripts/feat_zh_sugar_wide2_ok.c',
    'scripts/feat_zh_sugar_wide3_ok.c', 'scripts/feat_zh_sugar_wide4_ok.c',
    'scripts/feat_zh_sugar_wide5_ok.c',
    'scripts/feat_zh_init_elem_branch_ok.c', 'scripts/feat_zh_init_elem_branch_neg.c',
    # feat_zh_try_propagate_ok.c 暂 不 入闸: 目标 测试 (尝试 传播 修法 未 落地, 见 Hy2 审计)
    'scripts/feat_zh_bitfield_struct_ok.c', 'scripts/feat_zh_generator_struct_ok.c',
    'scripts/feat_zh_coro_struct_ok.c',
    'scripts/feat_zh_ternary_scalar_ok.c',
    'scripts/feat_zh_unknown_ptr_param_ok.c', 'scripts/feat_zh_closure_capture_ok.c',
    'scripts/feat_zh_closure_capture2_ok.c',
    'scripts/feat_zh_adt_multi_payload_ok.c',
    'scripts/feat_zh_iface_sret_ok.c', 'scripts/feat_zh_iface_ret_use_ok.c',
    'scripts/feat_zh_union_ok.c', 'scripts/feat_zh_struct_param_field_ok.c',
    'scripts/feat_zh_nullable_static_local_wide_ok.c',
    'scripts/feat_zh_typedef_local_ok.c', 'scripts/feat_zh_typedef_scope_ok.c',
    'scripts/feat_zh_nullable_static_wide_ok.c', 'scripts/feat_zh_nullable_static_wide_neg.c',
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
    'scripts/feat_zh_ptr_escape_field_ok.c', 'scripts/feat_zh_ptr_escape_field_neg.c', 'scripts/feat_zh_ptr_escape_whole_ok.c',
    'scripts/feat_zh_struct_init_ok.c', 'scripts/feat_zh_struct_init_scalar_neg.c', 'scripts/feat_zh_struct_init_undef_neg.c', 'scripts/feat_zh_struct_init_type_neg.c',
    'scripts/feat_zh_arr_elem_call_assign.c', 'scripts/feat_zh_arr_elem_call_assign_big.c', 'scripts/feat_zh_arr_brace_init_ok.c',
    'scripts/feat_zh_struct_arrfield_copy_ok.c',
    'scripts/feat_zh_partial_then_move_neg.c', 'scripts/feat_zh_partial_then_assign_neg.c', 'scripts/feat_zh_partial_field_move_ok.c',
    'scripts/feat_zh_nullable_unwrap_cmp_ok.c',
    'scripts/feat_zh_nullable_i64_ok.c', 'scripts/feat_zh_nullable_i64_unchecked_neg.c',
    'scripts/feat_zh_nullable_f64_ok.c',
    'scripts/feat_zh_err_transfer_check_ok.c', 'scripts/feat_zh_err_transfer_unchecked_neg.c', 'scripts/feat_zh_err_transfer_use_neg.c',
    'scripts/feat_zh_cell_borrow_ok.c', 'scripts/feat_zh_cell_underflow_rt.c', 'scripts/feat_zh_cell_conflict_rt.c',
    'scripts/feat_zh_err_double_payload_ok.c',
    'scripts/feat_zh_err_double_try_ok.c', 'scripts/feat_zh_err_double_decons_ok.c', 'scripts/feat_zh_err_try_mixed_neg.c',
    'scripts/feat_zh_err_forward_ok.c', 'scripts/feat_zh_err_forward_mixed_neg.c', 'scripts/feat_zh_err_forward_normal_neg.c',
    'scripts/feat_zh_err_struct_small_ok.c', 'scripts/feat_zh_err_struct_big_ok.c',
    'scripts/feat_zh_err_struct_mixed_neg.c', 'scripts/feat_zh_err_struct_forward_ok.c',
    'scripts/feat_zh_iface_move_freeze_neg.c', 'scripts/feat_zh_iface_ret_neg.c',
    'scripts/feat_zh_alias_multilevel_dangle_neg.c',
    'scripts/feat_zh_disc_value_copy_ok.c',
    'scripts/feat_zh_loop_carry_move_neg.c',
    # A2/A4/A11 (2026-09-11): 闭包捕获接口引用 / 三目条件窄化(±) / 内建展开出参豁免
    'scripts/feat_zh_closure_capture_iface_ok.c',
    'scripts/feat_zh_tern_narrow_ok.c', 'scripts/feat_zh_tern_narrow_neg.c',
    'scripts/feat_zh_builtin_outparam_ok.c',
    # A3 (2026-09-11): 闭包数组 (编译期常量下标 解析期展开) + 三种误用精确拒
    'scripts/feat_zh_closure_array_ok.c', 'scripts/feat_zh_closure_array_neg.c',
    'scripts/feat_zh_closure_array_bounds_neg.c', 'scripts/feat_zh_closure_array_dynidx_neg.c',
    # A1 (2026-09-11): 尝试 传播 约定 复核锚 + 转存变量 传播 规范写法 + 裸返 拦截负例
    'scripts/feat_zh_try_propagate_ok.c',
    'scripts/feat_zh_err_transfer_propagate_ok.c',
    # A1 全 ABI (2026-09-11): 错误联合值 变量 带 tag — 判码后 返 r 直接 传播 (原 静默吞错; 原 _neg 锚 已 转 正向)
    'scripts/feat_zh_err_transfer_return_ok.c',
    # C7 并发轴 (2026-09-11 扫雷: 16 个 锚 原 全部 在 闸 外 → 逐个 复验 通过 后 入闸;
    #   其中 feat_lock2 抓出 真 bug: 锁住 延迟 解锁 在 提前返回 路径 漏发 — 已修, 见 施工日志)
    'scripts/feat_thread_zh.c', 'scripts/feat_chan_zh.c', 'scripts/feat_chan2_zh.c',
    'scripts/feat_actor_zh.c', 'scripts/feat_zh_actor19.c', 'scripts/feat_zh_async20.c',
    'scripts/feat_zh_select24.c', 'scripts/feat_select2.c', 'scripts/feat_zh_struct_concur25.c',
    'scripts/feat_atomic_zh.c', 'scripts/feat_atomic_cas_zh.c', 'scripts/feat_atomic_xchg_zh.c',
    'scripts/feat_memorder_zh.c', 'scripts/feat_zh_fence21.c', 'scripts/feat_mutex2_zh.c',
    'scripts/feat_lock2.c',
    # C7 修 的 回归 锚 (锁住 三 退出路径 全 解锁; 原 提前返回 漏发)
    'scripts/feat_zh_lock_defer_ok.c',
    # v1.2 深度清单 审计 补齐 (2026-09-11): 原 判 ✅ 但 无 锚 / 只 有 闸外 锚 的 项 —
    #   ① 忘记 #20: 新 写 语义 锚 (析构 计数 三 面: 忘记不析构 / 对照析构 / 丢弃即析构)
    #   ② 背压 #68: 新 写 语义 锚 (+ 汇编 自旋回边 结构 证据)
    #   ③④⑤⑥⑦: 原 闸外 锚 入闸 (惰性/重载/CTFE/异步定时器/编译期反射) — 消除 "✅ 无 回归 保护"
    'scripts/feat_zh_forget_ok.c', 'scripts/feat_zh_backpressure_ok.c',
    'scripts/feat_zh_lazy20c.c', 'scripts/feat_overload_zh.c', 'scripts/feat_comptime_zh.c',
    'scripts/feat_async_zh.c', 'scripts/feat_reflect.c',
    # 同上 第二批: 类型别名泛型 #104 / 可变参数模板 #171+175 / 生成器 #149 / 解释求值 #170
    'scripts/feat_zh_generic_typedef46.c', 'scripts/feat_zh_generic_typedef46_arr.c',
    'scripts/feat_zh_generic_bounds45.c', 'scripts/feat_gen_zh.c', 'scripts/feat_zh_generator56.c',
    'scripts/feat_interp.c',
    # ==== 地1 G0-1 (2026-09-11): 闸 覆盖 制度化 — 闸外 锚 批量 定性 后 入闸 ====
    # 原 242 个 闸外锚 全量 编译+运行 勘查 (scratch_gl/_gap_survey2.py) 三 分类:
    #   ① 合格 入闸 (本节) ② ft_* 光板/probe 归档 ③ 异常 留观 (记 台账 A18/A19/A21…)
    # 判据: 状态 OK + 无 @DEP/@MULTI (闸 视 SKIP 为 红) + 非 一次性 调试光板
    # --- ABI/sret 大构值 返回 (9) ---
    'scripts/feat_sret.c', 'scripts/feat_sret11.c', 'scripts/feat_sret15.c',
    'scripts/feat_sret4.c', 'scripts/feat_sret5.c', 'scripts/feat_sret6.c',
    'scripts/feat_sret7.c', 'scripts/feat_sret8.c', 'scripts/feat_sret9.c',
    # --- 内建/映射 (文件IO·格式化·字符类) (9) ---
    'scripts/feat_map8_zh.c', 'scripts/feat_map9_zh.c', 'scripts/feat_map_zh.c',
    'scripts/feat_zh_map2.c', 'scripts/feat_zh_map3.c', 'scripts/feat_zh_map4.c',
    'scripts/feat_zh_map5.c', 'scripts/feat_zh_map6.c', 'scripts/feat_zh_map7.c',
    # --- 甲言 特性 锚 (50) ---
    'scripts/feat_zh_alloca83.c', 'scripts/feat_zh_arena82.c', 'scripts/feat_zh_attribute94.c',
    'scripts/feat_zh_bitfield.c', 'scripts/feat_zh_bitint.c', 'scripts/feat_zh_char8.c',
    'scripts/feat_zh_cow88.c', 'scripts/feat_zh_cpp_interop79.c', 'scripts/feat_zh_defer_block53.c',
    'scripts/feat_zh_dep_type52.c', 'scripts/feat_zh_dep_type52_neg.c', 'scripts/feat_zh_dep_type52_neg2.c',
    'scripts/feat_zh_elifdef.c', 'scripts/feat_zh_encap197.c', 'scripts/feat_zh_encap197_b_neg.c',
    'scripts/feat_zh_enum_base.c', 'scripts/feat_zh_exception55.c', 'scripts/feat_zh_exception_abi80.c',
    'scripts/feat_zh_generic_bounds45_neg.c', 'scripts/feat_zh_guard_type_dup_neg.c', 'scripts/feat_zh_inherit193_neg1.c',
    'scripts/feat_zh_inherit193_neg2.c', 'scripts/feat_zh_inline93.c', 'scripts/feat_zh_leak86.c',
    'scripts/feat_zh_lib.c', 'scripts/feat_zh_mmap89.c', 'scripts/feat_zh_nullptr.c',
    'scripts/feat_zh_override198.c', 'scripts/feat_zh_override198_neg.c', 'scripts/feat_zh_own_partial_elem.c',
    'scripts/feat_zh_own_partial_elem_neg.c', 'scripts/feat_zh_own_partial_elemwhole_neg.c', 'scripts/feat_zh_phaseC_combined.c',
    'scripts/feat_zh_range_predecl_ok.c', 'scripts/feat_zh_specialization50.c', 'scripts/feat_zh_stack33b.c',
    'scripts/feat_zh_str.c', 'scripts/feat_zh_syntax_a.c', 'scripts/feat_zh_syntax_b.c',
    'scripts/feat_zh_syntax_c.c', 'scripts/feat_zh_tdef_dbl.c', 'scripts/feat_zh_template_meta68.c',
    'scripts/feat_zh_top_level_type.c', 'scripts/feat_zh_type_prog74.c', 'scripts/feat_zh_type_prog74_neg.c',
    'scripts/feat_zh_typedrop_deepnest.c', 'scripts/feat_zh_typedrop_fieldorder.c', 'scripts/feat_zh_typeof.c',
    'scripts/feat_zh_variadic_templates72.c', 'scripts/feat_zh_vec_range_ok.c',
    # --- 语法糖 族 (排序/切片/聚合/矩阵/窗口 …) (35) ---
    'scripts/feat_adjdiff_zh.c', 'scripts/feat_compact_zh.c', 'scripts/feat_concat_zh.c',
    'scripts/feat_count_zh.c', 'scripts/feat_cumprod_zh.c', 'scripts/feat_destructure_zh.c',
    'scripts/feat_dot_zh.c', 'scripts/feat_fill_zh.c', 'scripts/feat_filter_zh.c',
    'scripts/feat_find_zh.c', 'scripts/feat_flatten_zh.c', 'scripts/feat_interleave_zh.c',
    'scripts/feat_matadd_zh.c', 'scripts/feat_matmul_zh.c', 'scripts/feat_matscale_zh.c',
    'scripts/feat_meanvar_zh.c', 'scripts/feat_median_zh.c', 'scripts/feat_merge_zh.c',
    'scripts/feat_minmax_zh.c', 'scripts/feat_partition_zh.c', 'scripts/feat_peak_zh.c',
    'scripts/feat_rawstr_zh.c', 'scripts/feat_repeat_zh.c', 'scripts/feat_replace_zh.c',
    'scripts/feat_reverse_zh.c', 'scripts/feat_rotate_zh.c', 'scripts/feat_scan_zh.c',
    'scripts/feat_shuffle_zh.c', 'scripts/feat_slice_zh.c', 'scripts/feat_sort_zh.c',
    'scripts/feat_sumprod_zh.c', 'scripts/feat_take_zh.c', 'scripts/feat_wavg_zh.c',
    'scripts/feat_window_zh.c', 'scripts/feat_zh_math.c',
    # --- 语言 特性 锚 (en/C 通道) (88) ---
    'scripts/feat_actor.c', 'scripts/feat_adt.c', 'scripts/feat_adt2.c',
    'scripts/feat_adt4.c', 'scripts/feat_adt_zh.c', 'scripts/feat_arena.c',
    'scripts/feat_arena_zh.c', 'scripts/feat_assert.c', 'scripts/feat_assert2.c',
    'scripts/feat_async.c', 'scripts/feat_bitops_zh.c', 'scripts/feat_borrow.c',
    'scripts/feat_borrow10_zh.c', 'scripts/feat_borrow2_zh.c', 'scripts/feat_borrow_zh.c',
    'scripts/feat_chan.c', 'scripts/feat_charcmp.c', 'scripts/feat_clamp.c',
    'scripts/feat_closure.c', 'scripts/feat_closure2.c', 'scripts/feat_coalesce_zh.c',
    'scripts/feat_compr.c', 'scripts/feat_compr_zh.c', 'scripts/feat_comptime.c',
    'scripts/feat_contract_zh.c', 'scripts/feat_conv_zh.c', 'scripts/feat_coro.c',
    'scripts/feat_coro_zh.c', 'scripts/feat_cptr.c', 'scripts/feat_dbl.c',
    'scripts/feat_declare_zh.c', 'scripts/feat_defer.c', 'scripts/feat_defer2.c',
    'scripts/feat_err.c', 'scripts/feat_exc.c', 'scripts/feat_exc_zh.c',
    'scripts/feat_exhaust_zh.c', 'scripts/feat_fact_zh.c', 'scripts/feat_fold_zh.c',
    'scripts/feat_fptr.c', 'scripts/feat_fptrtbl.c', 'scripts/feat_gen.c',
    'scripts/feat_generic.c', 'scripts/feat_generic2.c', 'scripts/feat_generic3.c',
    'scripts/feat_generic4.c', 'scripts/feat_generic_zh.c', 'scripts/feat_guarantee_fail.c',
    'scripts/feat_guarantee_zh.c', 'scripts/feat_i16.c', 'scripts/feat_i16b.c',
    'scripts/feat_i64.c', 'scripts/feat_inherit.c', 'scripts/feat_interp_zh.c',
    'scripts/feat_lifetime_zh.c', 'scripts/feat_ll_min.c', 'scripts/feat_multiret_zh.c',
    'scripts/feat_mutex.c', 'scripts/feat_namedarg2_zh.c', 'scripts/feat_namedarg_zh.c',
    'scripts/feat_pipe.c', 'scripts/feat_raii.c', 'scripts/feat_range.c',
    'scripts/feat_range2.c', 'scripts/feat_range_chk.c', 'scripts/feat_range_chk2.c',
    'scripts/feat_range_zh.c', 'scripts/feat_select.c', 'scripts/feat_slice.c',
    'scripts/feat_smartptr.c', 'scripts/feat_swap_zh.c', 'scripts/feat_switch_zh.c',
    'scripts/feat_sz3.c', 'scripts/feat_t64.c', 'scripts/feat_t64b.c',
    'scripts/feat_thread.c', 'scripts/feat_transpose_zh.c', 'scripts/feat_tw_min.c',
    'scripts/feat_type_width.c', 'scripts/feat_typedef2.c', 'scripts/feat_union_dbl.c',
    'scripts/feat_unistruct2.c', 'scripts/feat_until_zh.c', 'scripts/feat_us_sz.c',
    'scripts/feat_use_zh.c', 'scripts/feat_vartmpl.c', 'scripts/feat_volatile_zh.c',
    'scripts/feat_wx_zh.c',

    # ==== 地1 G4-1 (2026-09-11): 特性 ↔ 闸内锚 对照 后 补 锚 (「✅ 无 闸内锚」 归零 波一) ====
    'scripts/feat_zh_borrow_split_ok.c', 'scripts/feat_zh_borrow_split_neg.c',
    'scripts/feat_zh_recursive_type_ok.c', 'scripts/feat_zh_macro_hygiene_ok.c',

    # ==== 地1 A21/D1 修复 后 转正 (2026-09-11): 遍-范围 系列 原 长期 挂红 ====
    #   (D1: 迭代变量 预声明 假阳 A4001; A21: 字符串 迭代 长度 取错 表 → 恒 60 次)
    'scripts/feat_range3.c', 'scripts/feat_range4.c', 'scripts/feat_range5.c',
    'scripts/feat_range6.c', 'scripts/feat_range7.c',

    # ==== 地1 A24 (2026-09-11): 命名违规 修正 — 原 feat_zh_leak86_neg 是运行期用例而非编译失败,
    #   按闸纪律 (_neg = 编译失败期望) 改名 feat_zh_leak86_leak + 补 @EXPECTED ====
    'scripts/feat_zh_leak86_leak.c',

    # ==== 地1 A23 (2026-09-11): 管道 |> 优先级 低于 赋值 = 修复后 转正 ====
    #   (原 r = 5 |> add1 |> dbl 被解析成 (r=5) |> add1 |> dbl — 链结果丢弃, r=5)
    'scripts/feat_pipe2.c',

    # ==== 地1 A18 (2026-09-11): 结构体字段 函数指针 返回 浮 → 调用点 补 节浮标 ====
    #   (原 o.区(&r)+1.0 = 541229081.0 — 返回被 消费端 当 整数; 具名函数指针 一直正常)
    'scripts/feat_zh_field_fnptr_ret_ok.c',

    # ==== 地1 A18b (2026-09-11): 文件级 结构体 的 字段 函数指 返回浮 (另 一 条 解析路径) ====
    'scripts/feat_zh_field_fnptr_global_ok.c',

    # ==== 地1 A19 (2026-09-11): 嵌套成员链 双精度字段读 — 编码缺失 修复 ====
    #   (feat_adt3/adt5 为 原 留观 的 老 C 锚, 随 A19 修复 转正)
    'scripts/feat_zh_nested_field_double_ok.c',
    'scripts/feat_adt3.c', 'scripts/feat_adt5.c',

    # ==== 地1 A26-① (2026-09-11): 逗号 连写 双精度 字段 (浮标 跨 逗号 继承) ====
    'scripts/feat_zh_comma_field_double_ok.c',

    # ==== 地1 A26-② (2026-09-11): 内存地址 成员分支 改按构序查 ====
    #   (feat_pattern/unistruct/usr2 为 原 留观 的 老 C 锚, 随 本次 修复 转正)
    'scripts/feat_zh_union_nested_double_ok.c',
    'scripts/feat_pattern.c', 'scripts/feat_unistruct.c', 'scripts/feat_usr2.c',

    # ==== 地1 A27 (2026-09-11): 字段 函数指针 「返回浮」 侧表 补 两个 typedef 分支 ====
    #   (feat_trait 为 原 留观 老 C 锚, 随 本次 修复 由 area=0.0 转正)
    'scripts/feat_trait.c',

    # ==== 地1 A22 (2026-09-11): _Generic 指针 精确 分派 (基类+层级+数组衰减) ====
    #   旧 行为: 任意 指针 命中 第一个 指针 分支 (字符* 选 整数*), 层级 不 参与, 字符数组 不 衰减
    'scripts/feat_zh_generic_ptr_ok.c',
    # ==== 地1 A22/A28 (2026-09-11): 空* 关联 分支 漏 消费 `*` → 关联表达式 被 当 解引用 (静默 错值) ====
    'scripts/feat_zh_generic_voidptr_ok.c',
    #   (feat_concepts / feat_overload 为 原 闸外 留观 老 C 锚: 前者 (字符)0 转型 类型码 缺失 → rc=7,
    #    后者 字符 数组 不 衰减 → r4=1; 本次 A22 双向 修复 后 转正 入闸)
    'scripts/feat_concepts.c', 'scripts/feat_overload.c',

    # ==== 地1 A16 (2026-09-11): 未知 结构体 标签 的 值 声明 → 显式 诊断 (原 静默 跳过 → 隐式变量 垃圾值) ====
    'scripts/feat_zh_struct_unknown_tag_neg.c', 'scripts/feat_zh_struct_decl_ok.c',

    # ==== 地2 G0-4/A30 (2026-09-11): 取环境(getenv) 真读 (原 无导入槽 → 静默 恒空) ====
    'scripts/feat_zh_getenv_ok.c',

    # ==== 地2 G0-2/A31 (2026-09-11): 导入表 已 有 槽 的 函数 必须 可 调用 (白名单 补 13 个) ====
    'scripts/feat_zh_math_import_ok.c',

    # ==== 地2 A34 (2026-09-11): GNU 语句表达式 ({ ... }) 块内变量 作用域 (原 码生成 落空 → 当函数名) ====
    'scripts/feat_zh_struct_fnptr_float_ok.c', 'scripts/feat_zh_printf_spec_ok.c', 'scripts/feat_zh_printf_f_wide_ok.c', 'scripts/feat_zh_printf_int_pad_ok.c', 'scripts/feat_zh_contract_fingerprint_ok.c', 'scripts/feat_zh_annot_cap_neg.c', 'scripts/feat_zh_contract_ret_ok.c', 'scripts/feat_zh_ll_to_double_ok.c', 'scripts/feat_zh_stmtexpr_ok.c', 'scripts/feat_zh_call_widearg_ok.c', 'scripts/feat_zh_fmt_interp_wide_ok.c', 'scripts/feat_zh_closure_cap_neg.c', 'scripts/feat_zh_struct_arr_member_inc_ok.c', 'scripts/feat_zh_pending_tag_ok.c', 'scripts/feat_zh_label_cap_neg.c',
    'scripts/feat_zh_nested_struct_field_ok.c', 'scripts/feat_zh_nested_struct_arrfield_ok.c',
    'scripts/feat_zh_bigfield_ok.c', 'scripts/feat_zh_struct_field_matrix_ok.c', 'scripts/feat_zh_gouarray_field_ok.c', 'scripts/feat_zh_typedef_union_anon_ok.c', 'scripts/feat_zh_typedef_struct_anon_align_ok.c', 'scripts/feat_zh_type_alias_sizeof_ok.c',

    # ==== R3-匿 (2026-09-13): 匿名体 中文 类型别名 (类型 名 = 构/联合/枚 { ... };) + 函数级 别名 真尺寸 ====
    #   文件级 原 解析中断 / 函数级 原 静默 4 (8 字节体 当 4 用 → 运行崩); 词法 补注册 中文关键字 类型定义 ≡ typedef
    'scripts/feat_zh_type_alias_anon_ok.c', 'scripts/feat_zh_type_alias_anon_union_ok.c',
    'scripts/feat_zh_type_alias_local_ok.c', 'scripts/feat_zh_type_alias_anon_local_neg.c',

    # ==== R4 (2026-09-13): `sizeof(((struct T *)0)->字段)` —— 结构词 前缀 (struct/union/构/联) 反射 取字段尺寸 ====
    #   修复前: 判据 只认 (词变量|型词)+`*` → C 形态落 通用表达式 → 码生成 对成员取址 → 未定义函数 'x'
    'scripts/feat_zh_sizeof_ptrfield_ok.c',

    # ==== 序8 (2026-09-13): decl_form_matrix.py 报出的 三 个 零格 形态 —— C 具名标签 typedef ×2 + 别名 反射 ====
    'scripts/feat_zh_typedef_named_tag_ok.c',

    # ==== 探雷 启灵 雷2/雷5/雷6 (2026-09-13): 显式置空 / 未声明标识符 文案 / 实参 成员 逃逸 (I2 假阳) ====
    #   雷1 (接口死循环) 已 于 068da05 修; 雷3 (除零检查) 实测 已 好; 雷4 (返 错 N) 本 批 修
    #   T3 (2026-09-13) 收尾: `可空 变量 = 空;` 赋值置空 真支持 → 原 neg2 (响亮 A8002) **转正** 为 null_assign2_ok
    'scripts/feat_zh_null_assign_ok.c', 'scripts/feat_zh_null_assign2_ok.c',
    'scripts/feat_zh_null_assign_neg.c',
    'scripts/feat_zh_null_struct_decl_ok.c', 'scripts/feat_zh_null_struct_decl_neg.c',  # T3b (2026-09-13) 构值 声明 置空
    'scripts/feat_zh_alias_target_ok.c', 'scripts/feat_zh_alias_target_neg.c',  # D11 (2026-09-13) 类型别名 目标类型 校验
    'scripts/feat_zh_array_2d_sizeof_ok.c',  # D6 (2026-09-13) 多维数组 下标链 大小()
    'scripts/feat_zh_uint_shr_ok.c',  # 位运算族 (2026-09-13) 无符号 右移 (~ 继承 无符号性)
    'scripts/feat_zh_i2_argfield_ok.c', 'scripts/feat_zh_undeclared_var_neg.c',

    # ==== D 系列 收口 (2026-09-13): 5 站 一次 钉死 (审计 探针 到 正锚 的 落闸) ====
    'scripts/feat_zh_union_anon_ok.c',         # D2/D8 裸匿名 union (④ 文件级匿名块 补 联合分派)
    'scripts/feat_zh_array_field_align_ok.c',  # D5/D9 多维数组字段 对齐依据 = 元素尺寸 (与 域行数 拆开)
    'scripts/feat_zh_bitfield_narrow_ok.c',    # D4 位域 容器 宽度 按 基类型 (原 硬编 4 字节槽)
    'scripts/feat_zh_sizeof_addr_ok.c',        # D12/D13 大小(&x) = 8 (原 返 0/随机) + 未初始化 作 大小() 操作数 不误拒
    'scripts/feat_zh_union_array_align_ok.c',  # D14 联合 含 数组成员 的 对齐 (联合字段 补 满卯 汇总)

    # ==== 嵌套匿名体 字段 布局/对齐 矩阵 (2026-09-13): 匿名嵌套 注册 未设 标合 → 总尺寸 12 轮 up 16 ====
    'scripts/feat_zh_nested_anon_matrix_ok.c',

    # ==== T1 (2026-09-13): `大小(裸标签)` 静默 返 4 → 响亮 A1001 (零静默) ====
    #   原: `大小(P3)` = 4 而 `大小(构 P3)` = 12; 裸标签 指针 形态 更 荒谬 (`大小(P3 *)` = 16 静默通过)
    'scripts/feat_zh_sizeof_tag_ok.c',
    'scripts/feat_zh_bare_tag_sizeof_neg.c', 'scripts/feat_zh_bare_tag_sizeof_neg2.c',

    # ==== T2 (2026-09-13): typedef 匿名体 内 的 匿名嵌套 字段 — 原 挂账「整字段丢失」实测 已 好, 锚 钉死 ====
    #   6 形态 (单层/双层/匿名联合/匿名数组/对齐抬升) 全部 与 gcc 逐值 一致: 12 4 8 16 16 32
    'scripts/feat_zh_typedef_anon_nested_ok.c',

    # ==== A15-余 (2026-09-13): MSVC 类型族 升为 类型词 (词法层 单点 展开) ====
    #   `__int64` → `长 长` / `__int32` → `整` / `__int16` → `短` / `__int8` → `有符号 字`; 不动点 504E5F0A.
    #   覆盖 尺寸 全族 (8/8/4/2/1/24) + 大值 取值 + 数组 元素 + 64 位 运算 + 结构体 字段 + 裸类型 局部 声明.
    #   多行/少实参 形态 (刻意 避开 排查 中 发现 的 既存 A47 `sizeof(别名*)` 缺陷); 断言行 = 最后 一行; 与 gcc 逐字节 全等
    'scripts/feat_zh_msvc_types_ok.c',

    # ==== A48 (2026-09-13): `尺寸(类型定义别名*)` 求值错乱 — `尺寸()` 的 typedef 名 4 个子分支 都不消费 `*` ====
    #   触发边界: 别名指针 sizeof 在 **首位 + 后跟实参** 时残留 `*` → 词元错位 (静默错值 64,0; 单独/第二位侥幸正确)
    #   覆盖: 结构体指针/结构体本体/浮点指针/标量指针/typedef 数组本体 (防改坏); 与 gcc 逐字节全等
    'scripts/feat_zh_sizeof_typedef_ptr_ok.c',

    # ==== A53 (2026-09-13): C11 匿名 结构体/联合 成员 (无 变量名) — 原 被 整体 丢弃 ====
    #   字段位置 `结构词 + 左花` 只把 匿名类型 登记到外层 (若 有 变量名); 无 变量名 ⟹ 孤儿 ⟹ offsetof=-1 且 尺寸 少一块
    #   修 = 扁平化 (C11 6.7.2.1): 联合成员 offset 都=基偏 / 结构体成员 累加 / 外层尺寸 = 基偏+匿名体尺寸
    'scripts/feat_c11_anon_member_ok.c',

    # ==== A55 (2026-09-14): `offsetof` 嵌套 member-designator (C99 7.17) — 原只读一个字段名 ====
    #   铁证: `offsetof(s1, buf.b)` 甲言 8 / gcc 16, 而**地址差** `&v.buf.b - &v` = 16 与 gcc 一致
    #        ⟹ 布局没问题, 错的是编译期那个计算 (字段序只给下标, 字段[].毛利 才是类型序)
    'scripts/feat_offsetof_nested_ok.c',

    # ==== A52 (2026-09-14): union 的内联匿名 struct 成员 — 原漏 union 分支 (当结构体累加) ====
    #   同形具名 struct 成员 (u1) 正确; 内联匿名 (u2) 走另一登记点漏了联合语义。插桩一次定位。
    'scripts/feat_union_inline_anon_ok.c',

    # ==== A54 (2026-09-14): `sizeof(<不完整类型>)` 原静默返 0 → 响亮 A1001 (C11 6.5.3.4p1) ====
    #   假尺寸进 `分配内存(n * 大小(X))` 只分 0 → 堆溢出炸弹; 负锚验证「因正确原因失败」
    'scripts/feat_zh_sizeof_incomplete_neg.c',

    # ==== A40g (2026-09-14): printf `%d` 打 INT32_MIN 错 (32位 neg + A39 movsxd 组合残留) ====
    #   根因: 负路径 取反 是 32位 `neg ebx` (对 INT32_MIN 无效: 0x80000000 取反仍自身) →
    #         调用点 A39 `movsxd rbx, ebx` 把 0x80000000 符号扩展成 0xFFFFFFFF80000000 →
    #         域重一 (A37 已改 64位) 按 64位 打印 18446744071562067968 ⇒ "-18446744071562067968"
    #   修: 调用点 movsxd 改 零扩展 (mov ebx,ebx, 89 DB) — 32位 |值| 本身已正确, 高位已是 0 无变化
    #   覆盖: INT32_MIN 作 变量/字面量/多实参交错 位置 + 邻近边界; 与 gcc 逐字节 全等
    'scripts/feat_zh_printf_int_min_ok.c',

    # ==== A56 (2026-09-14): 具标签 无变量名 的 结构体/联合 成员 (C11 6.7.2.1p13 匿名成员) ====
    #   病灶三合一: ① 内联定义链 只有「有变量名」的 `若` (与 A53 同语义 两条路径) → 无变量名 整块跳过;
    #   ② 外层 union 误用 结构体累加 (`union E { int x; union EU {...}; }` 得 16 应 8);
    #   ③ 提升时 只传 6 个参数 → 副优(双精度)/原进(指针)/位域 等 标记位 全丢 (`d.b = 1.5` 报 A9002);
    #   ④ (顺带) 内联定义路径 缺 双精度标记 (既存: `f.fs.b` 读 0.0, 主路径有 `末进一`→`构域浮` 内联路径没有)
    #   覆盖: 外层 struct+内union / 外层 struct+内struct / 外层 union+内union + 提升后 字段读写(含 double) + sizeof + offsetof
    'scripts/feat_zh_tagged_anon_member_ok.c',

    # ==== A62 (2026-09-14): 内联定义 里的 双精度字段 读 0.0 (多条路径 缺 双精度标记) ====
    #   病灶: 无 tag 路径 (L16947 `满再二`) / 主循环 A 块 (`末进一`) 都 设 标记 → `构域浮`;
    #         有 tag 的 多条 路径 (`条件尺寸` / `新尺寸` 两族 共 10 处) 全 无 → double 字段 副优 恒 0 → 读 走 整数 通道 得 0.0
    #   修法 (根治): 全局 `字段浮待` —— 各 路径 识别 double 时 置 1 (每字段 开头 重置), `字段尺寸递归` 一处 统一 消费 标 副优
    #   覆盖: F2 前置定义 / **F1 有 tag 内联 struct** / F4 无 tag 内联 / F6 有 tag 内联 enum; 与 gcc 逐字节 全等
    #   ⚠️ 未修 (另立): F5 有 tag 内联 union (走 `联合字段`, 且 读取 也 异常)
    'scripts/feat_zh_inline_def_double_ok.c',

    # ==== A64 (2026-09-14): 「具标签 无变量名」匿名成员 四矩阵 —— 外层 union + 内层 union (U2) 尺寸错 ====
    #   四矩阵: S1 外struct+内struct 24 / S2 外struct+内union 16 / U1 外union+内struct 16 / **U2 外union+内union 8 (甲言曾 16)**
    #   根因 (探针铁证): 提升时 `字段登记(构序,...)` 把每个提升字段**累加**进外层尺寸 (U2: x→4, p→+4, q→+8=16),
    #     而 max 分支拿**已被污染的当前尺寸**去比 (16>8) ⟹ 永远失效
    #   修法: 字段登记循环前存 `旧尺寸`, max 与**旧尺寸**比; A56(`内构序`)/A53(`深进`) 两处兄弟分支同修
    #   覆盖: 四矩阵 sizeof + 具名对照 N1/N2 + 提升后读写 (union 覆盖语义); 与 gcc 逐字节全等
    'scripts/feat_zh_anon_union_matrix_ok.c',

    # ==== A15-b (2026-09-14): `大小(<名字>)` 静默 返 4 (名字 根本 不存在) ====
    #   病灶: 落 `大小` 的 词变量 分支 (L7538) → 遍 变量表 未命中 → `命中一=0` → `若(!命中一)` 只 处理
    #         裸结构体标签 (T1) / 类型名 (L7487 已分流) → 其余 **静默 保持 `整 尺寸 = 4;` 返出**
    #         ⟹ `sizeof(拼错的名字)` = 4; 假尺寸 进 `分配内存(n * 大小(X))` = 堆溢出 炸弹 (同 T1/A54 危害类)
    #   定位 (无条件探针 铁证): 名=EA 下=18 / 名=fun 下=17 / 名=NoSuchThing 下=18 —— 三者 走 **同一分支**;
    #         交接 里 "`度序` 仅 L7209/L15201 两处理点 且 探针 不触发" 的 矛盾点 = 探针 插错分支
    #         (L7385 属 `大小((X)[0])` 括号 分支, 与 本 路径 无关)
    #   修法 (根治): 判据 = 非裸标签 && 后 不跟 `(` && `全回`(枚举常量表) 未命中 ⟹ 名字 不存在 → A1001
    #   覆盖: 负锚 (不存在 → A1001) + 正锚 (typedef/型词/全局/局部/数组/枚举常量/size_t/无符号字符 全 不误伤)
    #   + **A67 (2026-09-14, 同锚覆盖)**: `大小(函(...))` 产物 0xC0000005 —
    #     病灶: 词变量分支消费名字后只消费 成员链/下标, **不消费实参表 `(`** → 词位残留 → 解析错乱 → 崩
    #       (未改动的 seed 同样崩, 既有缺陷)。修: ① 后跟 `(` ⟹ 消费括号平衡实参表;
    #       ② 新增 **解析期** 返回宽度表 `返宽表` (主循环解析函数头时 `登返宽`) —— 既有 `中细`/`半次一`
    #          与 `源己`/`当三` **都只在码生成器阶段登记**, 解析期查恒 0 (首版实测 ccall=4/dcall=4, 应 1/8);
    #       ③ A15-b 判据补 `查函数纯 < 0` —— 首版漏了 `大小(函数名)`(无调用) 形态 → 误报「未声明」(闸盲区, 本锚补)
    #   ⚠️ 挂账 A68: 单精度返回 (`float f(void)`) —— `函返浮` 不区分 float/double ⟹ 本表记 8 (应 4)
    'scripts/feat_zh_sizeof_name_forms_ok.c',
    'scripts/feat_zh_sizeof_undeclared_neg.c',

    # ==== A50 (2026-09-14): `大小(<浮点字面量>)` —— `f`/`F` 后缀 被 词法层 丢弃 ====
    #   病灶: 词法 **直接丢弃** `f` 后缀, 且 是 **两条路径** (数字开头 `1.5f` L5510 / 句点开头 `.5f` L5767 内)
    #     ⟹ float 字面量 一律 被当 双精度, `大小(1.5f)` 得 8 (应 4). 又一例「同一语义 多条路径」.
    #   修法: 词法 打标记 `词长表[词序] = 3` (1/2 已 用于 长/长整 整数 后缀), `大小` 终简分支 据此 4/8.
    #   覆盖: `1.5f` / `0.5F` / `2.5e2f` 三形态, 与 gcc 逐字节全等.
    #   ⚠️ 本 锚 只 钉 宽度; float 字面量 的 运算/传参 语义 是否 需 单精度 通道 属 另 一 议题.
    # (同站 A68: 单精度 **返回** 的 `大小(函())` —— 见 `feat_zh_sizeof_name_forms_ok.c` 的 fcall 形态)
    'scripts/feat_zh_sizeof_float_lit_ok.c',

    # ==== A69 (2026-09-14): `大小(<表达式>)` —— 运算符 残留 到 外层, 整个 表达式 被 拼错 ====
    #   病灶: `度序` 块 各具体分支 只 消费 开头 的 名字/字面量 就 `返 数` ⟹ 运算符 残留
    #     (实测 `sizeof(x+1)` = **5** 即 `4+1` · `sizeof(x*2)` = **8** 即 `4*2` · `sizeof(p.a?p.a:p.a)` = 1 ·
    #      `sizeof(p.b+1.0)` = 0)。
    #   第二层 根因: 首版 用 `原级()` 消费 —— 但 **原级 只 管 高位** (不 处理 二元 `+` `*`), 仍 只 消费 `x`;
    #     改用 **`三元()`** (顶层: 三元 → 逻辑或链 → … → 乘法 → 原级) 才 消费 完整表达式。
    #   修法: `度序` 块 内 前瞻 ("前 一 token 是 值 且 当前 是 二元 运算符") ⟹ 表达式路径 (`三元()` + 宽度推断);
    #     判据 借此 排除 开头 的 一元 运算符 ⟹ `大小(*p)`/`大小(&v)`/`大小((T*)0->f)` 仍 走 各自 专门分支。
    #   覆盖: 二元算术 / 浮点运算 / 三元 / 乘法 / **额外括号包裹 (t5)**, 与 gcc 逐字节全等。
    #   ✅ A70 (同日闭环): `大小((x + 1))` 原 得 5 (应 4) —— 前瞻 不 跳过 **前导 `(`**;
    #     修 = 扫描前 `循环 (词型表[扫B] == 左括) 扫B++;` + 排他 `乘 后 紧跟 )` (= `((T*)0)` 指针声明符),
    #     反射形态 (`feat_zh_sizeof_ptrfield_ok` / `feat_zh_typedef_named_tag_ok` / `feat_reflect`) 已回归确认.
    'scripts/feat_zh_sizeof_expr_ok.c',
    # A9-余账 #2/#3 (2026-09-14): 逗号表达式 全形态 (表达式语句 / 控制语句体 / for 增量 / 括号 / 返回 / 值语义)
    'scripts/feat_zh_comma_expr_ok.c',
    # P0-3 (2026-09-14): 命令行 -D 宏注入 — 分离形 / 紧凑形 / #ifdef 判定 / #if 数值求值 /
    #   源码文本替换 / 无 = 值 默认 1。靠 @ARG 指令 把 编译器选项 传进 闸 (框架 扩展, 见 run_one_test)
    'scripts/feat_zh_cmdline_define_ok.c',
    # A71 (2026-09-14): 指针变量 多级下标 — 局部 T** / 形参 T** / 字符双指针 / 数组名对照 /
    #   2D 数组对照 / 全局指针数组对照 / 单跳对照 (7 组 与 gcc 逐字节 一致)
    'scripts/feat_zh_ptrptr_index_ok.c',
    # P0-4 (2026-09-14): 命令行 -I <目录> 头文件搜索 — 引号 + 尖括号 双形式 (靠 @ARG 传 -I)
    'scripts/feat_zh_incdir_search_ok.c',
    # P0-6 (2026-09-14): defined()/#ifdef 只认宏, 不认函数/变量 (五种非宏声明 + 真宏对照组)
    'scripts/feat_zh_defined_semantics_ok.c',
    # P0-7 (2026-09-14): #93 内联 误注册 标准 C「跨行花括号」inline 函数
    #   (条件块内同名 #define 文本级激活 → 展开残破宏体 → 括号不匹配; 跨行 + 同行对照组)
    'scripts/feat_zh_inline_hdr_ok.c',
    # P0-8 (2026-09-14): C 原型/声明 登记表 静默上限 256 → 8192 + 溢出响亮报错
    #   (400 条声明 + 末条调用; 确保 登记表 不静默截断)
    'scripts/feat_zh_decltab_ok.c',
    # P0-11 (2026-09-14): 局部「空维度数组 + 花括号初始化」长度推断
    #   (struct / typedef / 尾逗号 三形态; sizeof 断言; 与 gcc 逐字节对照)
    'scripts/feat_zh_local_array_infer_ok.c',
    # P0-14 (2026-09-14): 逗号声明的 续行 declarator —— ① 自身对齐继承 ② 名撞已注册标签 被当类型名吞
    #   (sizeof + offsetof 与 gcc 逐字节对照; 撞/不撞 × 结构体/typedef 双通道 对照)
    'scripts/feat_zh_comma_decl_align_ok.c',
    'scripts/feat_zh_comma_cont_clash_ok.c',
    # P0-15 / P0-16 (2026-09-14): C 兼容模式 的 所有权/借用 检查 误伤 ——
    #   ① `返移节` 缺 甲言模式 门控 → C 源 `return 聚合变量;` 被标已移 (git attr.c)
    #   ② A3105 悬垂引用 漏判 static 局部变量 (存储期 = 程序期)
    'scripts/feat_c_ret_agg_ok.c',
    'scripts/feat_c_static_local_ret_ok.c',
    'scripts/feat_zh_ret_move_neg.c',   # 甲言模式 下 返回移动 仍生效 (回归保护, 期望 编译失败)
    # P0-17 (2026-09-14): 多级指针 解引用 作 三目 分支 (`表达式构序` 丢 指针层级 → T** 的 *q 误判 构值)
    'scripts/feat_c_ternary_ptr_depth_ok.c',
    # P0-19 (2026-09-14): `sizeof(<类型>[<非常量维度>])` 无条件覆盖基类型尺寸
    'scripts/feat_c_sizeof_array_type_ok.c',
    # parenbang 根治 (2026-09-15): sizeof 维度含一元逻辑非 ! 的常量求值 (末列 缺 总零 -> 词位停 ! -> AST 错乱 -> 运行崩)
    'scripts/feat_c_sizeof_bang_dim_ok.c',
    # P0-20 (2026-09-14): `表达式构序` 的 下标/成员 分支 丢指针层级 (P0-17 的解引用姊妹)
    'scripts/feat_c_ternary_ptr_depth2_ok.c',
    # P0-20 未修 (2026-09-14): 数组元素是指针 slot[i] 的三目 (数组 base 且 元素是指针 -> 下标结果 = 指针)
    'scripts/feat_c_ternary_arr_elem_ptr_ok.c',
    # A71-3 根治 (2026-09-15): 解引用「下标结果」宽度 (char** 下标+解引用 用 堆次 而非 变量元素)
    'scripts/feat_c_char_arr_deref_ok.c',
    # A71 第1项 (2026-09-15): T***p 三跳 下标 (嵌套基 逐层传递 码生成内存指+长卯)
    'scripts/feat_c_triple_ptr_ok.c',
    # A71 第2项 (2026-09-15): 指针到数组 (*pm)[i][j] (声明器误判函数指针 + 解引用衰减取址)
    'scripts/feat_c_ptr_to_array_ok.c',
    # A71-续 (2026-09-15): 取址基/解引用基/转型基 成员静态检查 —— 原 只 认 变量基 ⟹ 6/8 漏检 (静默 codegen 垃圾)
    #   修: 节转序[] 记 转型 目标构序 (后缀链() 之前 预标) + 序七() 增 取址 下钻/转型 分支 + 后缀链 检查改用 序七()
    'scripts/feat_c_addr_base_member_ok.c',
    'scripts/feat_c_addr_base_member_neg.c',    # (&s)->typo
    'scripts/feat_c_addr_base_member_neg2.c',   # ((结构体 S*)&s)->typo
    'scripts/feat_c_addr_base_member_neg3.c',   # (*p).typo
    # A71-续2 (2026-09-15): 调用基/赋值基 成员静态检查 —— 序七() 再扩 两家族 (同族复查 q3/q4 收口)
    #   调用: f().typo (构值返回调用作成员基) — 序七 调用分支 (调用构返 退 调用构指)
    #   赋值: (s = t).typo — 序七 赋值分支 (值 = LHS 类型)
    #   ⚠️ 赋值基的 **构值语义** (codegen) 另有既存缺陷 ((s=t).字段 值错/赋值失效, 新旧编译器同错) — 已记台账, 本锚只钉检查不钉取值
    'scripts/feat_c_call_base_member_ok.c',
    'scripts/feat_c_call_base_member_neg.c',    # f().typo
    'scripts/feat_c_assign_base_member_neg.c',  # (s = t).typo
    # A71-续3 (2026-09-15): 赋值表达式作构值成员基 的 **codegen** 根治 (A71-续2 同站发现并挂账, 本轮销账)
    #   ① 内存地址() 增 节点_赋值 分支 (先发射赋值副作用, 再取 LHS 地址)
    #   ② 成员读 "链基" 判据 补 10 (赋值) — 原 只列 12/13/15/4/22 ⟹ 分支加了也走不到
    'scripts/feat_c_call_assign_base_ok.c',
    # A71-续4 (2026-09-15): 局部结构体「小数组字段」(总尺寸 ≤ 8) 点访问 —— 被当 8B 标量读 → 崩/错值
    #   判据 修正: 数组判定 `末凶一(构序,字段)` 前置到「结构体值字段 ≤8B」分支之前 (原 用数值阈值 域尺寸>8)
    'scripts/feat_c_small_arr_field_ok.c',
    # A71-续5 (2026-09-15): 构体整体赋值 的 RHS 家族 (下标/成员/箭头) —— 原 只认 变量/解引用/调用/赋值/三目
    #   `t = a[i]` 落标量兜底 → 只拷首 4 字节; 新增平级分支 (镜像 b = *p 的地址压栈 + 深利)
    'scripts/feat_c_struct_assign_rhs_ok.c',
    # A71-续6 (2026-09-15): 显式转型后无符号性判定 — (整数)<无符号 变量> 应 有符号
    #   (转型不换节点, 原 一律查变量声明 ⟹ 比较/除法 走错通道)
    'scripts/feat_c_unsigned_cast_ok.c',
    # A71-续7 (2026-09-15): 逗号声明中的「数组 + 花括初始化」—— 族 B 根治
    #   (历史: 2026-08-30 csmith 战役记录为「token 错位」, 当时拆独立声明绕开)
    'scripts/feat_c_comma_decl_arr_init_ok.c',
    # A71-续8 (2026-09-15): 二维数组作函数参数 — 行宽 + 指深/指内 登记 (族 A 根治)
    #   形参解析「跳过 [N]」从不登记行宽 ⟹ `整数 a[][3]` 的 a[i][j] 行/列偏移全错
    'scripts/feat_c_2d_arr_param_ok.c',
    # A71-续9 (2026-09-15): 浮点比较 NaN 语义 — 族 C 根治
    #   setb/setbe/sete/setne 不看 PF ⟹ 无序(NaN) 时 全错; < / <= 交换操作数走 seta/setae,
    #   == / != 用 setnp/setp 组合
    'scripts/feat_c_nan_cmp_ok.c',
    # A71-续10 (2026-09-16): 内联匿名结构体 的 位域 — 族 D 根治
    #   两条内联路径 (深进/行散) 都 只 `新升=4` 而 不走 构域位 ⟹ 每 位域 各占 4 字节
    'scripts/feat_c_inline_bf_ok.c',
    # P0-21 (2026-09-14): `extern 结构体 X { ... } 变量;` 内联定义的字段登记不完整
    'scripts/feat_c_extern_inline_struct_ok.c',
    # A75–A75f (2026-09-14): 顶层 内联/匿名 结构体 变量 的 花括 初始化器 (值布契约 / 数组左升 / 设计器 / 多维 行宽)
    'scripts/feat_c_top_inline_struct_init_ok.c',
    'scripts/feat_c_top_array_init_ok.c',
    'scripts/feat_c_anon_array_init_ok.c',
    'scripts/feat_c_top_2d_array_access_ok.c',
    # A76 (2026-09-14): `尾逃逸` 对 数组变量 只看 标量登记 表 -> `&arr[i]` 误 A4001
    'scripts/feat_c_addr_taken_array_ok.c',
    # A77 (2026-09-14): `memset(数组, 0, sizeof)` 首实参 = 写入目标, 原未识别 -> 误 A4001
    'scripts/feat_c_memset_init_ok.c',
    # A78-A81 (2026-09-14): 函数内 内联定义 结构体 变量/数组 花括初始化 —— 三条平行路径 只修了一条
    # (花括=已触标记 / 指针字段标记 / 是否数组判定 / 空维度元素数推断 / 声明初始化 未初始化移除)
    'scripts/feat_c_inline_struct_array_init_ok.c',
    # A95/A95b (2026-09-14): 文件级 extern 函数指针变量 声明 (extern void (*名字)(参数);) 原被静默跳过
    'scripts/feat_c_extern_funcptr_ok.c',
    # A103/A104/A104b (2026-09-14): 元素级 读检查 的 分支汇合 / 嵌套 else-if 上抛 / 循环回边 时机
    'scripts/feat_c_branch_elem_init_ok.c',
    # A107 (2026-09-14): 内建 <limits.h> 的 INT_MIN 少一 / 最小值字面量 溢出 -> 表达式形式
    'scripts/feat_c_limits_min_ok.c',
    # A108/A108b (2026-09-14): extern <类型> 变量 = 初始化; (= 定义) 原静默丢失初始化器
    'scripts/feat_c_extern_init_ok.c',
    # A2 (2026-09-16): 宏体 字符串/字符 字面量 与 注释 内的标识符 被 实参 替换 —— 语料闸根治
    #   副组() 宏体→暂存 的 3 处 循环 裸扫 体[] ⟹ `#定义 F(a) 'a'` 的 a 随实参变 ('a'→'9', "z"→"Q");
    #   实参 含引号 时 (git `OPT__FORCE(var,h,f)` 体 含 'f') 升级为 括号失配
    #   ⟹ git 11 个 builtin/*.c 整文件 编不出 (`[ERR] 括号不匹配 '(' 长简 '}'`)
    'scripts/feat_c_macro_lit_arg_ok.c',
    # A2002 (2026-09-16): 数组 指定初始化器 [enum/宏 常量] = ... 的 尺寸推断 (语料闸 — git gc 根治)
    #   三条 数组推断循环 记 指定下标 时 只认 常数 字面量 ⟹ [K_A] = {...} 不记 终转 ⟹ 尺寸 退化为 项数
    #   ⟹ git builtin/gc.c scheduler_fn[] (4 项 / 最大下标 4) 尺寸算成 4 ⟹ [4] 误报 A2001 越界
    'scripts/feat_c_desig_init_const_idx_ok.c',
    # A2003 + A2004 (2026-09-16): 转型 成员基 的 检查 + 取值 双修 (语料闸 — git merge; builtin 9/11 -> 10/11)
    #   检查: 后缀链 嵌套链 遍历 不看 链中间 的 转型结果 ⟹ 用「字段自身类型」检查 ⟹ 误报 A9001
    #   取值: 内存地址 成员分支 用「基自身声明类型」查字段 ⟹ 字段序 < 0 ⟹ 地址表达式 静默丢弃 ⟹ 写丢
    'scripts/feat_c_cast_member_base_ok.c',
    # A1005 (2026-09-16): 内置/导入表 豁免表 缺 clearerr (语料闸 — git pack-objects; builtin 10/11 -> 11/11)
    'scripts/feat_c_clearerr_ok.c',
    # C4 (2026-09-16): 函数名 与 结构体标签 同名 时 `名字(实参)->字段(...)` 被语句层 当 类型声明 吞掉
    #   ⟹ 后续 `(实参)->字段` 另起 语句, 成员 基 落成 实参 ⟹ 误报 A9001 (语料闸 — git reftable/record.c)
    'scripts/feat_c_dupname_func_tag_ok.c',
    # C8 (2026-09-16): 条件指令行尾的跨行块注释 ==> 「跳过 directive 行」循环只跳行尾, 注释永不闭合
    #   ==> 整个翻译单元被静默丢弃 (rc=0, 目标码 0 字节) — 9 处同族循环补块注释识别
    'scripts/feat_zh_directive_comment_ok.c',
]

# ==== 第 84 站 战线 1 补闸 (2026-09-17): C266 / C292+C295 / C299 三站 **零锚提交** 的补账 ====
#   造锚 过程 里 抓出 4 个 新缺陷 (见 台账 §85/§86)，并 修正 两个 认识:
#     · `甲言模式` 判据 = 中文**关键字**（不是中文标识符）—— 只有中文标识符的 C 文件
#       根本 走不到 所有权检查 ⟹ 拿它当锚 会 永远绿 (假锚)
#     · gcc oracle **对甲言保留字是盲的**（`长`/`整`/`空` 作标识符 gcc 照常编过）
#   入闸 前 过 三关: ① gcc 逐值对照 ② 「没修」的编译器上 必须 变红 (锚灵敏度)
#   ③ 静默兜底 槽计数 = 0 (本类缺陷 的 签名就是静默兜底) —— 靠新增的 `@EXPECTED silent:N` 指令
CORE_FEAT += [
    # C266: 结构体 逗号续行 的 类型序 —— 11 形态矩阵 (gcc 逐值对照通过)
    'scripts/feat_zh_struct_comma_continuation_ok.c',
    # C292+C295: `typedef T *TP;` 别名指针 8 形态 (形参/局部/全局/二级/const/一体写法)
    'scripts/feat_zh_typedef_ptr_alias_ok.c',
    # C299: 所有权「默认移动」的 指针豁免 —— 甲言关键字语法 + 孪生 C 版
    #   (scripts/oracle/feat_zh_own_ptr_exempt_ok_oracle.c 供 gcc 逐值对照)
    'scripts/feat_zh_own_ptr_exempt_ok.c',
    # 闸② 实参必须被解析 (底线: 直调真函数 的 实参 未声明 ⟹ 必报 A1001)
    'scripts/feat_zh_undeclared_arg_neg.c',
    # C332: 中文类型别名(站C) —— **修 C292+C295 的回归** (型定加 少传 2 个实参 ⟹ 取垃圾
    #   ⟹ 别名被当指针别名 ⟹ `字节型 b = 200;` 报 A2001)。含 宽度 钉死 + 别名套别名。
    #   本锚 就是 那个回归 的 网: HEAD(有回归) 上 变红, C332 后 变绿。
    #   (孪生 C 版: scripts/oracle/feat_zh_type_alias_width_ok_oracle.c)
    'scripts/feat_zh_type_alias_width_ok.c',
    # 回归 的 原始 受害者 (启元 09-13 加的 站C 语法锚) —— 它 挂红 两天 无人发现,
    #   因为 C292/C299 两站 都 没 跑 `--zh-feat` 闸 ⟹ 本批 起 每次改源码必须跑 本闸
    #   ⚠️ 第 155 站 (2026-09-20) 去重: 本 锚 在 **段1 (L267)** 已 登记, 此处 原 重复 一行
    #     ⟹ CORE_FEAT 原始 738 条 里 有 1 条 重复 (唯一 737)。删除本行, 账目 归真。
    #     (运行结果 不受影响 —— 同一 锚 跑 两遍 只是 浪费; 但「738」这个 口径 数字 需 修正 为 唯一数。)
    # ==== 第 89/90 站 (战线 4 的 Lua 语料 逼出来): R2 —— 结构体字段的「指向构序」(毛利) ——
    #   ① 第 89 站: 结构体标签 **前向引用** (`struct Dyndata *dyd;` 而 Dyndata 在之后才定义)
    #      原 守卫 `内构序 >= 0` 跳过 ⟹ 字段名 被下一轮 当无类型字段 登记 ⟹ 毛利 = -1 且永无回填。
    #      修法: 补 `堆丁(标签名, 宿主, 字段名)` 延迟回填。
    #   ② 第 90 站: 字段类型是 **typedef 指针别名** (`DP d;`) —— `容成` 只认结构体 ⟹ -1。
    #      修法: 改用 该 typedef 的 `是否指针标`/`指构序` (C292/C295 机制) + 按指针登记。
    #   ⚠️ 匿名体 typedef (`typedef union { … } X;`) 的 字段循环 **无 typedef 分支** ⟹ 同一形态仍错,
    #      根治法 = 并到 `体注册` helper (S5 里程碑 B), 不在本锚覆盖范围。
    'scripts/feat_zh_field_ptr_typedef_ok.c',
    # ==== 第 95 站 R2「嵌套内联体」四形态 —— 标记码 定位 出来的 4 个 解析块 ====
    #   ① `typedef union N1 { struct NK1 { V key_val; … } u; } N1;`  → δ 块 (L17544)
    #   ② `typedef union N2 { struct { V key_val; … } u; } N2;`        → γ 块 (L17508)
    #   ③ `struct N3 { struct NK3 { V key_val; … } u; };` (非 typedef) → 16798 块
    #   ④ ①+ 箭头链 (`p->u.key_val.i`)
    #   病根: 字段 只被 `字段尺寸递归` 建出、`字段类型`(唯一写 毛利 者) 从未调用 ⟹ 毛利 恒 -1 ⟹ A9001。
    #   锚灵敏度: HEAD (qcc_zh_seed) 上 报 A9001 变红 ✓; gcc 逐值对照 PASS ✓ (纯 C 锚, 同源直接跑 gcc)。
    'scripts/feat_zh_nested_inline_struct_ok.c',
#   C99 (2026-09-18 第 99 站): **全局匿名外层 + 内联具名体** (c13) —— 「R2 嵌套体」第 5 形态.
#   本锚 钉死 三处 (C100 结构词分支吞定义体 / C101 内联体缺 typedef 分支 / C102 字段类型链) 缺一必红;
#   锚灵敏度 已验: 在 F1E23049 (C99 之前) 上 编译过 但 运行 0xC0000005 (段错误).
'scripts/feat_zh_anon_global_inline_ok.c',
#   C104+C105+C106 (2026-09-18 第 101 站): 函数内 `typedef struct/union { ... } 别名;` (带体).
#   缺陷: 兜底「跳到首个分号」停在内层字段分号 ⟹ 其后语句被**静默吞掉** (printf 全丢, rc 仍 0).
#   灵敏度: 在 C103 版上 stdout 为空 ⟹ @EXPECTED out: 不符 ⟹ 必红.
'scripts/feat_zh_local_typedef_body_ok.c',
#   C111 (2026-09-18 第 104 站): 「非平凡左值表达式 + 两层链」第二层 基类型 (战线3 m3/m6).
#   ① 调用**返回 typedef 指针别名** 作 链基 (`取()->y->v`): 登记层 不记 函返构序/函返指 (C14 的
#      副作用) + 码生成层 `内存地址()` 调用分支 只处理「按值返回构体」⟹ 整段不发射 (静默 0 + 错值);
#   ② `((SP)p)->y->v`: 转型目标构序预标 门控 `深己==1`, 而 `(SP)p` 词法无 `*` ⟹ 深己==0 ⟹ 静默槽1 x2.
#   灵敏度: 在 C110 版 (4FFF672C) 上 输出 `RPT 0 541073432 1 7` rc=3 ⟹ 必红.
'scripts/feat_zh_call_ret_ptr_chain_ok.c',
#   C111-附 (2026-09-18 第 104 站): 「返回指针的调用」作成员访问的基 (`f(...)->字段`).
#   发现路径 = 跨版本**产物基线对比** —— C110→C112 之间 lua/lparser.c(4 函数)/ltable.c 的 .o 变了,
#     反汇编显示 C112 新增了 C110 **完全没发射**的 `getlocalvardesc(fs,idx)->vd.ridx` 求值
#     (lparser.c L270 赋值右侧整段丢失) ⟹ 旧版**静默错值**.
#   灵敏度: 旧编译器 4FFF672C 上 输出 `RPTF 4208928` rc=1 (必红); 新编译器 `RPTF 42` rc=0.
#   ⚠️ 本锚**必须保持最小**: 同文件里再加任何 `x->vd.字段` 的**变量基**访问, 旧编译器会因
#     「全表扫描兜底恰好命中」而**变绿** (上下文脆弱性, 详见锚文件注释).
'scripts/feat_zh_call_ret_ptr_field_ok.c',
#   ✅ C115/R20 (2026-09-19 第 110 站): 结构体「逗号声明」中 typedef 构体类型的非首字段 类型序.
#   根因 (第 108 站已定位): 文件级 `typedef struct {...} X;` 定义体的字段循环里, 字段类型序候选
#     `实肆` 的声明写在**循环体内** ⟹ 每轮重置 ⟹ 逗号续行 (`Complex alpha, beta;` 的 beta) 拿不到类型序.
#   修法: 声明**移出循环** + 循环体首行按**输入流位置**判定续行
#     (`若 (词位 <= 0 || 词型表[词位 - 1] != 逗号) 实肆 = -1;`) —— 判据与**控制流无关**,
#     故对本循环体内所有 `续;` (continue) 分支同样成立 (r21c 的「循环末尾重置」正被它们绕过 ⟹ Lua 5 件回归).
#   灵敏度: 旧编译器 (F19EBB9D) 上 A9001 编译失败 ⟹ 必红; 新编译器 `CTF 1.5 3 2.5 7` rc=0; gcc 逐值同.
#   覆盖形态: ① 逗号 + typedef 构体 ② 逗号 + 结构体数组 ③ 逗号 + 三字段 ④ 对照: 逗号 + 基本类型.
'scripts/feat_zh_comma_typedef_field_ok.c',
#   ✅ C116/R30 (2026-09-19 第 111 站): 具名 typedef 结构体 `typedef struct X { double re, im; } T;`
#     的「逗号续行 字段 浮点标记」—— 第 2+ 个 double 字段丢标记 ⟹ 写值报 A9002;
#     指针写 `p->im = 3.5` 更隐蔽: **rc=0 但运行值 0.0** (静默错值, 无任何报错)。
#   根因 (与 C115 的 `实肆` 同构): D2 路径 (带标签 typedef 定义体) 的浮点标记候选 `值肆`
#     声明在循环体内 ⟹ 每轮重置。匿名路径 D1 的对应物 `标轮` 早已被 A26 (2026-09-11) 移出循环 ⟹ D1 正常。
#   修法: `值肆` 移出循环 + 循环体首行 `若 (词位 <= 0 || 词型表[词位 - 1] != 逗号) 值肆 = 0;` (与 C115 同一套判据)。
#   灵敏度: 旧编译器 F19EBB9D 上 A9002 编译失败 (必红); 新编译器 `NTF 3.5 2.5 7.5` rc=0; gcc 逐值同。
#   ⚠️ 刻意不含 `const double` 前缀形态 —— 那是另一个既有缺陷 (本轮实测仍 0.0, 已挂账)。
'scripts/feat_zh_named_typedef_float_field_ok.c',
#   ✅ C117/C117b (2026-09-19 第 112 站): 写路径 `*指针 = 表达式` 的**浮点判据**.
#   症状: `*(double*)&v = 3.5;` 读回 0.0 (rc=0, 无报错) —— 写按 4 字节整型落, 读按 double 8 字节解释.
#     仅「指针节点是**变量**」(`double *p; *p = 3.5;`) 时才正确.
#   根因: 赋值分支 `节型表[子0[数]] == 12` 的 `当四` 只认 节点_变量. **读路径早已修 (L24368)**, 写侧漏了.
#   修法: `当四` 补 `|| 节指元[容分] == 21` (A22 精确类型码: 4双 + 16*1 层级 + 1 = 单级 双精度 指针).
#     ⚠️ 首版 C117 照抄读路径的 `指元素 == 8 && 非变量` ⟹ **修好双精度却打破整型**
#       (`*(long long*)&n = 1234567890123` 被当浮点存 → 4745807475586891776), 故收紧为精确码.
#   锚灵敏度 (**双向钉死**): C116 版 (79E07905) 上 ① 读回 0.0 必红;
#     C117 误判版 (qcc_zh_r34a_v1.exe) 上 ② 读回 4745807475586891776 必红; C117b 全绿 `DPW 3.5 1234567890123 2.5 42`.
#   ⚠️ 不含 `*(float*)&f = 3.5f` (另一既有缺陷, C116/C117b 均读回 0.00)、`*(&v)` (取址基)、`((double*)&v)[0]` (下标记).
'scripts/feat_zh_deref_write_float_ok.c',
#   ✅ C118b/C118c (2026-09-19 第 113 站): **(单精度*) 转型 的 解引用 读/写**.
#   症状: `*(float*)&f = 3.5f;` 读回 0.00 (写); `float h = *(float*)&g;` 读回 0.00 (读).
#   根因 (解析期, 非码生成): 括号转型 的 `符六`(指针元素宽度) 判定表 (L8615~8626) 依次判
#     char/short/unsigned/int/long/double —— **唯独没有 `float`** ⟹ `(单精度*)x` 的 `符六=0`
#     ⟹ `指元素` 不设 ⟹ 读/写 的 浮点判据 (`指元素==8` / `变量指单`) 全不命中 ⟹ 落 4 字节整型通道.
#   修法 (3 处, 两刀): ① `符六` 补 `(float/单) = 4`; ② 写路径 `当单` 补 `节指元==21 && 指元素==4`;
#     ③ 读路径 浮点分支 入口 同补 + 内部分支补 单精度读提升 (movss + cvtss2sd).
#     (`名基码()` 对 float/double 都返 4 ⟹ 节指元 都是 21 ⟹ 靠 `指元素` 宽度 4/8 区分 —— 故 ① 是基础, ②③ 是消费端)
#   锚灵敏度 (**三版钉死**): C117b 版 (qcc_zh_r36a_v1.exe) 上 `FPW 0.00 0.00 1.50` 必红;
#     C118b 版 (qcc_zh_r40a_v1.exe) 上 `FPW 3.50 1080033280.00 …` 必红; C118c 全绿 `FPW 3.50 3.50 1.50`.
#   ⚠️ 不含 `((float*)&f)[0]` (下标记) 与 `*(&f)` (取址基) —— 同类判据的另两条挂账, 混入会失去专一性.
'scripts/feat_zh_float_ptr_cast_ok.c',
#   ✅ C119 (2026-09-19 第 114 站): **具名 typedef 的单精度字段**(D2 型词判定表缺 float) + **指针算术作解引用基**(`*(p+1) = 值`).
#   症状: ① `typedef struct TG { float v; } TG; TG t; t.v = 4.5f;` ⟹ A9002「双精度 值 赋给 整型 字段」;
#     ② `float a[4]; float *p = a; *(p+1) = 2.5f;` ⟹ `a[1]` 读回 0.00 (写丢) —— 后者是**极常见**的指针遍历写法。
#   根因: ① 具名 typedef 路径 D2 的型词分支 (L17657) 判定表只有 char/double, **缺 float** ⟹ 值肆(浮点标记) 不设;
#     ② 写路径 `当四`/`当单` 只认 变量基 / 转型精确码 ⟹ **二元基** `p+1` 全不命中 ⟹ 落 4 字节整型存。
#   修法: ① D2 补 `否 若 (float/单) { 域尺寸 = 4; 值肆 = 1; }`; ②③ 新增 `二午`/`二单` 判据并入 `当四`/`当单`。
#   灵敏度: 旧编译器 (qcc_zh_seed_10002ACF.keep) 上 A9002 编译失败 ⟹ 必红; 新编译器 `FPT 4.5 2.5` rc=0。
#   ⚠️ **刻意不含第二个聚合变量** —— 本站最小化实验发现「两个聚合变量地址互相别名」(`TG arr[2]; TG brr[2];`
#      各写各的却都读到后写的值; gcc `9.5 1.5` vs 甲言 `1.5 1.5`), 那是**另一个既有缺陷** (C118c 上同样复现)。
'scripts/feat_zh_named_float_field_ptrarith_ok.c',
#   ✅ C120 (2026-09-19 第 115 站 · R32): **具名 typedef 结构体数组的 float 字段读**.
#   症状: `TG arr[4]; arr[0].v=1.0f; ... arr[3].v=4.0f;` 之后读四个元素 ⟹ 旧编译器 `4.0 4.0 4.0 4.0`
#     (更小: `TG arr[2]; arr[0].v=9.5f;` 后读 arr[0]/arr[1] 都是 9.5)。
#   根因 (反汇编铁证 `r51a_disasm.py`): 读路径「数组[i].字段」分支 (L24440~24446) 只有
#     位宽 / 域尺寸1 / 域尺寸2 / 双字 四路, **无浮点判定** ⟹ float 字段落 32 位整数读 (`rex mov (%rax),%eax`),
#     取值路径随后存的是**上一次浮点操作残留的 xmm0** (`movsd %xmm0,(%rsp)`) ⟹ 读出值与元素下标无关。
#     ⚠️ **地址与下标缩放全程正确** (`shl $0x2,%r11d` + `add %r11,%rax`) —— 「下标被忽略」的早期推断被反汇编推翻。
#     ⚠️ `TD`(double 字段) 数组「看起来正常」是巧合 (8 字节走 64 位整数读, 位模式当 double 用恰好一致)。
#   修法 (1 处): `域尺寸 == 2` 与 `否 {}` 之间插入 `否 若 (终捌单(串值二, 函)) { movss + cvtss2sd }`。
#   灵敏度: 旧编译器 (qqq_zh_seed.exe = DE68F254) 上 `ASF 4.0 4.0 4.0 4.0` 必红; 新编译器 `ASF 1.0 2.0 3.0 4.0` rc=0。
#   ⚠️ 不含 double 字段 (8 字节路径本次不动) 与原生 float 数组 (另走 `例 节点_下标` 独立分支)。
'scripts/feat_zh_struct_array_float_field_ok.c',
#   ✅ C121 (2026-09-19 第 116 站): `*(p+1) = 大数` 的**存储宽度** (二元基 未解析 半再一 → 默认 4 → 32 位存).
#   症状: `long long m[4]; long long *r = m; *(r+1) = 1234567890123LL;` ⟹ 旧编译器 `1912276171` (低 32 位, 静默截断)。
#   根因 (反汇编铁证 `r54a_disasm2.py`): 地址缩放 `imul 8` ✓ 正确, 值 `movabs` 64 位 ✓,
#     但**存**用 `rex mov %eax,(%rbx)` (32 位) —— `半再一` 解析链只覆盖 变量基/解引用基/箭头·成员基/转型指元素,
#     **无「二元基」** ⟹ 默认 4。(与 C119 的 `二午` 是同一处、不同维度: 那是「是否浮点」, 这是「存多宽」。)
#   修法: `半再一` 解析链补「二元节点 && 左操作数是变量 ⟹ 取其指向元素尺寸」。
#   灵敏度: 旧编译器 (qcc_zh_seed.exe = ED4FFA0A) 上 `LLW 1912276171` 必红; 新编译器 `LLW 1234567890123` rc=0。
'scripts/feat_zh_ptr_arith_store_width_ok.c',
#   ✅ C122 (2026-09-19 第 117 站): 结构体数组 **double 字段参与运算** (浮点标记).
#   症状: `TD arr[3]; arr[i].v = …; double s = arr[0].v + arr[1].v + arr[2].v;` ⟹ 旧编译器 **0.00** (gcc 7.50);
#     而**单字段直接打印**却对 —— double 的 8 字节位模式恰好能被当整数值用, 一旦参与运算就露馅。
#   根因: 读路径「数组[i].字段」两分支都**没设 `节浮标[数] = 1`**: ① C120 的 float 分支只做读提升;
#     ② 8 字节分支走 `中发(缓劣,…)` (64 位整数读) + `节长标[数]=1` ⟹ 后续 `+`/`-`/`*` 走整数运算。
#   修法: ① float 分支尾补 `节浮标[数] = 1;`; ② 8 字节分支前置 `若 (终捌(串值二, 函)) { movsd → xmm0; 节浮标[数]=1; }`。
#   灵敏度: 旧编译器 (qcc_zh_seed.exe = 0ACF33B0) 上 `DSF 0.00 0.00` 必红; 新编译器 `DSF 7.50 4.00` rc=0。
#   ⚠️ 不含 `TD *p = arr; p[i].v` (指针基写, 另一既有缺陷: 实测 `0.0 3.5 0.0`) 与
#     「浮点表达式直接作 printf 实参」(`printf("%.2f", b[0].v - b[1].v)` 实测 0.00) —— 均已挂账。
'scripts/feat_zh_struct_array_double_field_arith_ok.c',
#   ✅ C123 (2026-09-19 第 118 站): 局部「类型定义 结构体 指针」的**下标步长**.
#   症状: `typedef struct { double v; } TD; TD arr[3]; TD *p = arr; for(…) p[i].v = 1.5+i;`
#     后**用数组基读** `arr[i].v` ⟹ 旧编译器 `0.00 3.50 0.00` (gcc `1.50 2.50 3.50`)。
#     ⚠️ 读写**都**用 `p[i]` 则两侧同错 ⟹ 输出自洽 ⟹ **假绿** (旧编译器 `1.5 2.5 3.5`)。
#     ⚠️ 纯 int 字段同样错 (`TE *q; q[i].n = i*7;` ⟹ `0 0 0`), 只是更隐蔽。
#   根因 (反汇编铁证 `r64a_disasm6.py`): 局部指针主分支 (主源 L9950) 的 `变偏指(变节, …)`
#     元素尺寸判据只认 型定指元素/字符/短/空/长, **结构体落兜底 4** ⟹ 以 4 字节步进 (TD 实 8, TE 实 16);
#     同行 `指内` 早已有结构体判据, 元素尺寸独缺。写侧 `shl $0x2` vs 数组基 `imul $0x8` 即此。
#   修法: 判据补 `指深一 <= 1 && 整得 >= 0 && 类型表[整得].尺寸 > 0 ? 类型表[整得].尺寸 : …`
#     (指深一<=1: 多级指针元素是指针 8B, 由下一行 数组元素尺寸=8 覆盖)。
#   灵敏度: 旧编译器 (qcc_zh_seed.exe = F0DE22C6) 上 `PSP 0.00 3.50 0.00 0 0 0` 必红;
#     新编译器 `PSP 1.50 2.50 3.50 0 7 14` rc=0。
#   ⚠️ 全局 (`TD *gp`) 与 参数 (`fill(TD *p,…)`) 路径本就正确 —— 本锚只钉**局部**声明。
'scripts/feat_zh_typedef_struct_ptr_subscript_ok.c',
#   ✅ C124 (2026-09-19 第 119 站): 「数组元素字段」的**浮点标记** (解析期 后缀链 缺 下标基).
#   症状: `TD b[3]; b[0].v=5.5; b[1].v=1.5; printf("%.2f", b[0].v - b[1].v);` ⟹ 旧编译器 `0.00` (gcc `4.00`)。
#     ⚠️ **先赋给变量再打印就对** (`double s = b[0].v - b[1].v;`) ⟹ 直接作实参才暴露。
#     ⚠️ 一个操作数是「变量.字段」(`s.v`) 或 纯双精度数组元素 (`c[0]`) 时也不暴露 (各有标记来源)。
#   根因 (反汇编 `r72a_disasm7.py`): 二元判定 `表达式是否浮(左)||表达式是否浮(右)` 在**子节点生成之前**跑,
#     只能依赖**解析期**标记; 而 `后缀链` 设 `节浮标` 的两分支只覆盖 ①`底==节点_变量` ②`底==节点_成员/箭头`,
#     **`底==节点_下标` (`b[0].v`) 落空** ⟹ 两侧无标记 ⟹ 走整数路径 (`push %rax` + `add %eax,%ebx` + `cvtsi2sd`) ⟹ 0.00。
#   修法 (1 处): 分支② 入口条件补 `|| 节型别[底] == 节点_下标` (内部循环早已下标穿透; 序七 对下标基已返元素构序)。
#   灵敏度: 旧编译器 (qcc_zh_seed.exe = 0094DEA9) 上 `FEA 0.00 0.00 0.00` 必红; 新编译器 `FEA 4.00 7.00 9.00` rc=0。
#   ⚠️ 副产物: `数组[i].8字节整字段` 也随 `节长标` 一并修正 (同一入口条件)。
'scripts/feat_zh_struct_array_field_expr_arg_ok.c',
#   ✅ C125 (2026-09-19 第 120 站): 「取址基」`*&X` 的**宽度 / 浮点 / 64 位**判据.
#   症状: `double v = 0.0; *(&v) = 3.5;` ⟹ 旧编译器 `0.00`; `long long w; *(&w) = 1234567890123LL;` ⟹ `1912276171` (静默截断);
#     `double s = *(&v);` (读) ⟹ `0.00`; `*(&t.v)` / `*(&a[0])` 同样错。
#     ⚠️ **整数**形态却对 (`int v; *(&v) = 42;` ✓, 默认宽度恰好 4); ⚠️ **转型基**也对 (`*(double*)&v` ✓, 走 `节指元 == 21`)。
#   根因: 取址节点 `&X` 的 `节指元[]` = 0、`指元素[]` = 0 ⟹ ① 读路径解引用宽度链 (L24320~24362)、
#     ② 写路径存储宽度链 (L23989~24013)、③ 写路径浮点判据 `当四` (L24093) **三处全不命中** ⟹ 宽度落默认 (读 1B/写 4B)。
#     ⚠️ C117b 注释曾声称覆盖 `*(&v)` —— 实测只命中转型基 (`节指元 == 21`), 取址基从未生效。
#   修法 (4 处替换): ① ② 各加 `节点_取址` 分支 (浮点⟹8B+标记; 长整⟹8B+节长标; 变量⟹长壹(名)); ③ 新增 `取址浮` 并入 `当四`。
#   灵敏度: 旧编译器 (qcc_zh_seed.exe = 87D35325) 上 `DAW 0.00 1912276171 0.00 0.00 0.00` 必红; 新编译器 `DAW 3.50 1234567890123 4.50 5.50 3.50` rc=0。
#   ⚠️ 不含 `((double*)&v)[0]` (下标基是**表达式**) —— 另一个独立缺陷, 已挂账 (第 121 站入口)。
'scripts/feat_zh_deref_addr_width_ok.c',
#   ✅ C126/C126b/C126c (2026-09-19 第 121 站): **逗号表达式**的浮点值 (静默取错左操作数).
#   症状: `double r = (7, 2.5);` ⟹ 旧编译器 `7.00` (gcc `2.50`); `(y, 3.5)` ⟹ `2.50` (左值); `printf("%.2f", (x, 2.5))` ⟹ `0.00`。
#     ⚠️ **整数**形态全对 (`int r = (x, 3)` ✓, 非浮点求值路径早有正确逗号分支); ⚠️ `(x, x)` 因左右相同被掩盖。
#   根因: ① `码生成浮` 的二元分支**缺 `丙 == 逗号`** —— 子1 有浮标 ⟹ 进浮点分支, 发 `压浮0/短弊/弹浮0` 却因丙是逗号
#     而四则全不命中 ⟹ **无运算指令** ⟹ xmm0 = 弹回的**左操作数**; ② 实参判别早于码生成, 逗号节点浮标从未被设 ⟹ 整数压栈。
#   修法: ① 加逗号处理 (值 = 右操作数); ② 读侧统一入口 `表达式是否浮()` 加逗号分支 + 写侧 `例 节点_二元` 逗号分支补浮标传递。
#   ⚠️ **C126c 是一条真回归的修正**: GNU 语句表达式 `({…})` 被降级为 `逗号(块, 读临时)` (L8507~L8529),
#     首版无守卫 ⟹ 用「丢弃结果」模式执行那个块 ⟹ 块尾 `临时 = 末表达式` 被 DCE ⟹ 读临时得 0
#     (`feat_zh_stmtexpr_ok` ⑤ 由 5.00 变 0.00)。⟹ **凡对 `节点_二元(逗号)` 做通用处理, 必须排除「子0 是块」的语句表达式形态**。
#   灵敏度: 旧编译器 (qcc_zh_seed.exe = 68AE41C9) 上 `CE 7.00 2.50 7.00 0.00` 必红; 新编译器 `CE 2.50 3.50 2.50 4.50` rc=0。
'scripts/feat_zh_comma_expr_float_ok.c',
#   ✅ C127b (2026-09-19 第 122 站): **嵌套转型** `(双精度)(整数)v` (内层截断被吞).
#   症状: `double v = 3.5; double r = (double)(int)v;` ⟹ 旧编译器 `3.50` (gcc `3.00`, 结果 = 原 v 值);
#     ⚠️ 单层转型全对 (`(int)v` ✓ / `(double)i` ✓); ⚠️ 作**实参**时旧编译器给 `0.00`。
#   根因: `(双精度)X` 是**无操作转型** —— 不建新节点, 直接在 X 节点上置 `节浮标` (L8638); 而
#     `(整数) 于 双精度表达式` 会建**节点19 截断节点** (L8599, 语义 cvttsd2si, 自带 `节浮标[央]=0`)。
#     外层再置 `节浮标[节点19] = 1` ⟹ `码生成浮` 走到 `若 (节浮标[数]) { 码生成(数); 返; }`
#     ⟹ **只发截断不升级** ⟹ 消费端取 xmm0 (更早的浮点残值) ⟹ 得 3.5。
#   修法: **保留**外层浮标 (判定不丢信息), 在 `码生成浮` 里为转型包装节点 (19/27) 加分支
#     `若 (暂 == 19 || 暂 == 27) { 码生成(数); 旧挂(); 返; }` (先算整数再升级, 与「整数立即数→转换」同形)。
#     ⚠️ 首版 C127 用「守卫: 不给 19/27 设浮标」—— 修好赋值路径但**实参路径丢浮点信息** (`0.00`); 本刀两路俱正。
#   灵敏度: 旧编译器 (qcc_zh_seed.exe = D1913329) 上 `NC 3.50 3.50 3.90 3.50 7.00 …` 必红; 新编译器 `NC 3.00 3.00 3.00 3.00 6.00 7.00` rc=0。
#   ⚠️ 不含 `(double)(char)v` / `(double)(short)v` / `(double)(unsigned)v` —— 同族**另一根因**
#     (节点27 的 codegen 假定输入已在 eax、缺 `cvttsd2si`; `(unsigned)` 根本不建截断节点), 已挂账。
'scripts/feat_zh_nested_cast_ok.c',
#   ✅ C128/C128b (2026-09-19 第 123 站): **窄截断 / 无符号转型 的浮点源** (第 122 站挂账的同族根因).
#   症状: `double v=300.7; (double)(char)v` ⟹ 旧编译器 `0.00`; `(double)(short)w` ⟹ `0.00`;
#     `(double)(unsigned)x` ⟹ `3.50` (没截断)。
#   根因: ① `例 节点_窄转型` (L23446) 只做 `码生成(子0)` 然后**直接掩码 eax** (`and $0xff`/`and $0xffff`),
#     而浮点源的结果在 **xmm0** ⟹ eax 是垃圾 (反汇编: `movsd v→xmm0` → `and $0xff,%eax` → 从未从 xmm0 转换);
#     ② `(无符号)v` (浮点源) 在 L8599 不命中 (`(整数)` 截断分支只认 int/整) ⟹ 不建截断节点。
#   修法: ① 加浮点源分支 `码生成浮(子0); 若 (节长标) 长对(); 否 { 中发(宽二); }` (复用 例 节点_转型 写法);
#     ② L8599 条件补 `unsigned/无/无整`, **并排除后续跟 long/长** (那 64 位由下行 long 分支负责)。
#   ⚠️ **锚刻意用「范围内」值** —— 浮点→窄整 的**超范围**转换是 C 的**实现定义行为**
#     (实测 gcc 对 `(char)300.7` 给 `127` = CHAR_MAX, 走**饱和**; 甲言走截断) ⟹ 超范围用例无法做 gcc 逐值对照。
#   灵敏度: 旧编译器 (qcc_zh_seed.exe = 7D5AD2ED) 上 `NCF 44 0.00 4464 0.00 3.50` 必红; 新编译器 `NCF 44 44.00 4464 4464.00 3.00` rc=0。
'scripts/feat_zh_narrow_cast_float_ok.c',
#   ✅ C129/C129b/C129c/C129d/C129e (2026-09-19 第 124 站): **表达式基 下标** (`((双精度*)&v)[i]` / `(&v)[i]`).
#   由来: 第 120 站 (C125) 遗留挂账 K2 (C125 修的是取址基**解引用** `*&X`)。
#   症状: `((double*)&v)[0] = 3.5` ⟹ `0.00`; `((long long*)&w)[0] = 大数` ⟹ `1912276171`;
#     `((double*)&t)[0]` (结构体取址) ⟹ `0.00`; `(&v)[0]` ⟹ `0.00`; 读 ⟹ `0.00`。
#     ⚠️ **整数版巧合正确** (`((int*)&v)[0]` 默认宽度恰好 4)。
#   根因: `例 节点_下标` (读) 与 `例 节点_赋值` 的 `节型表[子0[数]] == 14` 分支 (写) 的基判定
#     只有 串词 / 节点_调用 / 嵌套(14,15), 之后**全部以「变量名」为中心** (`变名 = 节名表 + 子0[数]; 查变量(变名)`)
#     ⟹ 基是**取址/转型节点**时 变名 取到垃圾 ⟹ 无分支命中 ⟹ 宽度落默认 4、地址来源错。
#   修法: C129 读路径加 取址基 分支; C129b 写路径 (L23804) 开头拦截; C129c 两处浮点判定补 `节指元[取址]==21`;
#     C129d/e 补 **索引求值** (`码生成(子1[数])` 读 / `码生成(子1[子0[数]])` 写) + `压栈寄存器(11)` 保护。
#   ⚠️⚠️ **C129d 是本刀的自我纠错**: 首版 照抄「指针变量基」的 `移寄存器(11,0)` 但**漏了索引求值** ⟹ r11 来自
#     **未初始化的 eax** —— 单变量用例 读写同错 ⟹ **自洽假象** (看似通过), 多变量时越界 **崩溃** (0xC0000005)。
#     补索引求值后 C129d 又用错 `子1[数]` (写路径的 `数` 是**赋值节点**, `子1[数]` 是**右值**) ⟹ C129e 改为 `子1[子0[数]]`。
#   灵敏度: 旧编译器 (qcc_zh_seed.exe = C8F94B6C) 上 `EBS 0.00 1912276171 0.00 0.00 0.00` 必红; 新编译器 `EBS 3.50 1234567890123 7.50 5.50 3.50` rc=0。
'scripts/feat_zh_expr_base_subscript_ok.c',
#   ✅ C131 (2026-09-19 第 125 站): **全局(静态) 浮点 指针 的下标**.
#   由来: 第 124 站证明「凡下标基是表达式就落空」后, 本站扫同族形态 —— 用全局指针当基的探针先炸出一片红,
#     分型后确认这是**另一个**缺陷 (与表达式基无关): **只错「全局 + 浮点 + 下标」**, 局部同类写法完好。
#   症状: `static double *gp = g; gp[1] = 99.5;` ⟹ 编译期假阳性 A9002 (双精度 值赋给**整型**数组元素);
#     `double t = gp[0];` ⟹ 静默 `4212992.00` (= &g[0], 地址被当值); `printf("%.2f", gp[0])` ⟹ `0.00`。
#     float 指针 与「指向标量的指针」同病; 全局 int 指针 因默认步长恰好对 (巧合正确)。
#   根因 (三处, 同一类「消费端读不到全局变量的属性」):
#     ① `变量指浮`(L3107) 只扫 [解析基, 变量数) ⟹ 全局/静态 永远查不到 ⟹ 恒 0; 同族 `变量型序` 与 `变量指元素`
#        早在 t46 已改下界 0, 而 A22 另留了含全局回退的 `变量指浮查` 却未接线 ⟹ **解药在源码里, 只是没接上**。
#     ② L18423: `指浮=1` 无条件 (双指针 也判成「指向浮点」⟹ gpp[0][0] 回归); 槽/元素参数恒 4 (单级 double 指针 应 8)。
#     ③ `表达式是否浮`(L5137) 无 节点_下标 分支 ⟹ 上层插 `整转浮` 覆盖已读入 xmm0 的浮点值。
#   修法: ① `变量指浮` 补全局回退 (照 A22 `变量指浮查` 的既有模式, 不动调用点);
#     ② L18423 收窄: 仅 `满转==1 且 非单回` 才 元素 8 / 才置 指浮;
#     ③ `表达式是否浮` 补 节点_下标 分支 (多级指针 返 0, 以 `变量指深查` 守卫)。
#   ⚠️ 本条形态在 719 个 zh-feat 锚里**零覆盖** —— scripts 下的 feat 锚中没有任何全局 double 指针 或 float 指针 声明。
#   灵敏度: 旧编译器 (qcc_zh_seed.exe = BDBEAECB) 在 `gp[1] = 99.5` 处**编不过** (A9002) ⟹ 必红;
#     新编译器 `GFP 1.50 99.50 2.50 4.25 3.50` rc=0, 且与 gcc 逐值一致。
#   ── 以下为 C133 (2026-09-19 第 127 站): **结构体「指针字段」的值语义 / 下标** ──
#   由来: 第 126 站做完「表达式基 下标」后, 剩余 FAIL 里 成员基 四个 (W4/W12/R6/I2) 是**崩溃类**。
#   症状 (旧编译器 05DF5EA8): `TS s; s.p = gp;` 之后运行崩溃 (0xC0000005) —— `s.p` 存的不是指针;
#     `double v = (s.p)[1];` ⟹ 静默 `0.00`; `(s.p)[2] = 7.25;` ⟹ 值未落地。
#     **整数 指针 字段 (`int *r`) 完好** ⟹ 差异只在「字段 指向 double」(分型 M5/M6 对照实现)。
#   根因 (同一族「指针 声明 路径 的 浮标记 未清 / 指针 属性 未登」):
#     ① 字段 声明 路径 识别 双精度/单精度 时 置 浮标记 (值肆/目薄一/末进一 等),
#        其后 的 指针循环 **只 覆盖 域尺寸, 未 清 浮标记** ⟹ 指针 字段 的「字段是双精度」标记 被误置
#        ⟹ 字段 赋值 走 浮点存法 (cvtsi2sd + 双精度移动) ⟹ 存的是 指针值低 32 位 转成的 双精度。
#     ② 「体注册」(函数内 局部 类型定义 的 字段注册 单点收口, 2026-09-18 S5 站86 新增) **从不 登记 指针属性**
#        (原进/符肆/主挂/高贰) ⟹ 局部 结构体 的 指针 字段 被 `字段是否指` 当 非指针 (分型 P3/P4/P5 全红)。
#     ③ 嵌套基 下标 的 写/读 判据 只看 `变量是否浮(链根 结构体 名)` ⟹ 恒 0 ⟹ 写 走 整数存 (值 留 xmm0 未落地)
#        / 读 走 整数读 (double 位模式 当 整数 ⟹ 赋给变量 得 0.00; ⚠️ 直接 作 printf 实参 会 被 格式串 掩盖)。
#   修法 (四刀, 全部 零回归):
#     C133  `字段是否浮` / `终捌` 加 守卫「数据 指针 字段 (原进 且 非 函数指) 不是 双精度」;
#     C133b 「体注册」补 指针 属性 四件套 + 记 指层/指元 (镜像 同族 路径 L17661/L16884);
#     C133c 新增 `终捌指` (指针 字段 指向 元素 是否 双精度, 复用 副优 作载体) ⟹ 写路径 值准备/存法 按它 分派;
#     C133d 读路径 嵌套基 元素 读 补 浮点 分支 + 设 `节浮标[数]`。
#   判据 (零回归): r125b **13/24** (9 → 13; 转绿 W4_member / W12_member_noParen / I2_member_tern_idx / R6_member) ·
#     分型 8/8 + 7/7 (原 4/8 + 1/7) · E 15/15 · F 14/14 · r126a 11/11 · 自举 一步收敛。
#   ⚠️ 本条形态在 720 个 zh-feat 锚里**零覆盖** —— 原锚中没有任何「结构体 + 指针 字段 + 下标」的组合。
#   灵敏度: 旧编译器 (qcc_zh_seed.exe = 05DF5EA8) 在 `s.p = gp` 后即崩 (0xC0000005) ⟹ 必红;
#     新编译器 `SPF 99.50 1.50 7.25 7 8 0` rc=0, 且与 gcc 逐值一致。
'scripts/feat_zh_struct_field_ptr_subscript_ok.c',
'scripts/feat_zh_global_float_ptr_subscript_ok.c',
# ===== 第 134 站 (C144): 语法糖 的 **8 字节整数 (long long) 数组** 形态 (721 → 722) =====
#   由来: 原 糖锚 只 覆盖 整 (4B) 与 双精度 (8B 浮), 长整 数组 三 载体 齐 缺 (槽宽 / 糖后标 元素读 / 解析期 下标 长标记)
#     ⟹ 静默 32 位 截断 (均值 2500000000000 -> 329033728; 反转/排序 丢 高 32; 纯 C 侧 `p = q[1]` -> -1454759936).
#   判据: 旧编译器 (5D9B7CFA) 上 **必红** (fail2 329033728 = 均值 低 32 位); 新编译器 PASS, 与 gcc 逐值一致.
'scripts/feat_zh_sugar_long_ok.c',
# ===== 第 136 站 (C146): 变量名 撞 保留类型词 ⟹ 响亮报错 (722 → 724) =====
#   由来: 第 134 站探针 A6 (`long long 长 = a[1], ...`) 表面 是 「逗号 多声明」, 第 136 站 分型 排除 逗号,
#     定位 真因 = 变量名 `长` 撞了 保留类型词 ⟹ 型词 循环 贪婪 消费 ⟹ 变量 未 登记 ⟹ **静默 0**.
#   两锚: neg 用 **compile_fail** (项目既有机制, 70 个锚在用) —— 修复前 编译 成功 ⟹ 违反断言 ⟹ 天然必红;
#         ok 锁 合法组合 不 误报 (long / long long / unsigned long long / 无 长 长 / 长 长 / 无 long long).
'scripts/feat_zh_reserved_typeword_var_neg.c',
'scripts/feat_zh_long_combo_ok.c',
# ===== 第 140 站 (C140): 复合赋值 全矩阵 —— 补齐 覆盖盲区 (724 → 725) =====
#   由来: 第 140 站 覆盖统计 (766 个 锚): `+=` 66 文件 / `-=` `*=` `&=` `^=` 各 1 / **`/=` `%=` `<<=` `>>=` 零覆盖**
#     —— 整个 复合赋值族 里 一半 运算符 从未 被 任何 锚 测过. 复合赋值 走 「读-改-写」三 步,
#     位宽/符号/浮性 任一 环 错 即 静默 错值; 其中 `/=` 与 第 137 站「变量除法」同族.
#   判据: 26 例 与 gcc 逐值一致; 本锚 不 具「旧编译器 必红」区分度 (是 覆盖补充 而非 缺陷修复),
#     判据 口径 = 锚自身正确 + 全闸零回归 (同 第 133 站).
'scripts/feat_zh_compound_assign_ok.c',
# ===== 第 141 站 (C141): 转义序列补齐 + 零覆盖形态锚 (725 → 727) =====
#   由来: 第 141 站 扩展覆盖统计 (九 维度, 767 个 锚): `\xNN` / `\0` / `\a\b\f\v` 零覆盖;
#     `do { } while` / `void **` / `void *arr[]` / `register` 零覆盖。
#   探针 实测: 转义 是 **真缺陷** (`"\x41\x42\x43"` -> `x41x42x43`; `'\a'` -> 97); 其余 六 形态 行为 正确。
#   判据: escape 锚 **旧编译器 2F19096A 必红** (f1); do-while/voidptr 锚 是 覆盖补充型 (无 必红 区分度,
#     判据 口径 = 锚自身正确 + 全闸零回归)。
'scripts/feat_zh_escape_ok.c',
'scripts/feat_zh_do_while_voidptr_ok.c',
# ===== 第 143 站 (C148): 预定义宏 __LINE__ / __FILE__ / __func__ 修复 + register 补锚 (727 → 729) =====
#   由来: 第 143 站 第三批 覆盖统计 (预处理/位域/属性周边/复杂声明符 四维度, 769 个 锚):
#     `__LINE__` / `__FILE__` / `__func__` / `#pragma` / `#line` / `register` / `extern inline` /
#     `_Noreturn` / `__declspec` / 函数指针数组 / 复合字面量 等 13 形态 **零覆盖 = 缺陷温床**。
#   探针 实测 三个 预定义标识符 是 **真缺陷**: __LINE__ 恒 0 (gcc 4/5) · __FILE__ 恒 "?" (gcc 源路径) ·
#     __func__ 未实现 (A1001 未声明标识符) —— 与 gcc 逐值对照坐实。其余 形态 本站 只 补锚 不 改 代码。
#   判据: predef 锚 **旧编译器 EE6BA87A 必红** (行差 0 / 文件名 "?" / __func__ 编译失败);
#     register 锚 是 覆盖补充型 (无 必红 区分度, 判据 = 锚自身正确 + 全闸零回归)。
'scripts/feat_zh_predef_macro_ok.c',
'scripts/feat_zh_register_ok.c',
# ===== 第 144 站 (C150 + C150b + C152 + C153): 函数指针数组 崩溃根治 + `#line` 指令 (729 → 731) =====
#   由来: 第 144 站 十形态取证 (第三批 覆盖统计 剩余 10 个零覆盖形态, gcc -std=c99 逐值对照):
#     又捞到 **三个真缺陷** —— 函数指针数组 崩 0xC0000005 · 函数指针的指针 A1001 · `#line` 未实现。
#   修法: C150 (暴露元素数) + C150b (消费 `=` 前 插花括分支, 复用唯一出口 左升) + C153 (文件级 用 隔排 登记) —— 函数指针数组 **五路径**;
#     C152 (`#line` 指令: 词法层 `#` 分派 新增分支 + 全局 `引线名`, `__FILE__` 优先取它)。
#   ⚠️ 实测教训: **单靠 C150 无效** —— `=` 在通用分支 已被消费, 判据 永不成立 (「先排除代码是否被执行」再次应验)。
#   判据: 两锚 **旧编译器 (15E7FDF2) 上 必红** (崩 / 得 0 / 行号 8 / 真实路径);
#     ⚠️ 挂账: `(*pp)(7)` (函数指针的指针 **直接解引用调用**) 仍崩 —— 绕过法 `pp[0](7)` 与 `(**pp)(7)` 均可用。
'scripts/feat_zh_fptr_array_ok.c',
'scripts/feat_zh_line_directive_ok.c',
# ===== 第 145 站 (C155 + C155b): 类型词 作 变量名 响亮报错 (撞词家族扩到 类型词/修饰词) (731 → 733) =====
#   由来: 第 144 站 锚 自身 两次 踩坑 (`整 (*单)(整)` / `整 (**双)(整)`) ⟹ 第 145 站 系统取证 (r145a, 11 个 类型词 作 变量名):
#     **10 个 静默错值** (`整 双 = 7;` 读 得 **0**), 只有 `空` 报错. 机理 与 第 136 站 C146 同族 (型词循环 贪婪吃掉撞词变量名 ⟹ 声明变无名),
#     但 C146 只抓「同一 long 类 重复 >2」, 本 修复 抓 **基本类型词冲突/重复** (C155) 与 **型词后 直接 声明终止符** (C155b).
#   ⚠️ **本站最大教训 (白名单 vs 黑名单)**: C155b 首版 用**白名单** (要求 词位 必须是 变量名/结构词/…), 结果 **误报 主源自身**
#     —— 主源 L25948 有 `整 尺寸 = ...` (`尺寸` 是 sizeof 关键字, 被宽容接受作变量名) ⟹ **自举链 第 2 代 失败**;
#     改**黑名单** (`=`/`;`/`,`/`)`) 后: `尺寸` 是关键字不进型词循环 ⟹ 不报 ✓. ⟹ **「解析层全局判据」必须用最小黑名单 + 正面清单锚 (ok 锚) 双锁**.
#   判据: 撞词探针 10/11 报错 (`浮` 合法) · **neg 锚 旧编译器 必红** (编译成功) · **ok 锚 新旧编译器 都必绿** (防误报) ·
#     两代自举 · zh-feat 全量零回归.
'scripts/feat_zh_typeword_clash_neg.c',
'scripts/feat_zh_typeword_combo_ok.c',
# ===== 第 146 站: 第四批覆盖统计 零覆盖形态 补锚 (733 → 735) =====
#   由来: 第四批 覆盖统计 (初始化器 / 字面量常量 / 语句控制流 / 存储类 四维度, 扫 775 个锚) 剩余 4 个 零覆盖:
#     字符数组花括初始化 `{'a','b','c',0}` · `long double` 后缀 L · 宽字符 `L'A'` · **static 函数** (中文 `静` 形态 连 正则 都没扫到).
#   探针 (r146b/c) 逐个 与 gcc 逐值 对照 ⟹ **4/4 全部 已支持** (命中 0 个真缺陷) ⟹ 两 新锚 是 **覆盖补齐型** (无 旧编译器必红 区分度).
#   ⚠️ **重要 发现 (挂账)**: 中文 `字符` 不是 甲言类型名 (char 的中文名 只有 `字`/`字节`) —— `字符 甲[] = {...}` **静默跑偏**
#     (只存首元素 / 显式大小同样坏 / 字符串初始化得空串; 探针 q1 英 ✓ / q2 q3 q6 中 ✗).
#     本站 试 **词法层归一 `字符`→`字`** (照 `无符号`→`无` 先例) ⟹ **自举链 第 2 代 断** —— 主源自身 有 **2933 行** 用 `字符` 作**标识符** (参数名),
#     归一 = 全盘破坏 ⟹ 正解 是「**声明位置** 遇 `字符` 响亮报错 (提示用 `字`)」而非 全局归一, 属 **独立一站**.
#   🔔 收敛判据 (**仍未达成**): 命中率 序列 140=25% → 141=10% → 143=23% → 144=30% → **146=0%**;
#     本批 0% 但 **挂着 1 个已定性的真缺陷 (字符)** ⟹ 判据 口径 修正: 「命中率 ≤10% **且 无已定性的未修缺陷**」才 算收工.
'scripts/feat_zh_chararr_lit_ok.c',
'scripts/feat_zh_static_func_ok.c',
# ===== 第 147 站 (C157): 中文 `字符` 在声明位置 响亮报错 (735 → 736) =====
#   由来: 第 146 站 锚 自身 抓出 + 首版修法 (词法层归一 `字符`→`字`) **失败回滚** (主源 2933 行用 `字符` 作标识符 ⟹ 自举第 2 代断).
#   正解 (C157): 只 在 **声明 类型位置** 拦截 —— 判据 收窄 到 `字符` 后 **紧跟 标识符** (假「类型 变量」形态),
#     不 碰 `字符 = 5;` / `字符[3]` / `字符(` / `字 *字符` (声明符) ⟹ 主源 标识符用法 零命中 (**两代自举 实证**).
#   判据: neg 锚 **旧编译器 (FFFAFA0C) 必编译成功** (静默跑偏) / 新编译器 **必红** A9001;
#     配套 ok 锚 `feat_zh_chararr_lit_ok.c` (用 合法 类型名 `字`) 锁 正确路径.
#   💡 本站 再次 印证: **解析层判据 必须 最小化 + 两侧重验** (第 145 站 四轮收窄 的同型纪律).
'scripts/feat_zh_char_keyword_neg.c',
# ===== 第 149 站: 第五批 覆盖统计 (9 形态 全绿) + **扫雷收口** (736 → 737) =====
#   由来: 第五批 维度 (结构体/联合 · 字符串操作 · 位运算组合 · 类型转换 · 作用域, 扫 775 锚) 剩 9 个 零覆盖:
#     匿构 结构体 全局 · 相邻 字面量 拼接 · 首字符 判 NUL · `%*` 动态宽度 · 掩码与 · 异或交换 · 取位 (>> & 0x) · 清位 &= ~ · 转双精度后 再转.
#   探针 (r149b) 逐个 与 gcc 逐值 对照 ⟹ **9/9 全部 已支持** (命中 **0**) ⟹ 本锚 一次 钉住.
#   🔔 **扫雷收口 判据 达成**: 第四批 0/4 (带 1 个 已定性 挂账 = 中文 `字符` → 第 147 站 已修) · 第五批 **0/9**
#     ⟹ 连续两批 命中率 ≤10% 且 已定性 挂账 已 清 ⟹ **「零覆盖 = 缺陷温床」五批 扫雷 (140/141/143/144/146/149) 到此 收口**.
#   另: 本站 顺带 澄清 两个 旧挂账 **不成立** —— `常量 整 X` 是 **非法组合** (auto 推导 + 显式类型), 报错 正确;
#     `函数 f(整 长)` (参数名 撞 类型词) **实际可用** (实测 `g2 3`) ⟹ 两条 从 挂账 删除.
#   判据: 与 gcc 逐值一致; **无「旧编译器 必红」区分度** (覆盖补齐型).
'scripts/feat_zh_dim5_ok.c',
# ===== 第 153 站: 借用/生命周期/NLL 族 —— 中段 (有实现无锚) 补锚 (737 → 738) =====
#   由来: 大哥 2026-09-20「需要把核心的 15 个机制层打磨得一致」⟹ 每站 1 族 出「三段分层表」;
#     借用族 15 项: 顶 5 (有实现+有锚) · **中 4 (有实现无锚)** · 底 6 (无实现).
#   本锚 覆盖 中段 2 项 —— 都由 **读主源实现点** 核实为真机制 (非脚本 假阳性):
#     · #37 生命周期约束 `存期 名: 父名;` (主源 L11430: 父域 必须 已声明, 否则 A3005);
#     · #36 匿名生命周期 `存期 _;` (主源 L11449: `_` 自动推断, 不占名).
#   另 顺带 `借用 x;` / `放还 x;` (#2/#3 借用标记).
#   判据: 语义正确 · **无 旧编译器必红 区分度** (覆盖补齐型).
#   📋 本族 剩余: 中段 2 (32 闭包捕获模式 · 34 切片分裂) 待补锚; 底段 6 (省略规则/子类型化/协变逆变/借用迁移/冲突诊断/栈借用) 记挂账.
'scripts/feat_zh_lifetime_constraint_ok.c',
    # ===== 第 155 站: 类型系统族 —— 中段 (有实现无锚) 补锚 (737 唯一 -> 740 唯一) =====
    #   由来: 大哥 2026-09-20「需要把核心的 15 个机制层打磨得一致」⟹ 每站 1 族 出 三段分层表;
    #     类型系统族 35 项 (v3.0 #96-#130): 顶 15 (闸内 有锚) · **中 3 (有实现无锚)** · 底 17 (无实现, 多为 IR 后 P2)。
    #   本批 3 锚 覆盖 中段 3 项 —— 全部 由 **读主源实现点** 核实为真机制 (非脚本 假阳性):
    #     · #101 区间类型/范围约束: 主源 L12357 `范围 x 于 下 到 上;` (运行时 上下界检查, 越界 打印+退出)
    #       + L12388 `夹取 x, 下, 上;` (钳制修正, 不回退) + L12365 后升[]/串十[]/虚成[] 三平行登记表;
    #     · #115 trait 继承: 主源 L11965 `接口 子 : 父 { 新方法 }` 合并父方法 + L11990 子覆盖父同名;
    #     · #116 impl Trait: 主源 L18935 `函数 名(参数) -> 实现 接口名 { 体 }` + L10944-10949 返回检查 (A9005)。
    #   ⚠️ 重要 覆盖 发现: `-> 实现` 语法 在 752 个 feat 锚 里 **零覆盖** (本锚 是 首枚)
    #     —— 即「有实现、闸内 零锚」的 典型 中段项, 正是 15 族一致化 要 清 的 那一层。
    #   判据: 三锚 语义 正确 (与 gcc 无关的 甲言 原生语法, 无 gcc 对照面);
    #     **无 旧编译器必红 区分度** (属 覆盖补齐型, 同 第 133/140/141/149/153 站 口径)。
    #   📋 本族 剩余: 中段 0 (已清零) 之外的 底段 17 项 记 挂账 (HKT/行多态/交集/kind/归纳余归纳 等, 均 标 IR 后);
    #     另 #130 类型相等判定 属「编译器内部 机制」(基类型序 链判定), 无 用户可见 语法 ⟹ 需 内部 探针 形态 另议。
    'scripts/feat_zh_range_type101.c',
    'scripts/feat_zh_iface_inherit115.c',
    'scripts/feat_zh_impl_trait116.c',
    # ===== 第 156 站: 并发/异步/线程族 —— 中段 (有实现无锚) 补锚 (740 -> 744 唯一) =====
    #   由来: 同「15 族一致化」⟹ 并发族 30 项 (v3.0 #41-#70) 三段表:
    #     顶 13 (闸内已有锚) · 中 4 (有实现无锚) · 底 4 (无实现: 无锁/线程池/工作窃取/SIMD) · 待核 9。
    #   ⚠️ 本族特征: 实现 = **语法糖脱糖成运行时函数调用** (如 `送 ch, v` → chan_send), 非编译器内建并发;
    #     唯 `原子 加/交换/CAS` 是真 `lock xadd/xchg/cmpxchg` 指令 (见 feat_atomic_*)。
    #   中段 4 项 (读主源实现点核实, 全部在 L11739-L11855 一带):
    #     · #50 条件变量: `条件变量 cv;` 声明 + `唤醒 cv;`/`唤醒全部 cv;` 通知
    #       (另有 `等条件 cv[, m];` → __qcc_cv_wait 释放锁+忙等+重取锁)
    #     · #51 读写锁:   `读写锁 rw;` 声明 + `读锁/写锁/放锁 rw;` → __qcc_rwlock_rd/wr/un
    #     · #56 信号量:   `信号量 sem, n;` 声明 + `获取 sem;`/`放行 sem;` → __qcc_sem_wait/post
    #       (原「#56 有锚」是 sem↔semantics 假阳性: feat_zh_copy_semantics9.c 与信号量无关)
    #     · #58 屏障:     `栅栏 bar, n;` 声明 + `汇合 bar;` → __qcc_barrier_wait
    #       (与 #49 内存屏障区分: `屏障();` → __qcc_mfence, 已有锚 feat_zh_fence21.c)
    #   🔬 重要实测: **n=3 的 `汇合` 在单线程下真忙等 ⟹ 测试进程挂起 (15s 超时被 kill)**
    #     —— 这反证 __qcc_barrier_wait 是**真实现** (真计到达数 + 真自旋), 不是空壳。
    #     故 #58/#50 两锚用「可单线程通过」的降级面 (n=1 汇合 / 只测通知不测等条件), 挂起路径不覆盖。
    #   判据: 四锚语义正确 · **无旧编译器必红区分度** (覆盖补齐型, 同 133/140/141/149/153/155 站口径)。
    #   📋 本族剩余: 底段 4 项 (无锁/线程池/工作窃取/SIMD, 均 0 处源码) + 待核 9 项 (结构化并发/fork-join/取消/Send-Sync/纤程 等)。
    'scripts/feat_zh_condvar50.c',
    'scripts/feat_zh_rwlock51.c',
    'scripts/feat_zh_sem56.c',
    'scripts/feat_zh_barrier58.c',
    # ===== 第 157 站: 元编程/编译期族 —— 中段补锚 (744 -> 746 唯一) + **主源修复一例** =====
    #   由来: 同「15 族一致化」⟹ 元编程族 27 项 (v3.0 #166-#192) 三段表。
    #   本族判定 靠 **甲言语法全集** (317 个 解析层 语法词, 由 `!比较字符串(词名表[词位], "X")` 正则 提取)
    #     —— 取代 猜关键词 (第 156 站 sem↔semantics 假阳性的根治手段)。
    #   🔧 **本站 附带 主源修复 (第 148 站以来 首次改编译器)**: 常量求值器 `末列()` (L15930)
    #     解析 词变量 时 只查 宏表 (全回/域零) 与 帧内变量 (中劣), **唯独不查 `常量 名 = 值;`
    #     写入的 编译期常量表** (剩集[]/外联[], 写于 L12328) ⟹
    #     `常量 甲上界 = 64; 编译期断言(甲上界 == 64);` 报「[ERR] 编译期断言: 表达式不是编译期常量」。
    #     对照实证: `编译期断言(2 == 2)` PASS (证明 == 被支持) 而 具名常量 FAIL ⟹ 定位在「具名常量求值」。
    #     修法: 末列() 词变量分支 补 `池繁(名)` 回退 —— **解药早已在源码里** (池繁() L3393 返索引),
    #     只是没接线 (与 第 125 站「变量指浮查 未接线」同型: 改前先找 是否已有解药)。
    #     不动点 67C7163F -> **15D1E8B8** (自举 一步收敛 v1 == v2)。
    #   📌 两个 语法 实测 (锚 里 已标注):
    #     · 甲言 常量 声明 = `常量 名 = 表达式;` (**不带类型词**; 写 `常量 整 甲 = 2` 会把 常量名 取成 `整` 词)
    #     · `_Static_assert` 只在 **顶层** 被识别 (函数内 报 A1001「未声明函数」)
    #   锚灵敏度 (**双向钉死**, 非「覆盖补齐型」): 新编译器 15D1E8B8 PASS / 旧编译器 67C7163F **必红**
    #     (旧: 「不是编译期常量」/ 断言值 0 ⟹ `_Static_assert 失败`)。
    #   📋 本族剩余: **#189 编译期文件读取 (`嵌入 "文件" 于 数据, 长度;`)** 被 新缺陷 阻塞 ——
    #     实测 报「未声明标识符 '__vec__id'」(疑 嵌入 分支 生成/引用 内部符号 有误), 属 **待定性挂账**;
    #     其余 底段 (184 const泛型 / 185 编译期分配 / 186 编译期哈希 / 187 部分求值 / 188 超级编译) 主源 0 处。
    # ===== 第 170 站: 所有权/内存模型族 —— 中段 (有实现无锚) 补锚 (746 -> 748 唯一) =====
    #   由来: 同「15 族一致化」⟹ 所有权族 25 项 (v3.0 #1-#25) 三段表:
    #     顶 19 (闸内已有锚) · 中 1 (#17 部分移动, 有实现无锚) · 底 5 (13 移动构造细化 / 15 Drop 标记 / 16 Copy 派生 / 21 pin / 22 自引用)。
    #   #17 部分移动 由 **读主源实现点** 核实为真机制 (非脚本假阳性):
    #     L6931 `移态 移标[65536]` (所有权移动状态表: 0=可用 / 1=已移) ·
    #     L7081 `后子()` 查 域六 **路径式** 部分移动 (支持 `s.x` / `a[0]`) ·
    #     L7152 「变量 '%s' 的部分字段已移出, 不能整体移动」· L7177-7199 分支部分移动撤销日志 (A3-e2)。
    #   锚: 正例 (`移入 dst, src.字段;` 只移该字段、其余可用 + 嵌套字段 + 重绑定) + **负例** (部分移出后整体移动 ⟹ 必红)。
    #   判据: **负例自带灵敏度** —— 若 A3106 机制缺失, 负例会编译成功 ⟹ 框架判 compile_fail 失败。
    # ===== 第 171 站: 显式/确定性/错误族 —— 中段 补锚 (748 -> 749 唯一) =====
    #   由来: 同「15 族一致化」⟹ 显式/确定性/错误族 25 项 (v3.0 #71-#95) 三段表。
    #   本锚 覆盖 **#90 错误分类 / #89 可恢复 vs 致命** —— 两者是 **同一机制** (语法层 错误分层):
    #     主源 L10788 语法: `致命错误/可恢复错误/用户错误/系统错误/逻辑错误 "消息";`
    #     L10789-10794: 初值 `净戌一 = 1` (**致命**), **只有 `可恢复错误` 置 0** ⟹
    #       其余四类 打印后 终止程序; 唯 `可恢复错误` 继续执行。
    #   🔴 本站 顺带 **修一个真缺陷**: `可恢复错误;` (**无消息形态**) 崩 0xC0000005 ——
    #     根因: L10799 格式串 恒带 `%s` (`"%s %%s\n"`), 而 L10807 只在有消息时 才加实参
    #     ⟹ 无消息 生成 `printf("[可恢复错误] %s\n")` **无实参** ⟹ `%s` 读 垃圾指针。
    #     修法 (根治): 格式串 随 有无消息 分叉 —— 无消息 时 `"%s\n"` (不带 `%s`)。
    #   锚三关 (**双向钉死**): 旧编译器 8738C415 **崩 3221225477** / 新编译器 F6AF8FA7 **PASS**。
    # ===== 第 172 站: 控制流/异常族 —— 中段 补锚 (749 -> 750 唯一) =====
    #   由来: 同「15 族一致化」⟹ 控制流/异常族 20 项 (v3.0 #146-#165) 三段表:
    #     顶 9 (146/147/148/149/150/151/153/156/164) · 中 4 (152/155/159/162) · 底 6 (154/157/160/161/163/165) · 待核 1 (158)。
    #   #152 尾调用优化 (TCO) 由 读主源实现点 核实:
    #     L10823 语法 `尾调用优化;` (函数体首行) → L10826 `当前函数尾调[单粗] = 1` 标记;
    #     L4531 `旧序[8192]` = 每函数参数复制点标签 (尾递归跳转目标, 跳过前奏栈分配);
    #     L4532 注释「编译器自身源码不用 → 自举不触发 TCO → 收敛稳定」。
    #   锚: **1000000 层 尾递归 不爆栈** ⟹ TCO 生效 (无 TCO 则 0xC00000FD 栈溢出) —— **自带灵敏度**。
    #   ⚠️ 首版探针 用 `累加` 求和 ⟹ 和 5000050000 超 32 位 `整` 溢出 (探针自身错); 改为 倒计数 只测 不崩。
    # ===== 第 173 站: **真接入** #157 模式匹配条件 (变体载荷臂 + 守卫) (750 -> 751 唯一) =====
    #   原状: 主源 L14234 **显式拦截** —— `[ERR] 依据 变体载荷臂 暂不支持 或/守卫 模式 (锤B v1; #98)`。
    #   两刀实现:
    #     ① 收窄拦截 (只拦 `或`, 放开 `守卫`) —— 下游早已具备落地能力:
    #        L14236 守卫解析无条件; L14303 臂内绑定在臂体内生成; L14248 守卫存入臂表随臂发射。
    #     ② **载荷臂补 `池平 = 1` (绑定臂标记)** —— 关键一刀: 原未设 ⟹ 深弊 = 0 ⟹
    #        守卫走 L14287「并入外层条件」路径, 在 **绑定之前** 求值 ⟹ 读未绑定变量 (静默错值);
    #        设后走 L14317「绑定后守卫 → 内层若」路径 ⟹ 语义正确。
    #   语法: `依据 r, x 穷尽 <型名> 对照 <变体>(<绑定...>) 若 <条件>: <值>, ...`
    #   锚: 守卫成立 / 不成立 / 双绑定+算术 三路。
    #   灵敏度 (**双向钉死**): 旧编译器 F6AF8FA7 **报错拒绝** / 新编译器 8A6ACD6B **PASS**。
    #   ⚠️ 挂账: 当 tag **不匹配**该载荷臂时, 该臂仍影响臂选择 (实测 `d.tag = 空点` 时
    #      `u` 保持初值 ⟹ 空点臂未被选中) —— 需 再 一刀 (守卫内层若 置于 tag 相等分支内)。
    'scripts/feat_zh_guard_payload157.c',
    'scripts/feat_zh_tco152.c',
    'scripts/feat_zh_err_class90.c',
    'scripts/feat_zh_partial_move17.c',
    'scripts/feat_zh_partial_move17_neg.c',
    'scripts/feat_zh_ct_assert176.c',
    'scripts/feat_zh_proc_macro172.c',
    # 第 175 站 (#97 真接入): 判别类型 **字符族载荷** —— 原 被主源 显式拦截
    #   `[ERR] 判别 载荷暂不支持 字符 (R1-B 锤A v1 标量集: 整/长/短/双 + 无前缀; #97)` + 退出(1)
    #   语义: `判别 形 于 { 圆(字 c), 方(无符号 字 u), 空点() };` — 载荷 扩展到 1 字节字符族.
    #   七处接线: ① L10533 取消拦截 → 码 3; ② 删 L10537 死闸; ③ L10540 无符号 → 码 7;
    #     ④ L2177 `判载字段` 宽度补 1B 分支; ⑤ L2182 `值劣` 含 码 7;
    #     ⑥ L10568/L10579 解析期 宽度公式 同补 1B (三份拷贝, 漏一 = 静默错值);
    #     ⑦ L14207 依据 绑定端: 码 3 → `是否字符标`, 码 7 → 再 + `无符号标`.
    #   覆盖: 字段直读 / 依据 绑定 / 字节边界 255 (零扩展) / 无符号 字 / union 不串扰 / 混合宽度.
    #   灵敏度 (**双向钉死**): 旧编译器 226AD340 **报错拒绝** / 新编译器 **PASS**。
    'scripts/feat_zh_disc_char_payload_ok.c',
    # 第 176 站 (#97 真接入 续): 判别类型 **8 字节载荷** (`长 长` / `无 长 长`) 解禁
    #   原 被主源 拦截: `[ERR] 判别 载荷 长 长/无 长 长 暂禁 (局部帧 8B 整成员读 码生成缺陷 挂账; #97)`
    #   **挂账复核**: 三条同型修复 (L2000 字段槽垃圾清零 / L4670 `表达式是否长` 成员分支 /
    #     L25192 局部 8B 补 `节长标`) **全在挂账同日 2026-09-04 落地** ⟹ 挂账已过期.
    #   两刀: ① L10541 解除拦截; ② L14207 `依据` 绑定端 补 码 4 → `是否长标` / 码 8 → + `无符号标`
    #     (解禁**暴露的真缺口**: 原落默认分支 ⟹ 绑定变量无长标记 ⟹ 8B 值写 4B 槽 高位截断).
    #   覆盖: 8B 字段直读 / 依据绑定 (长 长 结果通道) / **高位边界 2^53+1** / `无 长 长` 零扩展 /
    #     8B 算术 / union 不串扰.
    #   灵敏度 (**双向钉死**): 旧编译器 74CEC96B **报错拒绝** / 新编译器 **PASS**。
    'scripts/feat_zh_disc_long_payload_ok.c',
    # 第 177 站 (#97 真接入 续): **typedef 别名 作 判别载荷类型** + 消灭误导性报错
    #   原缺口: 载荷类型化解析只认 `词型表[词位] == 型词`（基本类型词）; typedef 别名是
    #     **普通标识符**(词变量) ⟹ 进不去 ⟹ 静默归 码 0 且 类型名被当字段名 ⟹ 报出**误导性**的
    #     「判别 变体名 'v' 是保留字」。
    #   三刀: ① 别名 → 判载码 (用 `型定判断`/`型定查` 取 尺寸/无符号标/浮标 映射到既有 0..8 码;
    #     下游 宽度表 / `依据` 绑定端 / 布局 **全部已就绪, 无需改**);
    #     ② 结构体 载荷 → 准确报错 (原 = 误导性「变体名 是保留字」);
    #     ③ 指针 载荷 → 准确报错 (原 = 「缺字段名」, 信息不准确)。
    #   负例锚 (`_neg`) 钉死 ②③ 的**报错方向**; 正向锚 覆盖 整/字 别名载荷 + 字段直读 + union 不串扰。
    'scripts/feat_zh_disc_alias_payload_ok.c',
    'scripts/feat_zh_disc_struct_payload_neg.c',
    # 第 178 站 (#97 v2 真接入): **指针 作 判别载荷类型**（含 多级指针）
    #   ⚠️ 第 177 站的 指针负向锚 `feat_zh_disc_ptr_payload_neg.c`（断言「指针载荷报错」）
    #     已 **按设计失效** —— 指针载荷 从「报错拒绝」变成「可用」⟹ 该锚 移除, 换 正向锚。
    #   两刀 (10 处): ① 结构 `荷栽目` +载荷元素/指层 · `判载字段()` +判元/判层 两参 (码 9 ⟹ 8B +
    #     `原进=1`/`内联=8`/`符肆`/`主挂`/`高贰`) · 解析遇 `*` ⟹ 码 9 (层数 = 连续 `*` 数,
    #     元素尺寸 = 基类型宽, 层数>1 时 = 8) · 两处宽度公式 +码 9→8;
    #     ② `依据` 绑定端 码 9 ⟹ `是否长标` + `指元素尺寸`/`指深`/`指内`。
    #   覆盖: 字符指针(1B 元素) / 整指针(4B 元素缩放) / **二级指针**(元素 8B, 两次解引用) /
    #     字段直读 / 依据绑定 / union 不串扰。
    #   灵敏度 (**双向钉死**): 旧编译器 CB457658 **报错拒绝** / 新编译器 **PASS**。
    'scripts/feat_zh_disc_ptr_payload_ok.c',
    # 第 179 站 (#97 v2 续 真接入): **结构体标签指针 作 判别载荷类型**
    #   `结构体 点 *p` + `p->字段` 读写。改动: `荷栽目` +`载荷构序` / 解析期 +`判载构p` /
    #   载荷解析「词变量 + 查类型(名)>=0 + 后随 `*`」⟹ 结构体标签指针载荷 (码 9, 预置元素尺寸 =
    #   结构体尺寸) / `判载字段()` +`判构序` 参 ⟹ **`毛利 = 构序`（实测必需: `a.p->x` 的
    #   类型推导依赖它; 置 -1 时读到垃圾）** / `依据` 绑定端 补 `型序标`。
    #   覆盖: 箭头读 / 箭头**写**（写回被指向对象）/ 依据绑定后箭头读 / union 不串扰。
    #   灵敏度 (**双向钉死**): 旧编译器 4D01F208 **报错拒绝** / 新编译器 **PASS**。
    'scripts/feat_zh_disc_structptr_payload_ok.c',
    # ⚠️ **诚实挂账（负例锚钉死）**: **多级 结构体指针 载荷** (`点 **pp`) 暂不支持 ——
    #   实测 `b.pp[0]->x` 读到**相邻内存**（`b.pp = &单层` 正确, 但 `b.pp[0]` 得 0x12042f038）。
    #   对照: 普通结构体字段的二级指针 **正常** / 单级结构体指针载荷 **正常** / 标量多级指针 **正常**
    #   ⟹ 问题限于「判别载荷 × 结构体 × 多级」这一交叉, 需另立一站。
    #   （本站试过 `主挂 = 结构体尺寸` 的改法 —— **反而让单级回归**, 已回退。）
    'scripts/feat_zh_disc_multilevel_structptr_payload_neg.c',
    # 第 183 站 **真接入**: `类型 名 = <裸 结构体标签>;`（别名 直接 指向 结构体）
    #   原 报 A1003「目标 类型 无法 解析」—— 别名解析 只认 型词 / 已有别名 /
    #   **带 `结构体`|`枚举` 前缀** 的具名类型（R3 修复 2026-09-13）; **裸标签** 落最后 ⟹ A1003。
    #   一刀 (L17681): 在「别名套别名」支之前 加 `词变量 && 查类型(名) >= 0` 一支。
    #   灵敏度 (**双向钉死**): 旧编译器 75147182 **报错拒绝**(A1003) / 新编译器 **PASS**。
    'scripts/feat_zh_type_alias_struct_ok.c',
    # 第 183 站 续: **结构体别名 的 指针** 作 判别载荷 (`点型 *p` + `p->字段` 读写)
    #   依赖 上一锚 的 别名改动（否则 走不到 载荷解析）；
    #   = 恢复 第 180 站 撤回 的 改动（`是否构标` 别名 + 后随 `*` ⟹ 预置 元素尺寸/构序,
    #     复用 第 179 站 的 结构体标签指针 通道, 收口 成 码 9）。
    'scripts/feat_zh_disc_alias_structptr_payload_ok.c',
    # 第 185 站 **真接入**: **单精度(float)别名 作 判别载荷** (`类型 单精 = 单;` + `圆(单精 r)`)
    #   原 = 第 177 站 的**保护性报错**（当时 别名只在 `尺==8 && 浮` 时给码 1, 尺 4 的浮 会掉进整数码）。
    #   新增 **码 10 = 单精度 4B**, 4 组: ① 别名分支 码 10; ② 宽度公式 (判载字段 + 解析期两处,
    #     **同式三份**); ③ `副优 = (码==1 || 码==10)` 浮点标记; ④ `依据` 绑定端 码 10 走 `变浮`。
    #   **一次就通**: 单精度读写机制早已齐备（`终捌单(构序,字段名)` = `副优 && 现捌==4` 驱动 movss+cvt）。
    #   覆盖: 字段直读 / 依据绑定 / 与整载荷共存 / union 不串扰。
    #   灵敏度 (**双向钉死**): 旧编译器 0083F342 **报错拒绝** / 新编译器 **PASS**。
    'scripts/feat_zh_disc_f32_payload_ok.c',
    # ==== 第 191 站 (2026-09-21): 可空 指针 字段 置空 (`a.p = 空;`) ====
    #   语义: 可空 指针字段 = **纯 编译期 标记** (H1d-①, L16948), 运行时 空态 = 0 指针
    #     (布局 不变, 零成本) ⟹ `a.p = 空;` 等价 `a.p = 0;`; 判空/窄化/A8001 拦截 全 复用 既有 机制.
    #   原 缺口: T3 置空 (L8848) 判据 = `节型别[数] == 节点_变量` ⟹ 只 覆盖 变量 形态;
    #     字段 形态 落 值位置 `空` → 雷2 收口 (L7437) 报 A8002.
    #   本 站: `赋值()` 内 T3 之后 +2 行 — 成员节点 + `字段可空查(序七(基), 字段名)` 命中 ⟹
    #     消费 `空` 词元, 建 常量 0 替身, RHS 收口 `(空替零 >= 0) ? 空替零 : 管道链()`.
    #   覆盖: 值成员基 / 数组元素基 / 深链基 + 判空 / 窄化箭头读 / 往返 / 幂等 / 无串扰.
    #   灵敏度: 任一 历史 seed 上 本 锚 报 A8002; 新 编译器 PASS (双向钉死).
    'scripts/feat_zh_nullable_field_reset_ok.c',
    #   负例: **指针 基** (`盒 *q` 的 `q->p = 空;`) ⟹ 应 响亮 A8002 — 诚实 挂账:
    #     该 形态 下 「字段 写 0」底层 codegen 有 **既有缺陷** (直接写 0 → 生成 ex_pending 检查 引用
    #     → 「未声明标识符 'ex_pending'」; 经 空替零 绕行 → 编译通过 但 置空 **静默 不生效**);
    #     判据 显式 排除 指针基 ⟹ 宁可 报错, 不 静默 错值 (启元 G2). 修好后 删 本 锚 并 回 正向锚.
    'scripts/feat_zh_nullable_field_reset_ptrbase_neg.c',
    # ==== 第 192 站 (2026-09-21): 内联体 之后 的 `可空` 字段检查恢复 (槽错位 根治) ====
    #   背景: 全编译器 **30+ 处字段循环**, 只有 裸 struct body (L16952) 有 `可空` 处理; 且 它的
    #     槽消费判据 用 **等号** (`可空槽表[可空针] == 词位`), 而 其它 全部 消费点
    #     (L3261/L3263/L3278/L3568/L18830/L19024) 都用 **越过式** (`< 词位`).
    #   ⟹ 内联体/匿名体 子循环 不 消费 槽 ⟹ 残留槽 让 等号 判据 **永不命中**
    #     ⟹ **内联体 之后 的 所有 可空字段 的 可空登记 静默失效** (编译通过, 保护消失).
    #   修法: L16952 行内 +1 语句 `循环 (可空针 < 可空槽数 && 可空槽表[可空针] < 词位) 可空针++;`
    #     —— 先 越过 已过 去 的 残留槽 (与 其它 6 个 消费点 同款), 再 保留 等号 判据 做 登记.
    #     (不能 直接 把 == 改 < — 那会让 残留槽 触发 误登记/误报 A6003.)
    #   灵敏度: 旧 seed (含 第 191 站 9928347F) 上 负例锚 **编译通过** ⟹ 锚 红; 新 编译器 报 A8001 ⟹ 绿.
    'scripts/feat_zh_nullable_field_inline_ok.c',
    #   负例: 内联体 **之后** 的可空字段 未判空解引用 ⟹ 应 报 A8001 (旧编译器 上 静默 通过).
    #     ⚠️ 本 站 只 修 槽错位 —— 实测 `可空` 词 在 typedef/内联体 路径 下 早已 被 正确 消费
    #     (标量字段 布局 正确); 本站 顺带 挖出的 第 6 个 既有缺陷 (typedef 结构体 的 结构体指针
    #     字段 读取 错值, 静默) 已 挂账, 与 本锚 无关.
    'scripts/feat_zh_nullable_field_after_inline_neg.c',
    # ==== 第 193 站 (2026-09-21): `类型定义 结构体` 路径 的 数值型词 判定表 补全 ====
    #   病灶 (静默错值): 裸 struct body (L16952) 的 型词判定表 完备 (char/double/float/long/long long/
    #     short/int/signed + `无符号 长 长` 补充段), 而 `类型定义 结构体 标签 { ... }` 主体循环 (L17926)
    #     的 判定表 **只有 char/double/float** ⟹ `长 长 q` 只消费 第一个 `长`, 第二个 残留 ⟹ 字段 按
    #     默认 4B 登记 ⟹ **8 字节字段 读取 被 32 位截断** (实测 `h.q = 1234567890123` 读出 1912276171 = 低32位).
    #     同根因 也 让 指针字段 读数 截断 (`点 *a` 高位 残留 垃圾).
    #   修法: 判定表 补 long/长/长整 (含 双词元 处理) / short/短 / int/整 / signed/有符号 + 判定表 之后
    #     补 兜底段 (镜像 L16986) 处理 `无符号 长 长 x` 的 残留 `长`. (+2/-1 行)
    #   灵敏度: 旧编译器 (含 1E6C8267) 上 三个 大数 断言 必红 (读 低 32 位) ⟹ 双向钉死.
    'scripts/feat_zh_typedef_struct_numeric_ok.c',
    # ==== 第 196 站 (2026-09-21): `可空 T *p = 空;` 声明初始化置空 (主分支 判据 漏改) ====
    #   病灶 (假阳性报错): `可空 整 *q = 空;` 报 A8002「'= 空' 只能用于可空变量 ('q' 非可空)」.
    #   根因: 置空资格判据 有两个版本 —— `变量可空值` (读 `可空值标`, 而 **指针以 0 哨兵表示可空**,
    #     该标恒 0 ⟹ 对指针恒判「非可空」) 与 `可空声明判` (= 可空值标 || 串壬; T3b-2 / 2026-09-13 新增).
    #     三个 `= 空` 分支里 结构词前缀 (L9293) 与 逗号续行 (L9395) 已改用 `可空声明判`,
    #     **主分支 (L10143) 漏改** ⟹ 普通标量/指针/别名 形态 全部假阳性.
    #   改动: 1 行 —— 主分支 `变量可空值(变节)` → `可空声明判(变节)`.
    #   边界: 非可空变量 仍 响亮报错 (`串壬` 只认 变可空.tag == 无值, 非可空变量 tag 恒 有值).
    #   判据 设计: 锚 用 `p != 0` 直接比较 (不用 `判空`) —— 实测 置空后 `q == 0` 为真 (语义正确),
    #     而 `判空 q` 返假 是 `判空` 判据自身的问题 (对指针走 tag 路径), 属 **另一缺陷** 已挂账.
    #   灵敏度: 旧编译器 (含 B880B4DA) 上 本锚 每个 `= 空;` 行 报 A8002 ⟹ 锚红 (双向钉死).
    'scripts/feat_zh_nullable_decl_reset_ptr_ok.c',
    #   负例: 非可空 变量 的 `= 空` 仍 响亮报错 (放宽的 边界 钉死; 防未来 把 判据 进一步 放宽).
    'scripts/feat_zh_nullable_decl_reset_nonnull_neg.c',
    # ==== 第 198 站 (2026-09-21): 赋值路径 的 指针置空 (`可空 整 *q; q = 空;`) ====
    #   病灶: 报 A8002 (而 声明初始化 `可空 整 *q = 空;` 第 196 站 已修 ⟹ 同根因 另一分支).
    #   根因 (表层): T3 (L8848) 判据 用 `变量可空值` — 指针 以 0 哨兵 表示可空, 该标恒 0.
    #   根因 (深层, 本站 踩坑): 第一版 改用 `可空声明判` **失败** — 插桩 实测 `串壬=0`,
    #     而 反测 (`可空 整 *q; *q` 报 A8001) 证明 tag 落对了 ⟹ `赋值()` 求值 左值 时
    #     `副降一`(L3259 「赋值清除可空标记」) 已清 tag ⟹ `可空声明判` 在 赋值路径 必然失效.
    #   最终修法: 对 **指针** 而言 `空` 本就 是 空指针常量 0 (C 语义) ⟹ 判据 用 `变量指深查 > 0`,
    #     RHS `空` 退化 常量 0 走 普通赋值 (写 0 哨兵), 不进 值类型 打包段.
    #   灵敏度: 旧编译器 (含 33BEB05B) 上 `q = 空;` 报 A8002 ⟹ 锚红 (双向钉死).
    #   负例 边界: **非可空标量** `整 m; m = 空;` 仍 报 A8002 (不能用 `可空 整 n` —— 那是 T3 覆盖的 合法形态).
    'scripts/feat_zh_assign_null_ptr_ok.c',
    'scripts/feat_zh_assign_null_nonptr_neg.c',
    # ==== 第 205 站 (2026-09-21): R26 —— 裸结构体标签名 的 可空指针 声明初始化 (`可空 点 *p = 空;`) ====
    #   病灶: 报 A8002 雷2 (`空` 不是表达式值) —— 而 `可空 结构体 点 *p = 空;` 早已能通
    #     (第 196/198 站 的机制 只覆盖 「型词 / 结构词」前缀 的 声明路径).
    #   根因: `可空` 在 词法层 已被吞 (L6294 只记 槽位), 而 裸标签名 (如 `点`) 在 类型表
    #     但 **不在 标降一** ⟹ 语句分派层 L9780 (未知类型名兜底, 判据要求 查类型 < 0)
    #     与 L9819 (声明总闸) 都认不出它 ⟹ `点 *p = ...` 落 表达式语句 路径.
    #   QCC_DUMP=vars 实测: `可空 点 *p; p = 空;` 的 p 型序=-1 指深=0 (被当 标量可空值, 不是 结构体指针).
    #   修法: 语句分派层 L9780 之前 新增 一支 (不动 既有判据), 以 可空槽位 为 守卫
    #     (语义最窄: 只接管 带 `可空` 前缀 的 该形态), 调用序列 与 结构体指针 分支 同款;
    #     `= 空` 走 常量 0 插入 (与 第 191/198 站 同源机制), `= 其它` 走 赋值级().
    #   灵敏度: 旧编译器 (含 0A5DC193) 上 `可空 点 *p = 空;` 报 A8002 ⟹ 锚红 (双向钉死).
    #   负例 边界: **非可空** `点 *p = 空;` 仍 报 A8002 (新支 以 可空槽位 为守卫, 不接管; 且 结构体形态 亦不受影响).
    'scripts/feat_zh_nullable_bare_tag_ptr_init_ok.c',
    'scripts/feat_zh_nullable_bare_tag_ptr_init_neg.c',
    # ==== 第 206 站 (2026-09-21): R26-余 a —— 裸结构体标签名 的指针 声明初始化 (`点 *p = &t;`) ====
    #   病灶: `点 *p = &t;` 能"通过"编译但 **初始化被丢** —— 后续读 p 报 A4001 (探针 s205t);
    #     `可空 点 *p = &t;` + `p->x` 报「未声明标识符 'p'」(s205y).
    #   根因: 与第 205 站 同一根因 的 另一个面向 —— 裸标签名 进不了 声明路径, `点 *p = ...` 落表达式语句路径:
    #     变量 **登记了** (故不报未定义), 但 `= 初始化` 整段被吞.
    #   修法: 把 第 205 站 那支 的守卫 放宽到 「裸标签名 + `*` + 名」(去掉可空槽位要求),
    #     可空 由 局部标记 可空本 决定 (带可空: 外壹 消费槽 + `= 空` 常量 0; 不带: `= 空` 响亮 A8002).
    #   影响面: 主源自身 0 处 该形态 (scratch_gl/s206a.py 扫描).
    #   灵敏度: 旧编译器 (含 0BC2E059) 上 本锚 报 A4001 ⟹ 锚红 (双向钉死).
    'scripts/feat_zh_bare_tag_ptr_init_ok.c',
    # ==== 第 207 站 (2026-09-21): R26-余 d —— 裸结构体标签名 的指针数组 (`点 *c[2];`) ====
    #   病灶: `点 *c[2];` 能过编译但 c 被登记为 **纯标量** (QCC_DUMP=vars: 数组=0 指深=0)
    #     ⟹ c[0] = &u; c[0]->x 读得 0 (应 5). 对照 `结构体 点 *c[2];` 正常.
    #   修法: ① 守卫 追加 `!型定判断(词名表[词位])` (只接管 裸标签名; typedef 别名 走原路径);
    #     ② 支持 结尾 `[` 的 指针数组形态: 末午(名, 计数, 8) + 定类型(构序,0,-1,-1)
    #        + **显式 指元素尺寸 = 8** (C76 第 76 站 同坑: 否则 结构体元素赋值守卫 按值数组 走深拷贝 ⟹ 0xC0000005).
    #   灵敏度: 旧编译器 (含 E182DE7D) 上 c 被当标量 ⟹ c[0]->x = 0 ⟹ 锚 返 1 (非 PASS) ⟹ 锚红.
    'scripts/feat_zh_bare_tag_ptr_array_ok.c',
    # ==== 第 208 站 (2026-09-21): R22 —— typedef 具名标签 结构体 里, 字段类型 是 裸结构体标签名 ====
    #   病灶: `类型定义 结构体 盒甲 { 点 *a; 整 z; };` 的 字段 a 的 毛利 (指向构序) **从未写**
    #     ⟹ `h.a->x` 类型推导 断链 (第 199 站 打出的 间四=-1) ⟹ 生成码 只 lea 不 [rax] 取值
    #     ⟹ **静默乱码** (老编译器 实测 541245504, 应 42).
    #   根因: ① 该路径 用 `实肆 = 容成(名)` 取 字段类型序, 而 容成() **只认 型定表 的 是否构标 typedef 别名**
    #       —— **裸结构体标签名** (只进 类型表) 恒返 **-1** ⟹ 字段类型() 不调用 ⟹ 毛利 永不写;
    #     ② 域尺寸 也没被设 ⟹ 下一轮 `*` 消费 时 下失 取 默认 4 ⟹ 指针字段 符肆 得 4 (应 8).
    #     铁证 QCC_DUMP=types (老编译器): 盒甲.a 型序=-1/符肆=4/尺寸=16对齐=8
    #       ⇹ 同文件 裸 struct body 路径 同名形态 型序=2/符肆=8/尺寸=12对齐=4.
    #   修法 (+3 行, 行内插入): 容成 失败 时 用 查类型 直取 类型表 序, 并 同步 域尺寸/行
    #     (镜像本分支 既有的 满再>=0 支). 一处兜底 修 **5 个同族形态**.
    #   灵敏度 (双向): 老编译器 (含 071E43BB) 上 本锚 **A9001 编译失败** (值字段形态) ⟹ 必红.
    #   边界: 不影响 typedef 别名 (容成 命中) 与 typedef **指针**别名 (不在 类型表 ⟹ 查类型 返 -1, 仍走原支);
    #     `feat_zh_field_ptr_typedef_ok.c` (②⑦⑧) + `feat_zh_comma_typedef_field_ok.c` (匿名体路径) 守对照.
    #   ⚠️ 余项 R22-余 a: **匿名体** typedef 路径 (L17923 段 那份拷贝) 同一处 无 兜底 ⟹ 仍乱码 (探针 s208e).
    'scripts/feat_zh_bare_tag_field_in_typedef_ok.c',
    # ==== 第 209 站 (2026-09-21): R22-余 a —— **匿名体** typedef 里, 字段类型 是 裸结构体标签名 ====
    #   病灶: `类型定义 结构体 { 点 *a; 整 z; } 盒甲;` 的 字段 a 的 毛利 从未写
    #     ⟹ `h.a->x` 类型推导 断链 ⟹ **静默乱码** (实测 541237328). 与 第 208 站 同源, 走 **另一份拷贝** (L17923 段).
    #   根因: 该段 把 `实肆 = 容成(名)` **也包在 `若 (满再 >= 0) { … }` 块内** —— 裸标签名 的
    #     满再 = 型定查(名) 恒 -1 ⟹ **整块跳过** ⟹ 实肆 不被赋值 ⟹ 毛利 永不写.
    #     ⚠️ 原记「L17923 段 型定表[-1] 越界读」**经验证子代理 2026-09-21 括号配对取证 已证伪**
    #       (该段 5 处 型定表[满再] 访问 全在 保护块内).
    #   修法 (+971 字节, 行内插入): 兜底 **放到 `若 (满再 >= 0) { … }` 块之外**
    #     (守卫 用 满再 < 0 — 不依赖 实肆 跨轮残值), 用 查类型 直取 类型表 序 + 同步 池四.
    #     ★ 第 1 版 (兜底塞进块内) **实测无效** (编译器哈希变、行为一字未变) —— **位置就是关键**.
    #   灵敏度 (双向): 旧编译器 (含 D0EF7862) 上 本锚 **A9001 编译失败** (值字段形态) ⟹ 必红.
    #   一处兜底 修 **5 个同族形态** (老 -> 新): 指针 541237328->42 · 值字段 **A9001 编译失败**->11/12
    #     · 双指针 4347170->42 · 指针数组 **0xC0000005 崩**->42/43 · 逗号续行 541237320->42/43.
    #   边界: 匿名体 + typedef **指针**别名 对照 仍 `E 9 5` 不变; 第 208 站 全部探针 零回归.
    'scripts/feat_zh_bare_tag_field_in_anon_typedef_ok.c',
    # ==== 第 210 站 (2026-09-21): R26-余 b —— **裸结构体标签名 的 多级指针** ====
    #   病灶 (老编译器 C63D55A7 实测, 4 形态全坏): ①`可空 点 **p = 空;` **A8002**
    #     · ②`可空 点 ***p = 空;` A8002 · ③`点 **p;` **「未声明标识符 p」**(连声明都没登记)
    #     · ④`点 **c[2];` **0xC0000005 崩**.
    #   根因: 第 205~207 站 新增支 的 守卫 只认 **单级** (`词位+1 乘 && 词位+2 词变量`)
    #     —— 多级 的 词位+2 是 `*` 而非 词变量 ⟹ 不匹配 ⟹ 落原路径.
    #   修法 (+947 字节, 两处): ① 守卫 改「数级数」—— 循环 数 连续 `*` 得 级丙,
    #     新守卫 = `级丙 >= 1 && 探丙 指 词变量 && 结尾 是 赋号/分号/左方`; `词位 += 2` ⟹ `词位 += 1 + 级丙`.
    #     ② `定类型(构丙, 0, 1, 尺寸)` ⟹ `定类型(构丙, 0, 级丙, 尺寸)` (第 3 参 = 指深, 见 L3540).
    #   一处改动 修 4 形态 (老 -> 新): ①A8002->null=1/null=0 x=42 y=7 · ②A8002->null=1 x=42
    #     · ③未声明标识符->42 7 · ④0xC0000005 崩->42 43.
    #   灵敏度 (双向): 旧编译器 (含 C63D55A7) 上 本锚 **A8002 编译失败** ⟹ 必红.
    #   零回归: 单级对照 `可空 点 *p = 空;` 不变; **边界** `点 **p = 空;`(非可空) **仍报 A8002**.
    'scripts/feat_zh_bare_tag_multi_ptr_ok.c',
    # ==== 第 211 站 (2026-09-21): R26-余 f —— 裸结构体标签名 的 **逗号声明带初始化** ====
    #   病灶 (老 82394F60 实测): ①`点 *p = &t1, *q = &t2;` **「未声明标识符 q」**
    #     · ②三变量 同报 · ③`可空 点 *p = 空, *q = 空;` **A8002**.
    #   对照 (本来就对): 带 结构词 / 标量 `整 a = 7, b = 9;` / **无初始化** `点 *p, *q;`.
    #   🔑 关键鉴别: **只有「带初始化」才坏** ⟹ 病灶 精确落在 第 206/207/210 站 为 `= 初值` 新增的那支.
    #   根因: 该支 只处理 **首个变量** (`续;` 交还), 而 `, *q = ...` 落 语句分派层 无人接管 被整体吞掉
    #     —— `赋值级()` = `管道链()`, **`,` 天然终止表达式** ⟹ 词位停在 `,` 上 ⟹ `q` 从未登记.
    #   修法 (+2034 字节, 一处插入): 本支收尾 `若 (词型表[词位] == 分号)` **之前** 加 **逗号续行循环** ——
    #     与首项 **逐字同款** (数 `*` 级数 → 变偏指 → 定类型(深=级丁) → 可空 外壹 → `= 空` 常量 0 / `= 其它` 赋值级());
    #     非本族形态 (级丁<1 或 非词变量) ⟹ 断 交还.
    #   实测 (老 -> 新): ①「未声明标识符 q」->`A 42 43` · ②同 ->`E 42 43 44` · ③`A8002`->`D 1 1 42 43`(两个都置空).
    #   灵敏度 (双向): 旧编译器 (含 82394F60) 上 本锚 **编译失败** (A8002 / 未声明标识符) ⟹ 必红.
    #   零回归: 带结构词 / 标量 / 无初始化 三对照 逐字不变.
    'scripts/feat_zh_bare_tag_comma_init_ok.c',
    # ==== 第 212 站 (2026-09-21): R23 —— **裸结构体标签名 局部指针变量** 的 成员链检查（销账站）====
    #   挂账原文: 「局部指针变量 型序标 未登记 —— `序七(局部 盒 *q)` = -1
    #     ⟹ A8001/表达式构序/链检查 **一整类静默失效**」.
    #   本站跨代 QCC_DUMP=vars + 行为探针 查明: **R23 早在 第 206 站 就被顺带修好**, 只是当时没人发现:
    #     | 33BEB05B(196) · 0A5DC193(198) · **0BC2E059(205)** | 变[…] q 型序=-1 指元素=0 指深=0 | **静默放行 + 垃圾值 4347122** ✗ |
    #     | **E182DE7D (第 206 站)** · 当前 | **型序=2 指元素=8 指深=1** | **报 A9001** ✓ |
    #     ⟹ 第 206 站那支 (本为修 `点 *p = &t;` 初始化被丢) 在做 变偏指 + 定类型(…级丙…) 时 顺带把 型序标 也设上了.
    #   ⚠️ 本形态必须「声明与赋值分离」—— 带初始化那种在 ≤0BC2E059 上先报 A4001, 会**掩盖**要钉的 A9001 边界.
    #   ⚠️ 已排除 (非缺陷): `q->n = "字符串";` 类型不匹配 在 三代编译器 上 均静默, 但 **gcc 同形也只是 warning**.
    #   文件名含 _neg ⟹ 框架视为 compile_fail: 老编译器 静默放行 ⟹ 锚红; 新编译器 报 A9001 ⟹ 锚绿.
    'scripts/feat_zh_bare_tag_local_ptr_field_neg.c',
    # ==== 第 215 站 (2026-09-21): **C2 定案落地·第一刀** —— `错误 可空 T` 三态单标记 ====
    #   宪法依据 (大哥 2026-09-12 定案 · 终身不换): 可空 = 类型构造器 (Option<T>), 错误 = 类型构造器 (Result<T, 错误码>), 二者可嵌套.
    #   C2 定案: `错误 可空 T` 合法, 用**一个**标记表达三态 (无错/空/错误码); 保留值 待 查清 语料错码分布 再钉.
    #   **本站查证结论**: `错 N;` 打包 {码(低32), tag=1(高32)} (L12566); `解构(v,t)` 拆 64 位 (L14578);
    #     **tag==0 = 成功 ⟹ 0 已被占用**; 错码 是 池编号 (错误码池[4096]) 与 tag 空间独立
    #     ⟹ **「空」保留值 = tag == 2**.
    #   两处改动 (+1884 字节): ① `可空` 修饰返回类型 原一律 A6003 ⟹ **错误函数内 (右陆==1) 放行**;
    #     ② `原级()` 雷2 (`空` 在值位置 A8002) ⟹ **错误函数内** 构造打包 {码=0, tag=2}
    #        (镜像 L12566; **必须复用 节值表=9001** —— 那是 码生成 识别的 打包语义标记).
    #   三态: tag=0 有值 · **tag=2 空** · tag=1 错 (码在低 32).
    #   灵敏度 (双向): 旧编译器 (含 154B64D4) 上 本锚 **A8002 编译失败** ⟹ 必红.
    'scripts/feat_zh_err_nullable_three_state_ok.c',
    # 第 216 站 (2026-09-21) **C2 定案落地 · 步 2**: `尝试` 对「空」(tag=2) 的传播语义.
    #   病灶: `尝试` 判据 原为 `tag == 1` (只传播「错」) ⟹ 三态上线后 **tag=2「空」落进成功侧 被静默当有值**
    #     (实测 `传(0)` 返 t=0 v=100, 而正确应为 t=2 v=0).
    #   修法 (+354 字节): 判据改 **「非 0 即传播」** —— `比等(tag,1)` ⟹ `名庚(tag,0)`, **两处都改**
    #     (① 8/16/32 捕获分支; ② int 打包分支). 等价性: tag ∈ {0,1} 时两式等价 ⟹ 对既有锚零回归.
    #   ⚠️ 符号坑: 主源 有 **两套 比较符号** —— AST 层 (高平/顺八/下双) 与 **码生成层 (比等/名庚/比小)**;
    #     此处构造 码生成层 节点, 必须用 **名庚** (= !=, 见 L2747 高进 `置不等`); 主源**无** `比不等` 这个符号.
    #   灵敏度: 旧编译器 (373BE39B) 上 本锚 **编译通过 但 运行 exit=3** (t != 2) ⟹ 必红.
    'scripts/feat_zh_err_nullable_try_propagate_ok.c',
    # 第 217 站 (2026-09-21) **C2 定案落地 · 步 2 余项（构造侧）**: `返 空` 在 8/16/32 类载荷 的 tag 生成.
    #   病灶: `9001` 打包 {child0=值/码, child1=tag} 的返回侧码生成 (3 处, 按载荷类分派) 判据**全是
    #     「child1 是常量 且 == 1」** 且 tag **硬编码 1** ⟹ `返 空` 的 child1 = 常量 **2** 时三处全假
    #     ⟹ 8B 落「多返回值打包」路径 (rax=0x200000000) / 双精度 tag 全丢 (t=0) / 大结构体同族.
    #   修法 (+603 字节 · 3 处): 判据放宽 `== 1` ⟹ `!= 0`; tag 取 **child1 的常量值**.
    #   零回归: 现有锚 child1 恒为常量 1 ⟹ 逐字等价.
    #   灵敏度: 旧编译器 (F8B77A09) 上 本锚 编译通过、**运行 exit=1** (8B 行 t != 2) ⟹ 必红.
    'scripts/feat_zh_err_nullable_payload_empty_ok.c',
    # 第 218 站 (2026-09-21) **C2 定案落地 · 步 2 余项（传播侧）**: `尝试` 传播的 tag 保真 (8B / 双精度 / 大结构体).
    #   链条: 216 站 判据改「非 0 即传播」(int 路径保真) → 217 站 码生成 3 处判据放宽 (构造侧 保真)
    #     → 本刀 补上「child1 = 变量」的码生成处理 (3 处) + 解析侧 child1 改用捕获的真实 tag (2 处).
    #   各载荷类通道: 32 类 值走 sret (rdx=错码, rax=tag) · 16 类 值在 xmm0 (rdx=tag) · 8 类 **值占 rax**
    #     ⟹ 8 类必须 **先**求 tag 压栈 再求值 最后弹栈到 rdx (唯一需注意求值顺序处).
    #   灵敏度: 上一代编译器 (36085967) 上 本锚 编译通过、**运行 exit=1** (8B 行 t != 2, 得 1 = 降级为「错」) ⟹ 必红.
    'scripts/feat_zh_err_nullable_try_propagate_payload_ok.c',
    # 第 219 站 (2026-09-21) **C2 定案落地 · 步 3**: `判空 f()` 对「错误 可空 T」的语义.
    #   定案: `判空 f()` ≡ `解构(_,t); t == 2` —— **tag == 2 才算空** (三态: 0 有值 / 2 空 / 1 错).
    #   病灶 (静默错误): `判空 f()` 编译通过但 `若/否` 结构错乱 —— 实测 **两分支都执行**;
    #     根因 = 判空实现**无条件**把下一 token 当变量名消费 (`字 串进[64]; 复制字符串(串进, 词名表[词位]); 词位++;`)
    #     ⟹ `判空 找(0)` 把**函数名** `找` 吃掉 ⟹ `(0)` 落进 `若` 条件.
    #   修法: 消费前判 **`词变量 + 左括`**(调用形态) ⟹ 表达式路径: 当申=1(检查点) → 赋值级() →
    #     **节长标=1 (必须 64 位长读, 否则 >>32 恒 0)** → 构造 `((expr >> 32) == 2)`.
    #   两套标记共存: 普通 `可空 T` 变量 空 ⇔ tag 1 (既有, 不改); `错误 可空 T` 表达式 空 ⇔ tag 2.
    #   灵敏度: 上一代 (D4B353D5) 上 本锚 编译通过、**运行 exit=1** ⟹ 必红.
    'scripts/feat_zh_err_nullable_check_ok.c',
    # 第 220 站 (2026-09-21) **R29 真接入**: **裸结构体标签名 的 文件级 全局变量** `盒 变量;`.
    #   病灶: `盒 全局乙;` 在 QCC_DUMP=vars 里**完全没有** ⟹ `&全局乙` 取址 查变量 = -1 ⟹ 残留寄存器 ⟹ 0xC0000005 崩
    #     (实测旧编译器: `乙.a=4354247` 乱码后 exit=0xC0000005). 属「暴露而非回归」(第 208~211 站修好字段/指针形态后暴露).
    #   侦察: 打点 R29-IN 证明 `结构体 盒 {...}` 与 `结构体 盒 全局甲;` 都命中**顶层结构体分支**,
    #     而 `盒 全局乙;` **完全不出现** ⟹ 走另一条路 (解释了第 214 站两次改结构体分支守卫**均无效** = 改错了层).
    #   前两版失败 (都改**语句层**): ① 只放宽 ⟹ 内建 prelude 误命中, v1 整体损坏 (任何文件报 `未声明标识符 j`,
    #     多登记 38 变量); ② 收窄后不损坏但 `全局乙` 仍不登记 ⟹ 证明文件级形态**不走语句层**.
    #   本版: **顶层新增一支**, 用 L17593 的正确姿势 `中退(变量名, 构序, 1)` (结构体值全局变量登记).
    #   灵敏度: 上一代 (28C31DF0) 上 本锚 编译通过、**运行崩溃** (0xC0000005) ⟹ 必红.
    'scripts/feat_zh_bare_tag_global_ok.c',
    # 第 221 站 (2026-09-21) **C3 定案落地**: 除零 —— **错误联合函数内 = 固定错误码** · 普通函数内 = 保持终止.
    #   定案 (大哥 09-12 宪法): 「除零 → 错误联合函数内 = 固定错误码; 普通函数内 = 保持终止」. 改前**两者都终止**.
    #   机制 (侦察): 除零检查**不在码生成**而在**解析期** —— 乘法() (L8865) 末尾把「除法的除数」换成 __qcc_div_check(除数) 调用
    #     (中 IR 的 暂重/值成 只发 idiv, 无检查).
    #   修法 (+1959 字节 · 1 处插入): 错误联合函数内改用「若除数为 0 则返回固定错误码」的块:
    #     { 长整 __dz_N = <除数>; 若 (__dz_N == 0) 返回 打包{码=2147483647, tag=1}; <左> / __dz_N }.
    #     固定码 2147483647 (INT_MAX): 用户错码是「小池编号」, 不撞; 除数经 __dz 变量**只求值一次**.
    #   ⚠️ 第一版失败: 判据用 `半错一(当解析函)` (函数表查询) ⟹ **解析期函数表尚未登记完** ⟹ 恒假 ⟹ 不生效;
    #     改用**解析期标记 `右陆`** (尝试 用的就是它) ⟹ 立即生效.
    #   灵敏度: 上一代 (55340773) 上 本锚 编译通过、**运行终止** ([除零] + exit(1)) ⟹ 必红.
    'scripts/feat_zh_div_zero_err_ok.c',
    # 第 222 站 (2026-09-21) **C3 余项**: 复合赋值 `a /= b` / `a %= b` 的除零处理.
    #   病灶: 除零注入只在 乘法() (二元 / %), 而复合赋值走 赋值() 的展开路径 (L8935: a = a 算子 b) ⟹ **完全无检查**
    #     ⟹ 硬件崩 0xC0000094 (实测 错误函数内 `a /= b;` 调 (10,0) 直接崩, 无任何诊断 — 比二元形态更糟).
    #   修法 (+2313 字节 · 1 处插入): 在展开处对二元节点应用与第 221 站**同一套**构造
    #     (错误函数内 ⟹ `{ 长整 __dzC_N = <右值>; 若 (__dzC_N == 0) 返回 打包{码=2147483647, tag=1}; <左> 算子 __dzC_N }`).
    #   ⏳ 遗留挂账: **普通函数内**的 /= 0 仍崩 —— 复合赋值**缺** __qcc_div_check 注入路径 (需另刀接上).
    #   灵敏度: 上一代 (AEEF3C61) 上 本锚 编译通过、**运行崩溃** (0xC0000094) ⟹ 必红.
    'scripts/feat_zh_div_zero_compound_err_ok.c',
    # 第 223 站 (2026-09-21) **C3 余项 2**: **普通函数内** 复合赋值 `a /= b` / `a %= b` 的除零处理.
    #   病灶: 复合赋值处**从未**注入 __qcc_div_check ⟹ `a /= 0` 直接 idiv ⟹ 硬件崩 0xC0000094 (无诊断).
    #     对照: 二元 `/` 普通函数内 走运行库检查 ⟹ `[除零]` + exit(1) 优雅终止.
    #   修法 (+1057 字节): 在 赋值() 的复合赋值展开处 补「普通函数内」分支, 除数换成 __qcc_div_check(除数)
    #     (与 乘法() 既有做法逐字同款, 含 32/64 位选择).
    #   断言口径: `out:[除零]` 消息级 (沿用 feat_zh_bound_divzero.c 教训 —— 只断言 exit:nonzero 会把「硬崩」误判成通过).
    #   灵敏度: 上一代 (FA95D610) 上 本锚 运行崩溃 (0xC0000094) ⟹ 无 [除零] 消息 ⟹ 必红.
    'scripts/feat_zh_div_zero_compound.c',
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
    """最新自举产物: qcc_zh_v7 → ... → v1 (审计 2026-09-07: 原硬编码 v7, 仅烟测 v1 时 fallback gcc)

    G0-3 (2026-09-11) 反馈提速: 环境变量 QCC_TEST_BIN 可 指定 编译器 —— 小改 时 用
    「旧 seed 编 当前 源」 出 的 探针 编译器 只 跑 目标锚 (无 链 / 无 seed 交换 / 无 全闸)。"""
    env = os.environ.get('QCC_TEST_BIN')
    if env and os.path.exists(env):
        return env
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
    expected_err = None
    expected_silent = None   # 闸 (2026-09-17 第 84 站): `@EXPECTED silent:N` —— 静默兜底 槽计数 断言.
                             #   C266/C292/C299 这一类缺陷 的 **签名就是静默兜底**, rc=0 不等于没缺陷;
                             #   声明了才查 (未声明的不受影响, 零回归风险).
                             #   ⚠️ 只在 编译成功 时 可断言 (编译器 的 汇总行 在 成功收尾处 才打印).
    dep = False; multi = False
    arg_extra = []          # @ARG <编译器选项> (2026-09-14 P0-3): 闸 原 只 能 传 固定 [qcc, src, -o, out],
                            #   命令行 -D 宏 类 特性 完全 无法 覆盖 — 补 此 指令 (与 F5 闸盲区 同族: 闸 够不着 即 盲区)
    try:
        head = open(src, encoding='utf-8', errors='replace').read(600)
    except Exception:
        head = ''
    for line in head.split('\n'):
        m = re.search(r'//\s*@EXPECTED\s+exit:\s*(\S+)', line)
        if m: expected = m.group(1)
        m = re.search(r'//\s*@EXPECTED\s+out:(.+)$', line)
        if m: expected_out = m.group(1).strip()
        m = re.search(r'//\s*@EXPECTED\s+err:(.+)$', line)   # G0 (2026-09-11): stderr 断言 —
        if m: expected_err = m.group(1).strip()              #   运行时诊断 (如 [LEAK]/[ERR]) 走 stderr, stdout 断言 覆盖 不到
        m = re.search(r'//\s*@EXPECTED\s+in:(.+)$', line)
        if m: stdin_in = m.group(1).strip()
        m = re.search(r'//\s*@EXPECTED\s+silent:\s*(\d+)', line)   # 第 84 站: 静默兜底 槽计数 断言
        if m: expected_silent = int(m.group(1))
        if '@EXPECTED compile_fail' in line or '_neg' in os.path.basename(name): expect_cfail = True
        if '@DEP' in line: dep = True          # 显式声明: 依赖配套文件/外部符号 → 单文件编译失败 = 许可 skip (2026-09-07 收紧)
        if '@MULTI' in line: multi = True      # 显式声明: 需 -c + jyld 跨 .o 链接 → 单文件无主/外链失败 = 许可 skip
        m = re.search(r'//\s*@ARG\s+(.+)$', line)
        if m: arg_extra += shlex.split(m.group(1).strip())   # 2026-09-14 P0-3: 编译器 选项 (shlex 切分: `-D A=1` 与 `-DA=1` 两 形 皆可)

    # 编译 (输出名用 basename, 兼容 behavior/ 子目录; 并行安全: 每测试独立 exe 名)
    h1 = os.path.join('scratch_test', os.path.basename(name)[:-2] + '_H1.exe')
    try:
        os.remove(h1)
    except OSError:
        pass
    r = subprocess.run([qcc] + arg_extra + [src, '-o', h1], capture_output=True, text=True,
                       encoding='utf-8', errors='replace',
                       env=(dict(os.environ, QCC_DBG_SILENT='1') if expected_silent is not None else None))

    # 静默兜底 槽计数 断言 (第 84 站): 只在 编译成功 时 判定 (汇总行 在 成功收尾 才打印)
    if expected_silent is not None and r.returncode == 0:
        allout = (r.stdout or '') + (r.stderr or '')
        ms = re.search(r'本编译 共 (\d+) 次', allout)
        silent_n = int(ms.group(1)) if ms else 0
        if silent_n != expected_silent:
            return (name, False, '静默兜底 %d ≠ 期望 %d (%s)' % (
                silent_n, expected_silent,
                ' '.join(re.findall(r'槽\d: [^\n]*', allout))[:120]))

    if r.returncode != 0 or not os.path.exists(h1):
        if expect_cfail:
            # 闸盲区修补 (2026-09-14): 原 直接 return PASS —— 连 "编译器 优雅 报错" 与 "编译器 自身 崩溃"
            # (0xC0000005/0xC0000094 也 是 非零退出) 都 不 区分, 更 不 检查 @EXPECTED out:/err: 断言
            # (既有 `err:A1001` 类 断言 因此 是 装饰). 现: 合并 编译器 两 路 输出 后 逐条 校验 断言。
            # ⚠️ 注意 甲言 编译器 的 诊断 实测 落 **stdout** (非 stderr) ⟹ 两 路 合并 检查。
            cerr_all = (r.stderr or '') + (r.stdout or '')
            if expected_out and expected_out not in cerr_all:
                return (name, False, 'compile_fail 期望 out 含 %r 实际=%r' % (expected_out, cerr_all.strip()[:150]))
            if expected_err and expected_err not in cerr_all:
                return (name, False, 'compile_fail 期望 err 含 %r 实际=%r' % (expected_err, cerr_all.strip()[:150]))
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
        err = (p.stderr or '').strip()
    except subprocess.TimeoutExpired as te:
        try:
            te.kill()
        except Exception:
            pass
        rc = 'TIMEOUT'; out = ''; err = ''
        subprocess.run(['taskkill', '/F', '/T', '/PID', str(te.pid)], capture_output=True, timeout=10)
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
    return (name, False, f"期望exit={expected} 实际={rc} out='{out[:40]}' err='{err[:60]}'")


def main():
    ap = argparse.ArgumentParser(description='甲言行为断言测试')
    ap.add_argument('--fast', action='store_true', help='快速集 30 用例 (~1 分钟)')
    ap.add_argument('--zh-feat', action='store_true', help='核心确定性中文特性 (scripts/ CORE_FEAT 白名单: 所有权/借用/多态/copy 族)')
    ap.add_argument('--zh-feat-all', action='store_true', help='全量中文特性 (scripts/feat_zh_*; 依赖/无主库 skip, 其它诚实 FAIL)')
    ap.add_argument('--parallel', type=int, default=0, help='并行 worker 数 (0=串行)')
    ap.add_argument('--only', help='只跑指定测试 (逗号分隔)')
    args = ap.parse_args()

    # 闸① (2026-09-17 第 84 站 · 战线 1 补闸): 预置头「假实现(全吞实参的函数式宏)」上限锁.
    #   只降不升; 名单级 (改名 也 绕不过). 详见 scripts/check_prelude_stubs.py.
    #   活证据: 度量一旦 无人自动跑, 就会 空转 (原 `_debt_count.py` 输出 0/0 且不报错 就是 前车之鉴).
    try:
        import importlib.util as _ilu
        _sp = os.path.join(root, 'scripts', 'check_prelude_stubs.py')
        _spec = _ilu.spec_from_file_location('_chk_stub', _sp)
        _mod = _ilu.module_from_spec(_spec)
        _spec.loader.exec_module(_mod)
        _hits = _mod.scan(_mod.PRE)
        _base = _mod.load_base()
        if _base is not None:
            _added = sorted(set(_hits) - _base)
            if _added:
                print('[闸① 预置头假实现上限锁] ❌ 新增 %d 个 (不允许): %s' % (len(_added), ' '.join(_added)))
                print('    处置: 删掉新增的假实现, 或 (确有必要时) 在台账记明理由并 --write 更新基线.')
                sys.exit(1)
            _removed = sorted(_base - set(_hits))
            print('[闸① 预置头假实现上限锁] ✅ 无新增 (当前 %d / 基线 %d%s)' % (
                len(_hits), len(_base),
                (', 已减少 %d: %s' % (len(_removed), ' '.join(_removed))) if _removed else ''))
    except SystemExit:
        raise
    except Exception as _e:
        print('[闸①] 检查器 异常 (不计红, 但请查): %s' % _e)

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

    print(f'\n行为断言: 通过 PASS={pass_n} SKIP={len(skip_l)} FAIL={len(fail_l)}')
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

