# -*- coding: utf-8 -*-
"""check_gd52_margin —— G-D52-PERF 的「保留监视、判据不动」blocked 闸。

背景（施工单《G-D52_PERF 改 blocked》+ 2026-10-26 05:35 大哥裁定）：
  `check_compile_speed.py` 一行不改（--cc 仍 seed，阈值仍 110s/250s）；
  但 `-c` 实测 107.6s / 110s，余量仅 2.2%，不足以支撑「关闭并停止监视」⟹
  台账状态应为 **blocked**，只保留监视。

本脚本是「监视」的机械化：
  ① 先断言 `scripts/check_compile_speed.py` 的 git diff 为空（判据真没动）；
  ② 跑该判据，解析 `-c` best 与阈值；
  ③ 余量 < MONITOR_MARGIN_PCT（默认 3.0%）⟹ 返回 1（blocked 现场）；
     余量足够 ⟹ 返回 0（可提请改 settled/fixed，由大哥签字）。

用法: python scripts/check_gd52_margin.py
退出: 0 = 余量足够；1 = 仍 blocked（或判据被改动）
"""
import os
import re
import subprocess
import sys

sys.stdout.reconfigure(encoding='utf-8', errors='replace')
REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MONITOR_MARGIN_PCT = 3.0
LIMIT_C = 110.0


def main():
    # ① 判据文件不许动（大哥原话「判据不动」）
    r = subprocess.run(['git', 'diff', '--quiet', '--', 'scripts/check_compile_speed.py'], cwd=REPO)
    if r.returncode != 0:
        print('[FAIL] check_compile_speed.py 有未提交改动 —— 违反「判据不动」')
        return 1

    # ② 跑现有判据（不重实现、不改阈值）
    p = subprocess.run(['python', '-X', 'utf8', 'scripts/check_compile_speed.py'],
                       cwd=REPO, capture_output=True, text=True, encoding='utf-8', errors='replace')
    out = ((p.stdout or '') + (p.stderr or '')).strip()
    print(out)
    m = re.search(r'\[-c\]\s+([\d.]+)\s*/\s*([\d.]+)', out)
    if not m:
        print('[FAIL] 解析不到 [-c] 实测值')
        return 1
    best = float(m.group(1))
    margin = (LIMIT_C - best) / LIMIT_C * 100.0
    print('G-D52-PERF: -c best=%.1fs / 阈值 %.1fs ⟹ 余量 %.1f%%（监视阈值 %.1f%%）' % (
        best, LIMIT_C, margin, MONITOR_MARGIN_PCT))
    if p.returncode != 0:
        print('[BLOCKED] check_compile_speed 本身已红（先查量具再判退化）')
        return 1
    if margin < MONITOR_MARGIN_PCT:
        print('[BLOCKED] G-D52-PERF: 余量 %.1f%% < %.1f%% —— 保留监视，判据不动' % (margin, MONITOR_MARGIN_PCT))
        return 1
    print('[PASS] G-D52-PERF: 余量 %.1f%% >= %.1f%%（可提请改 settled/fixed，需大哥签字）' % (
        margin, MONITOR_MARGIN_PCT))
    return 0


if __name__ == '__main__':
    sys.exit(main())
