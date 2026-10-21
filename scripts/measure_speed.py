#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""measure_speed —— 甲言编译速度口径（同二进制 / 进程内计时 / ≥3 次取中位）

为什么不用外部轮询：本机实测 PowerShell `Start-Process -Wait` 会把耗时**量化到整秒**
（同一文件报 10.02 / 15.02 / 12.01s），据此判断提速会被量具骗。这里用 `time.perf_counter()`
包裹 `subprocess.run`，并用编译器自带的 `QCC_DBG_TIME` 四阶段打点做分解。

用法:
  python scripts/measure_speed.py --cc qcc_zh_seed.exe --runs 3 \
      D:\\qiyuan_home\\corpus\\lua-5.4.7\\ltests.c ... --self srclib_jiayan/qcc_zh.jy
  （`--self` 用 `-c` 编自身源，作为大样本）

输出（stdout 表格 + 可 `--json` 落盘）:
  文件 | 大小 | 中位总耗时 | 词法+宏展开 | 解析 | 码生成 | 对象写 | 阶段和/总
退出码: 0 = 全部跑通；1 = 有文件编译失败（rc != 0）
"""
import argparse
import json
import os
import statistics
import subprocess
import sys
import tempfile
import time

sys.stdout.reconfigure(encoding='utf-8', errors='replace')
REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def run_once(cc, src, obj, timeout=1800):
    env = dict(os.environ, QCC_DBG_TIME='1')
    t0 = time.perf_counter()
    r = subprocess.run([cc, '-c', src, '-o', obj], capture_output=True, env=env, timeout=timeout)
    dt = time.perf_counter() - t0
    err = (r.stderr or b'').decode('utf-8', 'replace')
    phases = {}
    for line in err.splitlines():
        if line.startswith('[TIME]'):
            # [TIME] 词法+宏展开=123ms 解析=456ms 码生成=789ms 对象写=12ms
            if '词法+宏展开=' in line and '合计=' not in line:
                for seg in line[6:].split():
                    k, _, v = seg.partition('=')
                    if v.endswith('ms'):
                        phases[k] = int(v[:-2])
            elif '合计=' in line:
                for seg in line[6:].split():
                    k, _, v = seg.partition('=')
                    if k in ('合计', '总') and v.endswith('ms'):
                        phases[k] = int(v[:-2])
                    elif k == '词元':
                        phases[k] = int(v)
    return r.returncode, dt, phases, err


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cc', default='qcc_zh_seed.exe')
    ap.add_argument('--runs', type=int, default=3)
    ap.add_argument('--json', default='')
    ap.add_argument('--self', action='append', default=[], help='额外的大样本（相对仓根）')
    ap.add_argument('srcs', nargs='*')
    a = ap.parse_args()

    srcs = list(a.srcs) + [os.path.join(REPO, s) for s in a.self]
    if not srcs:
        print('用法: measure_speed.py --cc <exe> [--runs N] <源文件...> [--self 相对路径]')
        return 2

    tmp = tempfile.mkdtemp(prefix='speed_')
    rows, bad = [], 0
    print('%-28s %9s %10s %10s %10s %10s %10s %8s' %
          ('文件', '字节', '中位总ms', '词法+宏', '解析', '码生成', '对象写', '阶段/总'))
    for src in srcs:
        if not os.path.exists(src):
            print('%-28s  [缺失] %s' % (os.path.basename(src), src))
            bad += 1
            continue
        obj = os.path.join(tmp, os.path.basename(src) + '.o')
        walls, ph = [], {}
        rc_all = 0
        for _ in range(max(1, a.runs)):
            rc, dt, phases, err = run_once(a.cc, src, obj)
            rc_all = rc_all or rc
            walls.append(dt * 1000.0)
            for k, v in phases.items():
                ph.setdefault(k, []).append(v)
        med = statistics.median(walls)
        g = {k: int(statistics.median(v)) for k, v in ph.items()}
        s = g.get('词法+宏展开', 0) + g.get('解析', 0) + g.get('码生成', 0) + g.get('对象写', 0)
        tot = g.get('总', 0) or med
        ratio = (s / tot) if tot else 0
        print('%-28s %9d %10.0f %10d %10d %10d %10d %7.0f%%%s' %
              (os.path.basename(src), os.path.getsize(src), med,
               g.get('词法+宏展开', 0), g.get('解析', 0), g.get('码生成', 0), g.get('对象写', 0),
               ratio * 100, '' if rc_all == 0 else '  rc=%d' % rc_all))
        rows.append(dict(file=os.path.basename(src), path=src, bytes=os.path.getsize(src),
                         median_ms=med, phases_ms=g, phase_sum_ms=s, internal_total_ms=tot,
                         ratio=ratio, rc=rc_all, tokens=g.get('词元', 0),
                         runs=len(walls)))
        if rc_all != 0:
            bad += 1

    if a.json:
        with open(a.json, 'w', encoding='utf-8') as f:
            json.dump(rows, f, ensure_ascii=False, indent=1)
        print('# JSON -> %s' % a.json)
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())
