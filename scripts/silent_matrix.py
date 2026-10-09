#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""silent_matrix.py —— 「静默兜底」8 槽 触发率矩阵（QCC_DBG_SILENT=1 · 逐文件实测）

用途（本计划 verification 第 4 步的量化判据）:
  对同一批语料、同一套参数, 用 **两个编译器**（父编 / 现役）各跑一遍,
  比对各槽合计 ⟹ 修复 是否 真的 把 槽 N 的 触发 压下去（自对照, 不受文件集口径影响）。

机制: 编译器 第0刀 报警器（`静默计(槽,名)`）默认零输出; `QCC_DBG_SILENT=1` 时
      stderr 逐次打 `[静默兜底] <名> (第 N 次)`, 编译尾 打 `[静默兜底 汇总]` + `    槽K: <名> x<n>`。

用法:
  python scripts/silent_matrix.py --cc qcc_zh_v1.exe --corpus lua,zlib --parallel 6 --json out.json
  可选 --keep-all 表示 连 下划线 前缀（前几轮 手工副本 `_cop*.c` / `_lh*.c`）也计入。
"""
import argparse
import json
import os
import re
import subprocess
import sys
import time
from concurrent.futures import ProcessPoolExecutor

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
os.chdir(REPO)
CORPUS_ROOT = r'D:\qiyuan_home\corpus'
TMP = os.path.join(os.environ.get('TEMP', '.'), '_silent_matrix')
os.makedirs(TMP, exist_ok=True)

TARGETS = {
    'lua':   dict(dir='lua-5.4.7',      defs=['-D', 'LUA_USE_WINDOWS']),
    'zlib':  dict(dir='zlib-1.3.1',     defs=[]),
    'libpng': dict(dir='libpng-1.6.43', defs=[]),
}

SUMM = re.compile(r'槽(\d):\s*(.+?)\s*x(\d+)')


def files_of(name, keep_all=False):
    d = os.path.join(CORPUS_ROOT, TARGETS[name]['dir'])
    out = []
    for f in sorted(os.listdir(d)):
        if not f.endswith('.c'):
            continue
        if f.startswith('_') and not keep_all:
            continue                      # 前几轮 手工副本（_cop*/_lh*/_lo*）不计
        out.append(os.path.join(d, f))
    return out


def compile_one(args):
    cc, src, defs, tag = args
    obj = os.path.join(TMP, '%s_%s.obj' % (tag, os.path.basename(src)[:-2]))
    env = dict(os.environ, QCC_DBG_SILENT='1')
    cmd = [os.path.abspath(cc), '--lenient'] + defs + ['-I', os.path.dirname(src),
                                                       '-c', src, '-o', obj]
    t0 = time.time()
    try:
        r = subprocess.run(cmd, capture_output=True, encoding='utf-8', errors='replace',
                           env=env, timeout=180, cwd=REPO)
        rc, txt = r.returncode, (r.stdout or '') + (r.stderr or '')
    except subprocess.TimeoutExpired:
        rc, txt = 999, '[TIMEOUT]'
    sec = round(time.time() - t0, 1)
    slots = {}
    for m in SUMM.finditer(txt):
        slots[int(m.group(1))] = int(m.group(3))
    names = {int(m.group(1)): m.group(2) for m in SUMM.finditer(txt)}
    hits = len(re.findall(r'\[静默兜底\]', txt))
    return dict(file=os.path.basename(src), rc=rc, sec=sec, slots=slots,
                names=names, hits=hits)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cc', required=True)
    ap.add_argument('--corpus', default='lua,zlib')
    ap.add_argument('--parallel', type=int, default=6)
    ap.add_argument('--json', default='')
    ap.add_argument('--keep-all', action='store_true')
    ap.add_argument('--detail', action='store_true')
    a = ap.parse_args()

    cc = a.cc if os.path.isabs(a.cc) else os.path.join(REPO, a.cc)
    out = {}
    for name in [x for x in a.corpus.split(',') if x]:
        fs = files_of(name, a.keep_all)
        tag = os.path.basename(cc).replace('.exe', '')
        tasks = [(cc, f, TARGETS[name]['defs'], tag) for f in fs]
        t0 = time.time()
        with ProcessPoolExecutor(max_workers=a.parallel) as ex:
            res = list(ex.map(compile_one, tasks))
        tot = {}
        nm = {}
        for r in res:
            for k, v in r['slots'].items():
                tot[k] = tot.get(k, 0) + v
                nm[k] = r['names'].get(k, '')
            if a.detail and r['slots']:
                print('  %-14s rc=%-3d %s' % (r['file'], r['rc'],
                      ' '.join('槽%d=%d' % (k, v) for k, v in sorted(r['slots'].items()))))
        ok = sum(1 for r in res if r['rc'] == 0)
        out[name] = dict(files=len(res), ok=ok, fail=len(res) - ok,
                         sec=round(time.time() - t0, 1),
                         slots={str(k): v for k, v in sorted(tot.items())},
                         names={str(k): nm[k] for k in sorted(nm)})
        print('%s @ %s  文件 %d (ok %d / fail %d)  %s  用时 %.1fs' %
              (name, os.path.basename(cc), len(res), ok, len(res) - ok,
               ' '.join('槽%s:%d' % (k, v) for k, v in sorted(out[name]['slots'].items())) or '零静默',
               out[name]['sec']))
    if a.json:
        with open(a.json, 'w', encoding='utf-8') as f:
            json.dump(out, f, ensure_ascii=False, indent=1)
        print('JSON →', a.json)
    return 0


if __name__ == '__main__':
    sys.exit(main())
