# -*- coding: utf-8 -*-
"""ORD-44 复现：lua 语料仍有 5 个「gcc 能编、甲言编不过」的 P0。blocked（C 兼容度战线，需逐个修）。

判据（用真编译器，不看旧 summary）：
  取 `D:\qiyuan_home\corpus\lua-5.4.7\lmathlib.c`（施工单 §1.3 首例），`qcc -c` 应失败（A1004）。
  · 失败 ⟹ 缺陷复现（rc=1，blocked 诚实）
  · 成功 ⟹ 至少该例已修，应复核其余 4 例后转 fixed
用法: python scripts/check_corpus_diff.py [--cc qcc_zh_v1.exe] [--file <路径>]
"""
import argparse, os, subprocess, sys
sys.stdout.reconfigure(encoding='utf-8', errors='replace')

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DEFAULT = r'D:\qiyuan_home\corpus\lua-5.4.7\lmathlib.c'


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cc', default=os.environ.get('QCC_TEST_BIN') or 'qcc_zh_v1.exe')
    ap.add_argument('--file', default=DEFAULT)
    a = ap.parse_args()
    cc = a.cc if os.path.isabs(a.cc) else os.path.join(REPO, a.cc)
    if not os.path.exists(a.file):
        print('[SKIP] 语料不存在: %s' % a.file)
        return 0
    out = os.path.join(os.environ.get('QCC_TMPDIR', os.path.join(REPO, 'scratch_corpus')), 'lmathlib.o')
    os.makedirs(os.path.dirname(out), exist_ok=True)
    p = subprocess.run([cc, '-c', a.file, '-o', out], cwd=REPO, capture_output=True, text=True,
                       encoding='utf-8', errors='replace')
    err = ((p.stderr or '').strip().splitlines() or [''])[0][:120]
    print('[%s] rc=%d %s' % (os.path.basename(a.file), p.returncode, err))
    if p.returncode != 0:
        print('[REPRO] 语料兼容缺陷复现 ⟹ blocked 诚实')
        return 1
    print('[PASS] 该例已能编 ⟹ 复核其余 4 例后转 fixed')
    return 0


if __name__ == '__main__':
    sys.exit(main())
