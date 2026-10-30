# -*- coding: utf-8 -*-
"""ORD-56 判据：**`.exe` 模式下 跨行宏（`\\` 续行）会让 顶层解析 中断** —— 用户主路径缺陷。

病灶（2026-10-30 实测，6 行最小复现）：
  ```c
  #define X "a" \\
      "b"
  int main (void) { return 0; }
  ```
  · `qcc x.c -o x.exe` ⟹ **rc=1「未找到 主 入口 — 顶层 解析 可能中断」**
  · `qcc -c x.c -o x.o` ⟹ rc=0（同源、同编译器）
  · 单行宏 `#define X "a"` 两种模式都 rc=0 ⟹ 触发条件 = **`\\` 续行 + `.exe` 模式（前插运行库）**

已定位的证据：
  · `QCC_DBG_PPDUMP`（临时探针）显示两种模式的词法输入里，`#define X "a" \\` 的续行**都没有被合并**
    （`\\` 原样保留 + 下一行独立）；
  · 但只有 `.exe` 模式在**顶层**报「未找到主入口」，`-c` 模式容错吞掉残渣；
  · 宏放在 `main` 之后 ⟹ `.exe` 也绿（顶层解析已完成，中断不影响入口查找）
    ⟹ 证明「残渣存在 + 位置在 main 之前」才致命。

判据（每格真跑，与 gcc 无关，纯甲言自身两模式对照）：
  A 跨行宏 + `.exe`  ⟹ 期望 rc=0（**当前 rc=1 = 缺陷复现**）
  B 跨行宏 + `-c`    ⟹ 期望 rc=0（对照，当前成立）
  C 单行宏 + `.exe`  ⟹ 期望 rc=0（对照，当前成立）
  D 跨行宏在 main 之后 + `.exe` ⟹ 期望 rc=0（对照，当前成立）

用法: python scripts/check_ord56_exe_multiline_macro.py [--cc qcc_zh_v1.exe]
"""
import argparse
import os
import subprocess
import sys

sys.stdout.reconfigure(encoding='utf-8', errors='replace')
REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.environ.get('QCC_TMPDIR') or os.path.join(REPO, 'scratch_ord56')

MULTI = '#define X "a" \\\n    "b"\n'
SINGLE = '#define X "a"\n'
MAIN = 'int main (void) { return 0; }\n'

CASES = [
    ('A_multi_exe', MULTI + MAIN, 'exe', 0),          # 期望 rc=0（**当前 rc=1 = 缺陷复现 ⟹ 本判据红**）
    ('B_multi_c', MULTI + MAIN, 'compile', 0),
    ('C_single_exe', SINGLE + MAIN, 'exe', 0),
    ('D_multi_after_main', MAIN + MULTI, 'exe', 0),
]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cc', default=os.environ.get('QCC_TEST_BIN') or 'qcc_zh_v1.exe')
    a = ap.parse_args()
    cc = a.cc if os.path.isabs(a.cc) else os.path.join(REPO, a.cc)
    if not os.path.exists(cc):
        print('[SKIP] 编译器不存在: %s' % cc)
        return 0
    os.makedirs(OUT, exist_ok=True)
    bad = 0
    for name, src, mode, want in CASES:
        c = os.path.join(OUT, name + '.c')
        with open(c, 'w', encoding='utf-8', newline='\n') as f:
            f.write(src)
        if mode == 'exe':
            cmd = [cc, c, '-o', os.path.join(OUT, name + '.exe')]
        else:
            cmd = [cc, '-c', c, '-o', os.path.join(OUT, name + '.o')]
        p = subprocess.run(cmd, cwd=REPO, capture_output=True, text=True,
                           encoding='utf-8', errors='replace')
        ok = (p.returncode == want)
        mark = 'PASS' if ok else 'FAIL'
        if not ok:
            bad += 1
        first = ((p.stderr or '').strip().splitlines() or [''])[0][:80]
        print('[%s] %-20s %-8s rc=%d 期望=%d  %s' % (mark, name, mode, p.returncode, want, first))
    print('==== check_ord56_exe_multiline_macro: %s (%d/%d) ===='
          % ('PASS' if bad == 0 else 'FAIL', len(CASES) - bad, len(CASES)))
    return 0 if bad == 0 else 1


if __name__ == '__main__':
    sys.exit(main())
