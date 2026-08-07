# 2026-08-07 甲言技术债攻坚 — P1 系列修复

## 背景
大哥指出 4 大痛点（CI 红/PR 绕过/镜像分歧/编码盲区），并要我审计甲言技术债。审计后按优先级攻克 P1 镜像/源分歧。

## 修复清单（主战场 qiyuan-jiayan，main 分支）

### P1-1: 镜像 func_tbl 512→1024 对齐 C 源 ✅ (commit 7fbc0c6)
- C 源 08-06 已扩 1024，镜像漏 → v1/v2 编 >512 函数程序静默丢函数
- 同步: func_tbl[1024], fn_ret_si_map[1024]（C 源原只有 func_tbl 扩了，fn_ret_si_map 仍 512 → 潜在越界，一并修）+ 守卫 fi<512→1024
- 新不动点 0A3D2F0A

### P1-3: 镜像 case-4 callee 单次扫描 ✅ (commit 6f22a5e)
- 镜像旧两段扫描（先找 nt==1 → 抓尾参当 callee）→ 单次取最后 child
- 新增回归 regress_fnptr_var_args.c（变量实参触发旧 bug）
- 新不动点 5349FD22

### P1-4: 镜像 case-14 静态 struct 数组 ≤8B 解引用 ✅ (commit 931e0ec)
- 镜像 is_struct_elem 空分支 → 补 C 版解引用逻辑
- 新增回归 regress_static_struct_arr_value.c
- 附带: C 源 L4414 全局 struct 数组 brace 初始化防崩（+gcnt==0 条件，崩溃→显式报错）
- 新不动点 207CC1C7

### P1-5: 镜像 -c 三处对齐 C 源 ✅ (commit 3824d92)
- is_user: coff_mode && !coff_is_builtin → extern 调用按 user call
- ginit: coff 模式挂最后一个函数（last_fn_i）
- write_coff_obj: 常量初值写 .data（原 calloc 全零）
- 清理 L8068 3 份重复 coff_mode if
- 新不动点 C2E7F197

### P1-6: C 源 -double 守卫 512→1024 ✅ (commit 4db809e)
- dbl_n∈[512,1024) 原静默不写却 dbl_n++ → 垃圾 double
- 对齐镜像 + 溢出报错
- 不动点不变（C 源改动不影响自举产物）

### P1-7/8: 测试补盲区 ✅ (commit 06433ff)
- 新增 regress_hexfp.c / regress_fn_macro_nest.c / regress_volatile.c
- b_hex/b_switch 注释标明 %08x/%#x、fall-through 为已知未实现
- 186/186 行为断言

### 镜像补 coff_is_builtin 定义 ✅ (commit 02e4b45)
- 根因: 镜像引用但无定义 → jy 未定义函数返回 0 → -c 内置函数全导出 scl=2
- 用 if 链实现（避免静态数组初始化）
- 效果: v1 -c 符号正确（counter 入 .data，内置函数不再导出），不再崩溃
- 新不动点 23E81084

## 已知遗留

### P1-5b: v1 自宿主 -c + 有初始化全局变量 DCL-MISS（未修）
- 现象: `int counter = 100;` v1 -c 报 `[DCL-MISS] unregistered decl: counter`，无 .o；host 正常（278B .o + counter 符号）
- 已排查: 两版 lexer `=`→AK 一致、parse 注册一致、var_lookup/var_isstatic/var_codegen_visible 一致、case-7 ginit 检查一致
- 定位线索: 镜像 var_static pdisp=0（C 源 -1）；host 全局变量初始化不设 pdisp（走 var_ginit=-1 但 case-7 仍能找到）；v1 却找不到 → 疑镜像全局变量注册在 -c 模式下 vars 表被 coff 相关重置
- 影响面: 仅自宿主 -c + 全局变量初始化（宿主正常，CI 用 host 不受影响）
- 建议: 在 v1 镜像 case-7 DCL-MISS 处插桩打印 vars 表，对比 -c 与普通模式

### P2-5: 镜像 3 份 write_coff_obj 死代码（未修）
- L7513/L7636/L7759 三份定义（第 1、2 份相同 5013 字符，第 3 份不同 8339）
- 需"埋标记编译"确认生效份再删

### new-brace: 全局 struct 数组 brace 初始化（未完整修）
- static struct Pair pairs[2]={{...}} 已从崩溃改为显式报错（AST overflow）
- brace_arr_init 需支持 struct 元素（leaf 分支按元素类型分流到 brace_fields）

## 教训
- 镜像/源分歧审计法: 统计两边 codegen 模式出现次数差 → 逐处对照（var_isstatic 等）
- jy 未定义函数不报错（返回 0）→ 静默失效，是镜像 bug 的隐蔽根源
- 甲言不支持函数内静态数组初始化（静 常 字 *bn[] 崩溃）→ 用 if 链
- 每次镜像改动必须 C 源↔镜像同步 + 重打不动点 + run_tests.py 双端验证

## 验证命令
- python scripts/run_tests.py（186/186）
- powershell -File scripts/build.ps1（不动点 23e81084）
- python scripts/test_multifile.py（6/6）
- python scripts/fuzz_qcc.py 50 828（0 差异）
