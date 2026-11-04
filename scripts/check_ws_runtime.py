# -*- coding: utf-8 -*-
"""check_ws_runtime — 工作窃取运行时竞态的可执行判据（WS61-RACE）。

背景（2026-11-02 复核）：全量锚 `feat_zh_ws61_ok.c` 偶发挂死/计数错。根因两处真竞态：
  ① 旧自旋锁 `xadd(+1)` 失败回滚 与 持有者释放 交叉 ⟹ 锁值变负，永久死锁；
  ② `__ws_out` 锁内 plain RMW、锁外 原子 RMW 混用 ⟹ 丢更新（final out=1）。
修法：票据锁 + out/kids_d/steals 全原子（见 srclib/qcc_rt.c）。

判据：
  A 原子混合加/减：4 线程 × 200000 次 `原子 加(&x,1)` + `原子 加(&x,-1)` ⟹ x == 0（5 次）。
  B ws61 锚：顺序跑 100 次 + 8 路并发 48 次 ⟹ 全部 rc=0 且输出 `WS61 36 8 8 1`。

用法: python scripts/check_ws_runtime.py [--cc qcc_zh_v1.exe]
"""
from qcc_tmp import tmp_dir, tmp_root
import argparse
import concurrent.futures
import io
import os
import subprocess
import sys
import tempfile

sys.stdout.reconfigure(encoding='utf-8', errors='replace')
REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

MIXED = '''#包含 <stdio.h>
整 x = 0;
静 整 工(空 *实参) {
    遍 (整 i = 0; i < 200000; i++) { 原子 加(&x, 1); 原子 加(&x, -1); }
    返 0;
}
函数 整 主(空) {
    线程池 池, 4;
    提交 池, 工, (空 *)0;
    提交 池, 工, (空 *)0;
    提交 池, 工, (空 *)0;
    提交 池, 工, (空 *)0;
    等待池 池;
    格式化 "x=%d\\n", x;
    返 0;
}
'''


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cc', default=os.environ.get('QCC_TEST_BIN') or 'qcc_zh_v1.exe')
    a = ap.parse_args()
    cc = a.cc if os.path.isabs(a.cc) else os.path.join(REPO, a.cc)
    work = tmp_dir('qcc_wsrt')
    bad = []

    def comp(src, out, timeout=300):
        r = subprocess.run([cc, src, '-o', out], cwd=REPO, capture_output=True, text=True,
                           encoding='utf-8', errors='replace', timeout=timeout)
        return r.returncode, ((r.stdout or '') + (r.stderr or ''))

    # A 原子混合加/减
    src = os.path.join(work, 'mixed.c')
    io.open(src, 'w', encoding='utf-8', newline='\n').write(MIXED)
    mex = os.path.join(work, 'mixed.exe')
    rc, out = comp(src, mex)
    if rc != 0:
        bad.append('原子混合 编译失败 rc=%d %r' % (rc, out.strip()[-120:]))
    else:
        for i in range(5):
            q = subprocess.run([mex], capture_output=True, text=True, encoding='utf-8',
                               errors='replace', timeout=120)
            if q.returncode != 0 or 'x=0' not in (q.stdout or ''):
                bad.append('原子混合 第%d次 不符 rc=%d out=%r' % (i + 1, q.returncode, (q.stdout or '').strip()[:40]))

    # B ws61 锚
    ws = os.path.join(REPO, 'scripts', 'feat_zh_ws61_ok.c')
    wex = os.path.join(work, 'ws61.exe')
    rc, out = comp(ws, wex)
    if rc != 0:
        bad.append('ws61 编译失败 rc=%d %r' % (rc, out.strip()[-120:]))
        seq_ok = par_ok = -1
    else:
        def run(_):
            r = subprocess.run([wex], capture_output=True, text=True, encoding='utf-8',
                               errors='replace', timeout=10)
            return (r.returncode, (r.stdout or '').strip())

        seq = [run(i) for i in range(100)]
        seq_ok = sum(1 for rc2, o in seq if rc2 == 0 and 'WS61 36 8 8 1' in o)
        with concurrent.futures.ThreadPoolExecutor(max_workers=8) as ex:
            par = list(ex.map(run, range(48)))
        par_ok = sum(1 for rc2, o in par if rc2 == 0 and 'WS61 36 8 8 1' in o)
        if seq_ok != 100:
            bad.append('ws61 顺序 通过 %d/100' % seq_ok)
        if par_ok != 48:
            bad.append('ws61 8路并发 通过 %d/48' % par_ok)

    print('[A] 原子混合 +/- 4线程×200k ×5: %s' % ('PASS' if not any('原子混合' in x for x in bad) else 'FAIL'))
    print('[B] ws61 顺序=%s/100 并发=%s/48' % (seq_ok, par_ok))
    for b in bad:
        print('[FAIL] ' + b)
    print('== 工作窃取运行时判据: %s ==' % ('PASS' if not bad else 'FAIL (%d 处)' % len(bad)))
    return 0 if not bad else 1


if __name__ == '__main__':
    sys.exit(main())
