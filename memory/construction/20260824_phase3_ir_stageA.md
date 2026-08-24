# Phase 3 施工日志 — IR 重构阶段 A（骨架 + 直译验证框架）

> 日期: 2026-08-24 | 阶段: A | 不动点: b44e2df8（直译全等保持）
> 验证: _ir_parity.py PASS（IR 产物 == 基准不动点）

## 背景
1.0 已封存（t35v reflog 全通）。进入**重构版本** = Phase 3 IR 重构（立身体）。
目标：AST → 三地址码 IR → IR→机器码直译，**直译产物与重构前逐字节 SHA 全等**（硬门槛）。

## 阶段 A 目标（骨架 + 框架验证）
- IR 表结构 + 记录/回放接口（符合 qcc C 子集：定长数组）
- 直译验证脚本 _ir_parity.py
- 最小 IR 子集接入 cg() 2-3 个 case
- 直译全等验证

## 实现
### IR 骨架（srclib 副本实验）
```c
#define IR_MAX 2000000
static int ir_op[IR_MAX], ir_a[IR_MAX], ir_b[IR_MAX], ir_c[IR_MAX];
static int ir_n;
// I_MOV_IMM(reg,imm) / I_MOV_IMM_LL(reg,lo,hi) / I_JMP_LBL(label) / I_LABEL(label)
static void ir_flush(void) { 逐条回放: mov_r_imm / mov_rax_imm64 / jmp_rel+patch_label / set_label }
```

### 迁移 case
- case 0（整数字面量）：nll ? I_MOV_IMM_LL : I_MOV_IMM
- case 20（SET_LABEL）：I_LABEL
- case 16/21（break/continue）：I_JMP_LBL（锚点未匹配，下一轮补）

## 关键结论
**直译全等保序原理**：
- IR 记录顺序 = 原 cg() 发射顺序（逐 case 等价替换）
- ir_flush 回放从头开始，cp 演化 = 原路径（IR 表不占字节）
- 所有依赖 cp 的值（label 补丁 patch_label(cp-4,...)、RIP 位移 stc_disp）自动一致
- **逐 case 迁 IR + SHA 全等 框架可行**（B44E2DF8 保持）

## 验证
- _ir_parity.py: 编译 qcc_work.jy, SHA 对比基准不动点 B44E2DF8 → PASS

## 下一步（阶段 B）
- 逐步迁移 cg() 全部 case（复杂度递增）
- 每步: _ir_parity.py 全等 + 自举 + 325 回归
- 临时值: 虚拟栈槽→物理 rsp 偏移映射
- 函数调用 case 4（最重）单独一轮
- 阶段 C: pass 框架 + 定律接口位预埋（线性/和/格三根）

## 本地提交
- 阶段 A 骨架代码（实验副本）+ 验证脚本
- 不推送远端（重构版本本地管理）
