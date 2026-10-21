#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""判据③ 自证测量器 —— 34k 自身源 / lcode.c 的 `-c` 端到端耗时（同二进制 · 进程内计时 · ≥3 次中位）。

为什么另起一个脚本（而不是复用 `measure_speed.py`）：
  上一轮被打回的原因之一是「低负载 173–177s」只写在日志正文里、**没有留下采样本体** ⟹ 无法自证。
  本脚本把三类证据写进**同一份 stdout**，任何人重跑都能得到同一形状的输出：
    ① 每次运行的**毫秒数**（逐次列出，不是只给中位）
    ② 每组的**机器负载**（`GetSystemTimes` 自采，不依赖外部命令）
    ③ **判定行**（绝对线 185000ms / 7400ms，PASS/FAIL 与退出码一致）
  ⟹ 日志本身就是证据，不需要人再补一句「当时负载很低」。

用法：
  python scripts/quiet34k.py                       # 默认 qcc_zh_v1.exe，3 次，测 34k + lcode
  python scripts/quiet34k.py --runs 5 --cc qcc_zh_seed.exe
  python scripts/quiet34k.py --allow-busy          # 负载 >25% 也照样跑（默认拒绝，避免量具失真）

退出码：0 = 两条绝对线都达标；1 = 有未达标；2 = 机器太忙/缺件（不计入判据）
"""
import argparse
import ctypes
import json
import os
import statistics
import subprocess
import sys
import time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SELF_SRC = os.path.join(ROOT, 'srclib_jiayan', 'qcc_zh.jy')
LCODE_SRC = r'D:\qiyuan_home\corpus\lua-5.4.7\lcode.c'
CRIT_34K_MS = 185000
CRIT_LCODE_MS = 7400


class FILETIME(ctypes.Structure):
    _fields_ = [('dwLowDateTime', ctypes.c_uint32), ('dwHighDateTime', ctypes.c_uint32)]


def _ft(v):
    return (v.dwHighDateTime << 32) | v.dwLowDateTime


def cpu_load(sample_sec=0.5):
    """返回 (busy%, 逻辑核数)。busy = 1 - Δidle/Δ(user+kernel)；kernel 含 idle。"""
    k32 = ctypes.windll.kernel32
    idle, kern, user = FILETIME(), FILETIME(), FILETIME()
    k32.GetSystemTimes(ctypes.byref(idle), ctypes.byref(kern), ctypes.byref(user))
    i0, k0, u0 = _ft(idle), _ft(kern), _ft(user)
    time.sleep(sample_sec)
    k32.GetSystemTimes(ctypes.byref(idle), ctypes.byref(kern), ctypes.byref(user))
    i1, k1, u1 = _ft(idle), _ft(kern), _ft(user)
    d_idle = i1 - i0
    d_total = (k1 - k0) + (u1 - u0)
    if d_total <= 0:
        return 0.0, os.cpu_count() or 1
    return (1.0 - d_idle / d_total) * 100.0, os.cpu_count() or 1


def one_run(cc, src, out, env):
    t0 = time.perf_counter()
    p = subprocess.run([cc, '-c', src, '-o', out], cwd=ROOT, capture_output=True, env=env, timeout=1800)
    ms = (time.perf_counter() - t0) * 1000.0
    if p.returncode != 0:
        msg = (p.stdout + p.stderr).decode('utf-8', 'replace').strip()[:200]
        raise RuntimeError('编译失败 rc=%d: %s' % (p.returncode, msg))
    return ms


def group(tag, cc, src, runs, out_dir, env):
    loads, times = [], []
    for i in range(runs):
        load, ncpu = cpu_load()
        loads.append(round(load, 1))
        ms = one_run(cc, src, os.path.join(out_dir, '%s_%d.o' % (tag.replace('/', '_'), i)), env)
        times.append(round(ms, 1))
        print('[RUN] %-6s #%d  %10.1f ms   load=%.1f%%  (%d 逻辑核)' % (tag, i + 1, ms, load, ncpu), flush=True)
    med = statistics.median(times)
    print('[MED] %-6s n=%d  runs=%s  median=%.1f ms  load=%s' % (tag, runs, times, med, loads), flush=True)
    return {'tag': tag, 'cc': cc, 'src': src, 'runs': times, 'median_ms': med, 'load_pct': loads}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cc', default='qcc_zh_v1.exe')
    ap.add_argument('--before', default=None, help='可选的对照二进制（A/B 同口径）')
    ap.add_argument('--runs', type=int, default=3)
    ap.add_argument('--allow-busy', action='store_true')
    ap.add_argument('--json', default=None)
    args = ap.parse_args()

    cc = os.path.join(ROOT, args.cc) if not os.path.isabs(args.cc) else args.cc
    if not os.path.exists(cc):
        print('[SKIP] 环境未就绪: 找不到编译器 %s' % cc)
        return 2
    if not os.path.exists(SELF_SRC):
        print('[SKIP] 环境未就绪: 找不到 34k 自身源 %s' % SELF_SRC)
        return 2

    load0, ncpu = cpu_load(1.0)
    print('[ENV] 机器负载 %.1f%% / %d 逻辑核   cc=%s' % (load0, ncpu, os.path.basename(cc)), flush=True)
    if load0 > 25.0 and not args.allow_busy:
        print('[SKIP] 机器太忙（%.1f%% > 25%%）⟹ 读数不可作为判据；清场后重跑，或 --allow-busy 仅取方向'
              % load0)
        return 2

    out_dir = os.path.join(ROOT, 'scratch_test')
    os.makedirs(out_dir, exist_ok=True)
    env = dict(os.environ, QCC_SMALL_HEAP='1')
    rows = []

    cc_list = [('after', cc)]
    if args.before:
        b = args.before if os.path.isabs(args.before) else os.path.join(ROOT, args.before)
        if not os.path.exists(b):
            print('[SKIP] --before 不存在: %s' % b)
            return 2
        cc_list.insert(0, ('before', b))

    for name, exe in cc_list:
        if os.path.exists(LCODE_SRC):
            rows.append(dict(group('%s/lcode' % name, exe, LCODE_SRC, args.runs, out_dir, env), who=name))
        else:
            print('[SKIP] 缺语料 %s（不影响 34k 判据）' % LCODE_SRC)
        rows.append(dict(group('%s/34k' % name, exe, SELF_SRC, args.runs, out_dir, env), who=name))

    load1, _ = cpu_load(1.0)
    print('[ENV] 收尾负载 %.1f%%' % load1, flush=True)

    after = {r['tag'].split('/')[1]: r for r in rows if r['who'] == 'after'}
    ok = True
    if 'lcode' in after:
        m = after['lcode']['median_ms']
        verdict = 'PASS' if m <= CRIT_LCODE_MS else 'FAIL'
        ok = ok and m <= CRIT_LCODE_MS
        print('[JUDGE] lcode 中位 %.1f ms  <= %d ms : %s' % (m, CRIT_LCODE_MS, verdict))
    if '34k' in after:
        m = after['34k']['median_ms']
        verdict = 'PASS' if m <= CRIT_34K_MS else 'FAIL'
        ok = ok and m <= CRIT_34K_MS
        print('[JUDGE] 34k   中位 %.1f ms  <= %d ms : %s' % (m, CRIT_34K_MS, verdict))
    if args.before:
        before = {r['tag'].split('/')[1]: r for r in rows if r['who'] == 'before'}
        for k in ('lcode', '34k'):
            if k in before and k in after:
                print('[AB] %-5s before=%.1f ms  after=%.1f ms  比值=%.2f×'
                      % (k, before[k]['median_ms'], after[k]['median_ms'],
                         before[k]['median_ms'] / after[k]['median_ms'] if after[k]['median_ms'] else 0))
    if args.json:
        with open(args.json, 'w', encoding='utf-8', newline='\n') as f:
            json.dump({'rows': rows, 'load_before': load0, 'load_after': load1,
                       'crit_34k_ms': CRIT_34K_MS, 'crit_lcode_ms': CRIT_LCODE_MS}, f, ensure_ascii=False, indent=1)
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())
