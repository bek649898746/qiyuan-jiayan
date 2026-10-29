# -*- coding: utf-8 -*-
"""ORD-45 复现：产物**运行速度**比 gcc -O0 慢 ~2.1–2.5x（三处纯浪费：比较/二元/mov 中转）。
blocked（新递施工单，三刀均可在指令链表层做，不需 CFG/SSA）。

判据：同一 bench.c，甲言编 A、gcc -O0 编 B，各跑 3 次取最小，ratio = A/B。
  ratio ≥ 1.6 ⟹ 复现（rc=1，blocked 诚实）；ratio < 1.6 ⟹ 应转 fixed。
用法: python scripts/check_ord45_runtime_speed.py [--cc qcc_zh_v1.exe]
"""
import argparse, io, os, subprocess, sys, time
sys.stdout.reconfigure(encoding='utf-8', errors='replace')

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TMP = os.environ.get('QCC_TMPDIR') or os.path.join(REPO, 'scratch_ord45')

BENCH = '''#include <stdio.h>
int main(void){
    volatile long acc = 0;
    for (long r = 0; r < 20; r++) {
        for (long i = 0; i < 1000000; i++) acc += (i & 7);
    }
    printf("%ld\\n", acc);
    return 0;
}
'''


def best(cmd, runs=3):
    t = []
    for _ in range(runs):
        t0 = time.perf_counter()
        p = subprocess.run(cmd, capture_output=True, text=True)
        t.append(time.perf_counter() - t0)
        if p.returncode != 0:
            return None, (p.stderr or '')[:120]
    return min(t), ''


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cc', default=os.environ.get('QCC_TEST_BIN') or 'qcc_zh_v1.exe')
    a = ap.parse_args()
    cc = a.cc if os.path.isabs(a.cc) else os.path.join(REPO, a.cc)
    os.makedirs(TMP, exist_ok=True)
    src = os.path.join(TMP, 'bench.c')
    io.open(src, 'w', encoding='utf-8').write(BENCH)
    jy = os.path.join(TMP, 'bench_jy.exe')
    gc = os.path.join(TMP, 'bench_gcc.exe')
    r = subprocess.run([cc, src, '-o', jy], cwd=REPO, capture_output=True, text=True,
                       encoding='utf-8', errors='replace')
    if r.returncode != 0:
        print('[FAIL] 甲言编译 bench 失败: %s' % ((r.stderr or '').strip().splitlines() or [''])[0][:100])
        return 2
    r = subprocess.run(['gcc', '-O0', '-w', src, '-o', gc], capture_output=True, text=True,
                       encoding='utf-8', errors='replace')
    if r.returncode != 0:
        print('[SKIP] gcc 不可用: %s' % (r.stderr or '')[:100])
        return 0
    tj, ej = best([jy])
    tg, eg = best([gc])
    if tj is None or tg is None:
        print('[FAIL] 运行失败 jy=%s gcc=%s' % (ej, eg))
        return 2
    ratio = tj / tg if tg else 0
    print('[BENCH] 甲言 %.4fs · gcc -O0 %.4fs · ratio %.2fx' % (tj, tg, ratio))
    if ratio >= 1.6:
        print('[REPRO] 产物运行速度仍慢 %.2fx（三处纯浪费未修）⟹ blocked 诚实' % ratio)
        return 1
    print('[PASS] ratio %.2fx < 1.6 ⟹ 应转 fixed' % ratio)
    return 0


if __name__ == '__main__':
    sys.exit(main())
