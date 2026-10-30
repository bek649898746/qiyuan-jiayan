# -*- coding: utf-8 -*-
"""bench_serve —— `--serve` 常驻编译的吞吐判据（同量独立进程 vs 常驻）。

为什么在仓里：`--serve` 的「毫秒级迭代」原是一句**未验证的宣称**（施工单 §一）。
2026-10-28（R891）实测：常驻与独立**几乎相同**（620 ms vs 628 ms/次）——
固定开销不在进程启动（169 ms），而在每次都要重建 prelude + 运行库（≈400 ms）。
本脚本把这条测量固化，避免下次再凭印象判断「serve 很快」。

判据：
  · N 个不同源文件，先 `N × 独立进程` 计时，再 `N × 常驻请求` 计时；
  · 打印两者与比值；`--expect-ratio R` 时比值 < R 则 rc=1（默认 0，只测量不判红）。

用法:
  python scripts/bench_serve.py [--cc qcc_zh_v1.exe] [--n 100] [--expect-ratio 10]
退出码: 0 = 达到期望比值（或未设期望）；1 = 未达到
"""
import argparse
import json
import os
import subprocess
import sys
import tempfile
import time

sys.stdout.reconfigure(encoding='utf-8', errors='replace')
REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def pick_cc(explicit):
    if explicit:
        return explicit
    env = os.environ.get('QCC_TEST_BIN')
    if env:
        return env
    for name in ('qcc_zh_v1.exe', 'qcc_zh_seed.exe'):
        p = os.path.join(REPO, name)
        if os.path.exists(p):
            return p
    return 'qcc_zh_v1.exe'


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cc', default=None)
    ap.add_argument('--n', type=int, default=100)
    ap.add_argument('--expect-ratio', type=float, default=0.0,
                    help='独立/常驻 的期望最小倍数；0 = 只测量')
    a = ap.parse_args()
    cc = pick_cc(a.cc)
    if not os.path.isabs(cc):
        cc = os.path.join(REPO, cc)
    if not os.path.exists(cc):
        print('[FAIL] 编译器不存在: %s' % cc)
        return 1

    tmp = tempfile.mkdtemp(prefix='qcc_bench_')
    srcs = []
    for i in range(a.n):
        p = os.path.join(tmp, 'b%03d.c' % i)
        with open(p, 'w', encoding='utf-8', newline='\n') as f:
            f.write('#include <stdio.h>\nint main(void){ printf("B%d\\n"); return %d; }\n'
                    % (i, i % 3))
        srcs.append(p)

    print('编译器: %s' % cc)
    print('样本: %d 个不同源' % a.n)

    # A) 独立进程
    t0 = time.time()
    ok_a = 0
    for i, sp in enumerate(srcs):
        r = subprocess.run([cc, sp, '-o', os.path.join(tmp, 'a%03d.exe' % i)],
                           cwd=REPO, capture_output=True)
        ok_a += (r.returncode == 0)
    t_indep = time.time() - t0
    print('[A] 独立进程 %d 次: %.2fs（%.1f ms/次，成功 %d/%d）'
          % (a.n, t_indep, t_indep * 1000 / a.n, ok_a, a.n))

    # B) 常驻
    p = subprocess.Popen([cc, '--serve'], cwd=REPO, stdin=subprocess.PIPE,
                         stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    t0 = time.time()
    ok_b = 0
    for i, sp in enumerate(srcs):
        msg = json.dumps({'cmd': 'compile', 'file': sp, 'out': os.path.join(tmp, 'b%03d.exe' % i)},
                         ensure_ascii=False, separators=(',', ':')) + '\n'
        p.stdin.write(msg.encode('utf-8'))
        p.stdin.flush()
        resp = p.stdout.readline().decode('utf-8', 'replace')
        ok_b += ('"ok":true' in resp)
    t_serve = time.time() - t0
    p.stdin.write(b'{"cmd":"quit"}\n')
    p.stdin.flush()
    p.stdout.readline()
    try:
        p.wait(timeout=20)
    except subprocess.TimeoutExpired:
        p.kill()
    print('[B] 常驻 serve %d 次: %.2fs（%.1f ms/次，成功 %d/%d）'
          % (a.n, t_serve, t_serve * 1000 / a.n, ok_b, a.n))

    ratio = (t_indep / t_serve) if t_serve > 0 else 0.0
    print('[比值] 独立/常驻 = %.2fx（%.1f ms vs %.1f ms）'
          % (ratio, t_indep * 1000 / a.n, t_serve * 1000 / a.n))
    if a.expect_ratio > 0:
        if ratio < a.expect_ratio:
            print('[FAIL] 未达期望 %.1fx（实测 %.2fx）' % (a.expect_ratio, ratio))
            return 1
        print('[PASS] 达到期望 %.1fx' % a.expect_ratio)
    return 0


if __name__ == '__main__':
    sys.exit(main())
