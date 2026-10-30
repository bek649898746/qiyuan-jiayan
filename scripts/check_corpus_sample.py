# -*- coding: utf-8 -*-
"""check_corpus_sample — 语料 `.c` 抽样编译矩阵（verification plan step 3 的可复算半）。

由来（2026-10-30 · skeptic 打回）：plan step 3 要求「同目录抽样 >=10 个 `.c` 的失败数不高于修前；
抽样中至少一个失败样本在修后变成可编」，而证据里只有单个 lmathlib ⟹ 无法复核「不是只修了一个特例」。

设计：
  · 用 `--cc` 指定编译器对 `--dir` 下（递归、限深）的 `.c` 逐个 `-c`；
  · 输出逐文件 `rc`，末行 `SAMPLES: total=N fail=K pass=P`（机读）；
  · `--expect-fail <= K` 时：`fail <= K` ⟹ rc=0，否则 rc=1（用于「不高于修前」）；
  · 只做单文件 `-c`，不起进程池（铁律：重负载串行，不并行两个编译器）。

用法:
  python scripts/check_corpus_sample.py --cc qcc_zh_v1.exe --dir D:\\qiyuan_home\\corpus\\lua-5.4.7 --n 12
  python scripts/check_corpus_sample.py --cc <修前exe> --dir ... --n 12   > corpus_before.log
  python scripts/check_corpus_sample.py --cc qcc_zh_v1.exe --dir ... --n 12 --expect-fail 2
"""
import argparse
import io
import os
import subprocess
import sys

sys.stdout.reconfigure(encoding='utf-8', errors='replace')
REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def pick(root, n, names=None):
    """稳定抽样：按相对路径排序取前 n 个（可复算，不随机）。"""
    got = []
    for dp, dns, fns in os.walk(root):
        dns.sort()
        for fn in sorted(fns):
            if fn.endswith('.c'):
                got.append(os.path.join(dp, fn))
    got.sort()
    if names:
        want = [w.lower() for w in names]
        sel = [p for p in got if os.path.basename(p).lower() in want]
        rest = [p for p in got if p not in sel]
        return (sel + rest)[:max(n, len(sel))]
    # 抽样为「首 n 个 + 均匀覆盖」：先取前 n/2，再在余下按等距取，保证跨目录覆盖
    if len(got) <= n:
        return got
    head = got[:n // 2]
    rest = got[n // 2:]
    step = max(1, len(rest) // (n - len(head)))
    tail = rest[::step][:n - len(head)]
    return head + tail


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cc', default=os.environ.get('QCC_TEST_BIN') or 'qcc_zh_v1.exe')
    ap.add_argument('--dir', required=True)
    ap.add_argument('--n', type=int, default=12)
    ap.add_argument('--files', nargs='*', default=None, help='强制包含的基名（如点名样本）')
    ap.add_argument('--expect-fail', type=int, default=None,
                    help='允许的最大失败数（不高于修前）；超了 rc=1')
    ap.add_argument('--scratch', default=os.environ.get('QCC_TMPDIR') or os.path.join(REPO, 'scratch_corpus'))
    ap.add_argument('--timeout', type=int, default=300)
    a = ap.parse_args()

    cc = a.cc if os.path.isabs(a.cc) else os.path.join(REPO, a.cc)
    if not os.path.exists(cc):
        print('[FAIL] 编译器不存在: %s' % cc)
        return 2
    if not os.path.isdir(a.dir):
        print('[SKIP] 语料目录不存在: %s' % a.dir)
        return 0
    os.makedirs(a.scratch, exist_ok=True)
    sel = pick(a.dir, a.n, a.files)
    print('# cc=%s' % os.path.basename(cc))
    print('# dir=%s 抽样=%d' % (a.dir, len(sel)))
    fails = []
    for i, src in enumerate(sel, 1):
        obj = os.path.join(a.scratch, 's%02d.o' % i)
        try:
            p = subprocess.run([cc, '-c', src, '-o', obj], cwd=REPO, capture_output=True, text=True,
                               encoding='utf-8', errors='replace', timeout=a.timeout)
            rc, msg = p.returncode, ((p.stderr or '').strip().splitlines() or [''])[0][:100]
        except subprocess.TimeoutExpired:
            rc, msg = -99, 'TIMEOUT'
        rel = os.path.relpath(src, a.dir).replace('\\', '/')
        print('%-28s rc=%3d %s' % (rel, rc, msg))
        if rc != 0:
            fails.append(rel)
    total = len(sel)
    print('SAMPLES: total=%d fail=%d pass=%d' % (total, len(fails), total - len(fails)))
    if fails:
        print('FAILED: %s' % ', '.join(fails))
    if a.expect_fail is not None:
        ok = len(fails) <= a.expect_fail
        print('[%s] fail=%d <= expect-fail=%d' % ('PASS' if ok else 'FAIL', len(fails), a.expect_fail))
        return 0 if ok else 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
