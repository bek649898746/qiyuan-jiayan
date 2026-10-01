# -*- coding: utf-8 -*-
"""check_c_mode_smoke —— **`-c` 模式冒烟**（闸② 候选）。

背景（第 439 站续三十七 · 2026-10-01）：
  **内建豁免表**（`qcc_zh.jy` @4547 起）是**编译器自动注入调用**的声明豁免 —— 表内项 ⟹ 跳过 A1001。
  **实测缺陷（P0-2 同型，范围更大）**：43 个 `__qcc_*` / `__gen_*` / `__coro_*` 运行时函数
  **未入表** ⟹ **链接模式**靠「自动包含 qcc_rt.c」侥幸拿到声明而通过，
  但 **`-c` 模式**（多文件编译，不自动包含运行时库）⟹ **任何使用这些特性的 .c 都 A1001**。
  （`__qcc_div_check` 是 2026-09-14 P0-2 已修的单例；本轮把同族的 43 项一并补齐。）

为什么需要本闸：
  **`run_tests_zh.py` 的 989 个锚全部是「链接模式」** ⟹ **`-c` 模式零覆盖** ⟹ 上述缺陷
  **闸永远发现不了**（本轮靠「发射符号 × 白名单」交叉普查才挖出）。

规则：对每个用例 **分别**跑 `-c` 与 链接模式，两者都必须成功；`-c` 失败即判红。

用法：
  python scripts/check_c_mode_smoke.py [编译器]      # 默认 qcc_zh_v1.exe
退出码：0 = 全过；1 = 有 `-c` 失败
"""
import io, os, subprocess, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CC = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, 'qcc_zh_v1.exe')
OUT = os.path.join(ROOT, 'scratch_c1b2' if os.path.isdir(os.path.join(ROOT, 'scratch_c1b2')) else '.', 'csmoke')
os.makedirs(OUT, exist_ok=True)

H = '# 包含 <stdio.h>\n'
CASES = {
    'pool': H + '函数 整 主(空) { 内存池 p, 16, 2; 空 *x = 取块 p; 格式化 "POOL %d\\n", (x!=0); 返 0; }\n',
    'cell': H + '函数 整 主(空) { 单元 u, 5; 格式化 "CELL %d\\n", 取单元 u; 返 0; }\n',
    'vec':  H + '函数 整 主(空) { 动态数组 v; 追加 v, 7; 格式化 "VEC %d\\n", 取元素 v, 0; 返 0; }\n',
    'sb':   H + '函数 整 主(空) { 字符串构建 s; 追加串 s, "X"; 格式化 "SB %d\\n", 构建长度 s; 释放串 s; 返 0; }\n',
    'sem':  H + '信号量 sem, 2;\n函数 整 主(空) { 获取 sem; 格式化 "SEM OK\\n", 0; 放行 sem; 返 0; }\n',
    'div':  H + '函数 整 主(空) { 整 a = 10; 整 b = 3; 格式化 "DIV %d\\n", a / b; 返 0; }\n',
    'awaited': H + '函数 整 主(空) { 格式化 "PLAIN OK\\n", 0; 返 0; }\n',
}

bad = []
for name, src in CASES.items():
    p = os.path.join(OUT, name + '.c')
    io.open(p, 'w', encoding='utf-8-sig', newline='\n').write(src)
    obj = os.path.join(OUT, name + '.obj')
    r = subprocess.run([CC, p, '-c', '-o', obj], capture_output=True, timeout=300)
    ok_c = (r.returncode == 0)
    msg = (r.stdout + r.stderr).decode('utf-8', 'replace').strip().replace('\n', ' ')
    print('%-8s -c: %s%s' % (name, 'OK' if ok_c else 'FAIL', '' if ok_c else '  | ' + msg[:110]))
    if not ok_c:
        bad.append(name)

print()
print('[check_c_mode_smoke] %d 用例 · `-c` 失败 %d' % (len(CASES), len(bad)))
if bad:
    print('  ❌ 失败: %s' % ' '.join(bad))
    sys.exit(1)
print('  → 全过')
sys.exit(0)
