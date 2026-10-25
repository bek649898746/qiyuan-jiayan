# -*- coding: utf-8 -*-
"""check_compile_speed —— 编译器自编耗时预算，**双口径**（台账 G-D52-PERF · D51/D52）。

背景（2026-10-25 启元审计 D51/D52）：
  本判据长期**只测 `-c`**（阈值 110 s），而自举与 1,661 个锚全走 `-o` ⟹
  `-o` 的词法比 `-c` 慢 95 倍（139.7 s vs 1.5 s）**15 天无人报警**。
  判据必须覆盖关键路径 ⟹ 本条扩成两条口径，各自门槛、各自复算。

口径：
  A `-c`   ：固定源（编译器自身）`-c`，跑 N 次取**最小值**，阈值 `--limit-c`（默认 110 s）。
  B `-o`   ：同一份源 `-o`（含前插运行库 → 词法要比 -c 多扫 ~12,059 词元 + 码生成 + PE 写），
             跑 N 次取最小值，阈值 `--limit-o`（默认 250 s；修好 D51 后应收紧）。
             基线（2026-10-25，不动点 199724E7）= 350.7 s ⟹ 现为**红**，正是本判据的意义。

负控（判据自己必须能红）：
  `--limit-c 1 --limit-o 1`      ⟹ 两条口径都必须 FAIL（逻辑负控）
  `--cc <慢词法父控> --legs o`    ⟹ `-o` 口径必须 FAIL（真实负控：修复前的编译器）

用法: python scripts/check_compile_speed.py [--cc qcc_zh_seed.exe] [--runs 2]
                                            [--limit-c 110] [--limit-o 250] [--legs c,o]
退出码: 0 = 所选口径全部达标；1 = 有口径超预算（回归仍在）；2 = 环境未就绪（**不得当绿**）
"""
import argparse
import os
import subprocess
import sys
import tempfile
import time

sys.stdout.reconfigure(encoding='utf-8', errors='replace')
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, 'srclib_jiayan', 'qcc_zh.jy')


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cc', default=os.path.join(ROOT, 'qcc_zh_seed.exe'))
    ap.add_argument('--runs', type=int, default=2)
    ap.add_argument('--limit-c', type=float, default=110.0)
    ap.add_argument('--limit-o', type=float, default=250.0)
    ap.add_argument('--legs', default='c,o')
    a = ap.parse_args()
    cc = a.cc if os.path.isabs(a.cc) else os.path.join(ROOT, a.cc)
    if not os.path.exists(cc):
        print('[FAIL] 环境未就绪: 找不到编译器 %s（本判据未执行，不判绿）' % cc)
        return 2
    if not os.path.exists(SRC):
        print('[FAIL] 环境未就绪: 找不到源 %s' % SRC)
        return 2
    legs = [x.strip() for x in a.legs.split(',') if x.strip()]
    w = tempfile.mkdtemp(prefix='qcc_speed_')
    bad = []
    for leg in legs:
        if leg == 'c':
            args = [cc, '-c', SRC, '-o', os.path.join(w, 'qcc.o')]
            limit = a.limit_c
        elif leg == 'o':
            args = [cc, SRC, '-o', os.path.join(w, 'qcc.exe')]
            limit = a.limit_o
        else:
            print('[FAIL] 未知口径 %r（只认 c / o）' % leg)
            return 2
        times = []
        fail_rc = None
        for _ in range(max(1, a.runs)):
            t0 = time.time()
            r = subprocess.run(args, capture_output=True, timeout=3600)
            dt = time.time() - t0
            if r.returncode != 0:
                fail_rc = r.returncode
                break
            times.append(dt)
        if fail_rc is not None:
            print('[-%s] 编译失败 rc=%d（本口径 未测出耗时 ⟹ 不判绿）' % (leg, fail_rc))
            bad.append('-%s(rc=%d)' % (leg, fail_rc))
            continue
        best = min(times)
        ok = best <= limit
        print('[-%s] %s（best %.1fs，阈值 %.1fs）%s'
              % (leg, ' / '.join('%.1f' % t for t in times), best, limit, 'PASS' if ok else 'FAIL'))
        if not ok:
            bad.append('-%s(%.1fs>%.1fs)' % (leg, best, limit))
    print('=== 编译器自编耗时预算（口径 %s）: %s ==='
          % (','.join(legs), 'PASS' if not bad else 'FAIL -> ' + ' '.join(bad)))
    return 0 if not bad else 1


if __name__ == '__main__':
    sys.exit(main())
