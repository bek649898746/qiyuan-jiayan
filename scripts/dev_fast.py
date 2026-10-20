# -*- coding: utf-8 -*-
"""开发期快速验证（验证分级 · 2026-10-18 大哥指令「加快节奏」）。

目的：把「每刀 18 分钟（v1+v2）」压到「9 分钟（只 v1）+ 秒级并行判据」，
       v2（v1==v2 自举闭环）只在**收尾/结构级改动**时跑一次。

两种模式：
  python scripts/dev_fast.py            # 默认: 用 --cc 旧编译器 编新源码 → 并行跑 4 个语义判据
  python scripts/dev_fast.py --gates    # 不编译: 只并行跑 IR 15 + 4 selftest + opt 4

退出码: 0 全过；1 有失败。
用法示例:
  python scripts/dev_fast.py --cc build\\qcc_zh_prev.exe
  python scripts/dev_fast.py --gates
"""
import argparse
import concurrent.futures as cf
import glob
import os
import subprocess
import sys

sys.stdout.reconfigure(encoding='utf-8', errors='replace')
REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
os.chdir(REPO)


def run(name, args, timeout=1800):
    r = subprocess.run(args, capture_output=True, text=True, encoding='utf-8',
                       errors='replace', timeout=timeout, cwd=REPO)
    out = (r.stdout or '') + (r.stderr or '')
    return name, r.returncode, out


def parallel(jobs, workers=4):
    results = []
    with cf.ThreadPoolExecutor(max_workers=workers) as ex:
        futs = [ex.submit(run, n, a) for n, a in jobs]
        for f in cf.as_completed(futs):
            results.append(f.result())
    return results


def main():
    ap = argparse.ArgumentParser()
    # 2026-10-21: 默认 从 `qcc_zh_v2.exe` 改为 **`qcc_zh_seed.exe`** —— build_zh.ps1 每次
    # 收尾都用 `$expected` 哈希**校验**种子，而 `qcc_zh_v2.exe` 没有任何校验、实测已静默落后
    # **6 个提交**（CE46F0D5 vs 种子 ABBF2A4A）⟹ 用旧编译器编新源码，A/B 结论全废
    # （本会话曾据旧 v2 误判「D39 让 -c 崩」，烧掉 ~1 小时）。
    ap.add_argument('--cc', default='qcc_zh_seed.exe', help='用于编译新源码的旧编译器（默认 = 哈希校验过的种子）')
    ap.add_argument('--out', default=os.path.join('build', 'dev_fast.exe'))
    ap.add_argument('--gates', action='store_true', help='跳过编译，只跑并行判据')
    ap.add_argument('--only', default='compiler', choices=['compiler', 'ir', 'opt', 'all'],
                    help='按改动面分级：compiler=编 v1+语义判据（默认）；ir=IR 15+4 selftest；opt=opt 4；all=全部')
    ap.add_argument('--workers', type=int, default=4)
    a = ap.parse_args()

    if not a.gates:
        print('== [1/2] 旧编译器编新源码（只 v1，不跑 v2）==')
        r = subprocess.run([a.cc, os.path.join('srclib_jiayan', 'qcc_zh.jy'), '-o', a.out],
                           capture_output=True, text=True, encoding='utf-8', errors='replace')
        print(((r.stdout or '') + (r.stderr or '')).strip()[-400:])
        if r.returncode != 0:
            print('=== dev_fast: FAIL（编译失败，无需跑判据）===')
            return 1
        cc = a.out
    else:
        cc = a.cc

    # 语义判据（验证「编译器本身」：语法/类型/诊断/内联/只读/错误链）
    sem = [
        ('guard', ['python', 'scripts/check_inline280_guard.py', '--cc', cc]),
        ('inline280', ['python', 'scripts/check_inline280.py', '--cc', cc]),
        ('inline280_en', ['python', 'scripts/check_inline280_english.py', '--cc', cc]),
        ('defect', ['python', 'scripts/check_defect_probes.py', '--cc', cc]),
        ('a9006', ['python', 'scripts/check_a9006_cast_const.py', '--cc', cc]),
        ('errchain84', ['python', 'scripts/check_errchain84.py', '--cc', cc]),
        ('overload204', ['python', 'scripts/check_overload204.py', '--cc', cc]),
        ('simd63', ['python', 'scripts/check_simd63.py', '--cc', cc]),
        ('variant120', ['python', 'scripts/check_variant120.py', '--cc', cc]),
        ('dataflow220', ['python', 'scripts/check_dataflow220.py', '--cc', cc]),
        ('exception148', ['python', 'scripts/check_exception148.py', '--cc', cc]),
    ]
    ir = [(os.path.basename(f)[:-3], ['python', f]) for f in sorted(glob.glob('scripts/check_ir_*.py'))]
    ir += [(n + ' --selftest', ['python', 'scripts/%s.py' % n, '--selftest'])
           for n in ('check_ir_emit_cover', 'check_ir_real_edges', 'check_ir_recon', 'check_ir_value')]
    opt = [(n, ['python', 'scripts/%s.py' % n])
           for n in ('check_opt_zh', 'check_opt_per_pass', 'check_opt_census', 'check_opt_gain')]

    jobs = []
    if a.only in ('compiler', 'all'):
        jobs += sem  # --gates 时也跑（cc 已是 a.cc）；原 `and not a.gates` 导致 0 项空跑 PASS
    if a.only in ('ir', 'all'):
        jobs += ir
    if a.only in ('opt', 'all'):
        jobs += opt

    if not jobs:
        print('=== dev_fast: FAIL（0 项判据 —— 模式/参数不对，拒绝空跑 PASS）===')
        return 1
    print('== [2/2] 并行判据 (%d 项, %d 路) ==' % (len(jobs), a.workers))
    results = parallel(jobs, a.workers)
    fails = []
    for name, rc, out in sorted(results):
        tail = out.strip().splitlines()[-1][:110] if out.strip() else ''
        print('%-34s rc=%d  %s' % (name, rc, tail))
        if rc != 0:
            fails.append(name)
    if fails:
        print('=== dev_fast: FAIL (%s) ===' % ','.join(fails))
        return 1
    print('=== dev_fast: PASS（语义 + IR + opt 全绿；收尾再跑 build_zh.ps1 验 v1==v2）===')
    return 0


if __name__ == '__main__':
    sys.exit(main())
