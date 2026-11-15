#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""check_opt_selfhost —— P0 判据：发布链（-S → opt_zh → asm_zh）能自举收敛。

判据（缺一不可）：
  1. 优化链产物 code 字节 < 未优化链（默认下限 5%）
  2. 第二代的优化产物与第一代 **逐字节相同**（不动点）
  3. 两代都能编出可运行的编译器（第二代跑 --help 成功）

用法:
  python scripts/check_opt_selfhost.py [--cc qcc_zh_seed.exe] [--src srclib_jiayan/qcc_zh.jy]
                                       [--work D:\\qcc_tmp\\opt_selfhost] [--min-pct 5]
退出码: 0 = 通过；1 = 判红；2 = 环境未就绪（缺 opt_zh/asm_zh）
"""
import argparse
import hashlib
import io
import os
import re
import subprocess
import sys

sys.stdout.reconfigure(encoding='utf-8', errors='replace')
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
os.chdir(ROOT)
try:
    from qcc_tmp import tmp_dir
except Exception:  # pragma: no cover
    tmp_dir = lambda name: os.path.join('build', name)


def run(cmd, t=3600):
    r = subprocess.run(cmd, capture_output=True, timeout=t)
    out = (r.stdout + r.stderr).decode('utf-8', 'replace')
    return r.returncode, out


def code_bytes(out):
    m = re.search(r'\((\d+) bytes code', out)
    return int(m.group(1)) if m else -1


def sha8(p):
    h = hashlib.sha256(io.open(p, 'rb').read()).hexdigest()
    return h[:8].upper()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cc', default='qcc_zh_seed.exe')
    ap.add_argument('--src', default=os.path.join('srclib_jiayan', 'qcc_zh.jy'))
    ap.add_argument('--work', default=None)
    ap.add_argument('--min-pct', type=float, default=5.0)
    ap.add_argument('--generations', type=int, default=2,
                    help='1 = 只跑一代并与 --expect 比对（约 8 min，适合带护栏的判据链）；2 = 两代收敛（约 16 min）')
    ap.add_argument('--expect', default='', help='一代模式的期望 sha8（P0 = 12895A81）')
    ap.add_argument('--opt', default='opt_zh.exe')
    ap.add_argument('--full-opt', action='store_true',
                    help='走完整元数据通道（默认 = --peep-only，与 build_zh.ps1 -Opt 默认一致）')
    a = ap.parse_args()

    for p in (a.opt, 'asm_zh.exe', a.cc):
        if not os.path.exists(p):
            print('环境未就绪：缺 %s' % p)
            return 2

    W = a.work or tmp_dir('opt_selfhost')
    os.makedirs(W, exist_ok=True)
    fails = []
    fails_early = []

    def one(cc, tag):
        base = os.path.join(W, tag + '.exe')
        rc, out = run([cc, '-S', a.src, '-o', base])
        if rc != 0:
            print('FAIL %s: -S 失败 rc=%d\n%s' % (tag, rc, out[-800:]))
            return None
        raw_bytes = code_bytes(out)
        asm = base + '.asm'
        if not os.path.exists(asm):
            print('FAIL %s: 缺 %s' % (tag, asm))
            return None
        oasm = os.path.join(W, tag + '_opt.asm')
        opt_cmd = [a.opt, asm, '-o', oasm] + ([] if a.full_opt else ['--peep-only'])
        rc, out = run(opt_cmd)
        for _l in out.splitlines():
            if _l.startswith('[耗时]'):
                print('  %s %s' % (tag, _l))
                _m = re.search(r'段和=(\d+)\s+总=(\d+)\s+ms', _l)
                if not _m:
                    fails_early.append('%s 耗时行缺 段和/总' % tag)
                else:
                    _s, _t = int(_m.group(1)), int(_m.group(2))
                    if _t <= 0 or _s * 100 < _t * 95:
                        fails_early.append('%s 分段耗时和/总 = %d/%d < 95%%' % (tag, _s, _t))
                    else:
                        print('  %s 分段和/总 = %d/%d (%.1f%%)' % (tag, _s, _t, 100.0 * _s / _t))
        if rc != 0:
            print('FAIL %s: opt_zh 失败 rc=%d\n%s' % (tag, rc, out[-800:]))
            return None
        exe = os.path.join(W, tag + '_opt.exe')
        rc, out = run(['asm_zh.exe', oasm, '-o', exe])
        if rc != 0:
            print('FAIL %s: asm_zh 失败 rc=%d\n%s' % (tag, rc, out[-800:]))
            return None
        optb = code_bytes(out)
        return {'tag': tag, 'exe': exe, 'raw': raw_bytes, 'opt': optb, 'sha': sha8(exe), 'asm': asm}

    r1 = one(a.cc, 'gen1')
    fails.extend(fails_early)
    if r1 is None:
        if fails:
            print('\n[失败 FAIL] ' + '；'.join(fails))
        return 1
    pct = (r1['raw'] - r1['opt']) * 100.0 / r1['raw'] if r1['raw'] > 0 else 0.0
    print('gen1: raw=%d opt=%d (%.3f%%) sha=%s' % (r1['raw'], r1['opt'], pct, r1['sha']))
    if pct < a.min_pct:
        fails.append('code 字节降幅 %.3f%% < %.1f%%' % (pct, a.min_pct))

    rc, out = run([r1['exe'], '--help'], 300)
    if rc != 0 or 'Usage' not in out:
        fails.append('gen1 编译器 --help 失败 rc=%d' % rc)

    if a.generations == 1:
        if not a.expect:
            fails.append('一代模式必须给 --expect（否则无法断言收敛）')
        elif r1['sha'] != a.expect.upper():
            fails.append('一代产物 sha %s != 期望 %s' % (r1['sha'], a.expect.upper()))
        if fails:
            print('\n[失败 FAIL] ' + '；'.join(fails))
            return 1
        print('\n[通过 PASS] 优化链一代 sha=%s（= 期望），code %d→%d（-%.3f%%）'
              % (r1['sha'], r1['raw'], r1['opt'], pct))
        return 0

    r2 = one(r1['exe'], 'gen2')
    fails.extend(fails_early)
    if r2 is None:
        if fails:
            print('\n[失败 FAIL] ' + '；'.join(fails))
        return 1
    print('gen2: raw=%d opt=%d sha=%s' % (r2['raw'], r2['opt'], r2['sha']))
    if r2['sha'] != r1['sha']:
        fails.append('两代优化产物不等: %s != %s' % (r1['sha'], r2['sha']))
    if r2['opt'] != r1['opt']:
        fails.append('两代 code 字节不等: %d != %d' % (r1['opt'], r2['opt']))

    if fails:
        print('\n[失败 FAIL] ' + '；'.join(fails))
        return 1
    print('\n[通过 PASS] 优化链自举收敛 sha=%s，code %d→%d（-%.3f%%）' % (r1['sha'], r1['raw'], r1['opt'], pct))
    return 0


if __name__ == '__main__':
    sys.exit(main())
