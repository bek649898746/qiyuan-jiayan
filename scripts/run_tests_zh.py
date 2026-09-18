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
    'scripts/feat_zh_syntax_c.c',
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

