#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""check_speed_parse —— 解析阶段速度判据（真实语料 + 进程内计时）

背景（2026-10-21）: `类型含析构字段深()` 对**每个变量**都递归重扫整张类型图（无记忆）
⟹ lua `lparser.c` 解析 **18.0s**、`lcode.c` **13.4s**（gdb/addr2line 采样 97% 落在本函数），
而同样 5 万词元的 `lvm.c` 只要 1.5s。修复 = 深 0 处按 `类型代` 版本号记忆化。

判据（驱动已发布的编译器 · 真实语料 · 读 `QCC_DBG_TIME` 的 `解析=` 毫秒）:
  · `lparser.c` 解析 ≤ 4000ms（修前 18053ms，7.4×+）
  · `lcode.c`   解析 ≤ 2500ms（修前 13374ms，11×+）
  · 两文件合计 `-c` 墙钟 ≤ 8s（修前 ~33s）
语料缺失（`D:\\qiyuan_home\\corpus\\lua-5.4.7` 不存在）⟹ **[SKIP] 环境未就绪** + 退出码 0（可区分）。

用法: python scripts/check_speed_parse.py [--cc qcc_zh_seed.exe] [--runs 3]
退出码: 0 = PASS 或 SKIP；1 = 超预算（提速被回退）
"""
import argparse
import os
import statistics
import subprocess
import sys
import tempfile
import time

sys.stdout.reconfigure(encoding='utf-8', errors='replace')
REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CORPUS = r'D:\qiyuan_home\corpus\lua-5.4.7'
BUDGET = {'lparser.c': 4000, 'lcode.c': 2500}   # 解析 ms 上限


def measure(cc, src, runs):
    env = dict(os.environ, QCC_DBG_TIME='1')
    wall, parse = [], []
    tmpdir = tempfile.mkdtemp(prefix='speed_')
    obj = os.path.join(tmpdir, os.path.basename(src) + '.o')
    for _ in range(runs):
        t = time.perf_counter()
        r = subprocess.run([cc, '-c', src, '-o', obj], capture_output=True, env=env, timeout=600)
        wall.append((time.perf_counter() - t) * 1000)
        if r.returncode != 0:
            return None, None, r.returncode
        ms = None
        for line in (r.stderr or b'').decode('utf-8', 'replace').splitlines():
            if line.startswith('[TIME]') and '解析=' in line:
                for seg in line[6:].split():
                    k, _, v = seg.partition('=')
                    if k == '解析' and v.endswith('ms'):
                        ms = int(v[:-2])
        if ms is None:
            return statistics.median(wall), None, 0
        parse.append(ms)
    return statistics.median(wall), statistics.median(parse), 0


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cc', default='qcc_zh_seed.exe')
    ap.add_argument('--runs', type=int, default=3)
    a = ap.parse_args()
    if not os.path.isdir(CORPUS):
        print('[FAIL] 环境未就绪: 语料目录 %s 不存在 —— 本判据需真实语料（本地开发机 / 审计机）' % CORPUS)
        return 2
    bad = 0
    total_wall = 0.0
    print('%-12s %10s %10s %8s' % ('文件', '解析中位ms', '墙钟中位ms', '预算ms'))
    for f, budget in BUDGET.items():
        src = os.path.join(CORPUS, f)
        if not os.path.exists(src):
            # 2026-10-24: 缺输入文件 = 判据不完整，计失败（原 continue 会让必要项缺失而仍判 PASS）
            print('%-12s %s' % (f, '[FAIL] 缺文件'))
            bad += 1
            continue
        wall, parse, rc = measure(a.cc, src, a.runs)
        total_wall += wall or 0
        if rc != 0:
            print('%-12s [FAIL] 编译失败 rc=%d' % (f, rc))
            bad += 1
            continue
        if parse is None:
            print('%-12s [FAIL] 未取到 [TIME] 解析= （编译器未含计时打点？）' % f)
            bad += 1
            continue
        ok = parse <= budget
        print('%-12s %10d %10.0f %8d  %s' % (f, parse, wall, budget, 'PASS' if ok else 'FAIL'))
        if not ok:
            bad += 1
    ok_all = (bad == 0) and (total_wall <= 8000)
    print('两文件 墙钟合计 %.1fs（预算 8.0s）' % (total_wall / 1000.0))
    print('=== 解析速度 判据: %s ===' % ('PASS' if ok_all else 'FAIL'))
    return 0 if ok_all else 1


if __name__ == '__main__':
    sys.exit(main())
