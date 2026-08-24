# Phase 3 施工日志 — IR 重构阶段 B 进度（控制流 + 运算迁移）

> 日期: 2026-08-24 | 阶段: B（进行中）| 不动点: b44e2df8（直译全等持续保持）
> 验证: _ir_parity.py 每步 PASS

## 已迁移到 IR（12 种 IR 指令）

| IR 指令 | 落码行为 | 覆盖 case |
|---|---|---|
| I_MOV_IMM / I_MOV_IMM_LL | mov eax,imm32 / mov rax,imm64 | case 0 字面量 |
| I_JMP_LBL / I_LABEL | jmp label / set_label | case 16/20/21 goto/label |
| I_TEST_JZ / I_TEST_JNZ | test eax; jz/jnz label | case 8/9/22 if/while/ternary |
| I_PUSH_RAX / I_ALU_RBX | push rax; pop rbx+alu_rr | case 2 32位运算（无缩放/除）|
| I_LOAD_RBP | mov eax,[rbp+disp](+符号扩展) | case 1 局部 int 读 |
| I_PUSH_R64_RAX / I_MOV_RBX_RAX64 / I_ALU64_OP | push_r64; mov rbx,rax; pop+add/sub/imul64 | case 2 LL 64位 PK/MK/DK（无指针缩放）|

## 关键保序结论
- IR 记录顺序 = 原 cg() 发射顺序（逐 case 等价替换）
- ir_flush 回放从头开始，cp 演化 = 原路径 → label 补丁/RIP 位移自动一致
- **直译 SHA 全等持续保持**（B44E2DF8），证明框架在控制流+运算场景完全可靠

## 教训（脚本替换）
- 替换范围 end 计算错（nll 与 break 不同行）→ 空替换删分支体 → FAIL
- 职责分离：定义脚本（g3 只加指令）+ 分支脚本（g2 只改分支）避免重复插入
- 锚点用行号法（注释特殊字符致文本锚点失败）

## 下一步
- case 10（赋值）简单路径（var=expr 标量）
- case 4（函数调用）——最重，单独一轮
- case 1 其余分支（static/数组/指针/LL/double 读）
- 迁移覆盖 git 主要路径后统一上主线（同步镜像 + 自举 + 325 回归）
