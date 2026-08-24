# Phase 3 施工日志 — IR 重构上主线（新不动点 650993A2）

> 日期: 2026-08-24 | 里程碑: IR 骨架进编译器 | 不动点: 650993a2
> 验证: 自举五代全等 + H1==H2 + 325/325 + 20/20 + git 612/612 + 冒烟全通 + 直译全等

## 上主线内容
- **IR 骨架**（ir_emit/ir_flush + 18 种指令）进入 srclib/qcc_x86.c
- **迁移的 case**（部分走 IR，其余直译）：
  - 控制流: if/while/ternary/break/continue/goto/label (case 8/9/22/16/21/25/20)
  - 运算: 32位加减乘(无缩放/除) + LL64位PK/MK/DK(无指针缩放) + double算术(PK/MK/DK/DV)
  - 变量: 局部 int 读(LOAD_RBP) + 局部 int 赋值(STORE_RBP) + static 读(LOAD_RIP32/64/LEA_RIP)
- 18 种 IR 指令: MOV_IMM/MOV_IMM_LL/JMP_LBL/LABEL/TEST_JZ/TEST_JNZ/PUSH_RAX/ALU_RBX/LOAD_RBP/
  PUSH_R64_RAX/MOV_RBX_RAX64/ALU64_OP/STORE_RBP/PUSH_XMM0/DBL_BIN/LOAD_RIP32/LOAD_RIP64/LEA_RIP

## 直译全等验证（关键）
- 新旧编译器编译**同一份源码**产物全等:
  - qcc_work.jy → b44e2df8（旧产物不变）
  - git 文件 (diff/sequencer/reflog-walk/revision/dir) → 5 个全等
- **原理**: IR 保序回放 → cp 演化=原路径 → label 补丁/RIP 位移自动一致

## 不动点演进说明
- 编译器源码加 IR 骨架 → 编译器二进制变 → 自举不动点从 b44e2df8 → 650993a2
- **直译全等 ≠ 编译器二进制全等**: IR 化保"codegen 产物"全等，编译器自身含 IR 代码

## 验证
- 自举 v1==v5 = 650993a2（1-cycle）
- H1==H2: qcc_x86(C) vs v1(镜像)
- 325/325 + behavior 20/20
- git 全量 612/612 + 冒烟（reflog/log/status）

## 下一步（阶段 B 继续）
- case 4（函数调用）——最重，参数压栈/对齐/回填/call 分派/builtin
- case 15（成员访问）/ case 10 复杂分支（struct/指针赋值）
- 逐步迁移 + 每步直译全等验证
- 阶段 C: pass 框架 + 定律接口位预埋
