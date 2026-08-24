# Phase 2 施工日志 — 汇编器提速（飞轮第一齿轮）优化1：跳过 final gen_code

> 日期: 2026-08-24 | 提交: Phase2 | 新不动点: b44e2df8（五代全等, H1==H2）
> 验证: build.ps1 全绿 325/325 + behavior 20/20 + git 全量 612/612 + 冒烟全通

## 症状
自举链 v1-v5 编译 qcc_work.jy（1MB 镜像）需 14.6s，比 gcc 宿主（qcc_x86.exe 3.8s）慢 4 倍。验证循环转速慢（每次改动自举链 ~70s）。

## 根因分析（插桩定位）
- 用 gcc 宿主插桩计时：解析 0.5s + gen_code×3 共 2.3s + write_pe 0.65s
- **gen_code 被调 3 遍**（pass1 估算 + pass2 真汇编 + final 重发）——codegen 是主热点（66%）
- pass1/2/final 的 cp 各差 69/28 字节（基址变化 + gen_final 状态差异）
- var_lookup 34 万次调用但每次 ~1 次 strcmp（backward 立即命中）——**符号表非 O(n²) 瓶颈**
- v1 慢 4 倍 = qcc codegen 生成低效机器码（v1 自身运行慢）——但这是 Phase 5 范畴

## 修复（Phase2 优化1）
**非 -S 模式跳过 final gen_code**（main 12816-12832）：
- 原：pass1 → pass2 → 稳定循环 → final → 稳定循环2（最多 3-5 遍 gen_code）
- 新：pass1 → pass2 → 稳定循环 → **跳过 final**（非 -S 用 pass2 稳定代写 exe）
- **-S 模式保留 final**（gen_final=1 时 asm_emit 才写文本，-S 需要）
- 依据：实验证明跳过 final 产物能自举，自举不动点收敛到同一值；gen_final 仅影响 -S 文本，不影响机器码正确性
- -c 模式（coff/H1==H2 验证）本就单遍，不受影响

## 收益
- gcc 宿主编译：3.8s → 3.16s（-17%）
- **自举链 v1 编译：14.6s → 10.7s（-27%）**
- 验证循环提速 27%，飞轮转快

## 验证
- 自举五代全等 = b44e2df8（1-cycle 闭包）
- H1==H2：qcc_x86(C) vs v1(镜像) 编译 reflog-walk.c 哈希一致
- run_tests 325/325 + behavior 20/20
- git 全量 612/612 + 冒烟（reflog/log/status）全通
- build.ps1 全绿（自举 b44e2df8 1-cycle）

## 后续 Phase 2 候选（待做）
- 符号表哈希（var_lookup/func_find）——插桩显示收益 <1%（每次已 1 strcmp 命中），优先级低
- codegen 内部微优化（生成更少指令）——收益大但属 Phase 5 磨优化
- 单遍汇编（数据引用统一 RIP 相对）——收益 33% 但改动大
- 验证策略分层（大哥定调）：纯性能优化走"自举+H1==H2+325+Git冒烟"轻量闭环，Git 全量编译降频为语义改动/封存节点/定期
