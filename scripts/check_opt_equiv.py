#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""check_opt_equiv —— 优化链 **行为等价** 判据（R880）。

对 20 个 锚 + 5 个 真实语料源，各走 两条链：
  未优化: `cc -S` → `asm_zh`     优化: `cc -S` → `opt_zh` → `asm_zh`
断言：**两边 程序输出 逐字节 相同**、退出码 相同（不是「都能编过」就算）。
选材 规则（可复算）：锚 = `CORE_FEAT` 里 按序 取 前 20 个 **未优化链 能编能跑** 的；
语料 = `scratch_gl`（git 语料）里 按序 取 前 5 个 能编能跑 的。取不到 20/5 个 ⟹ 判红（不给「抽不到」留口子）。

用法: python scripts/check_opt_equiv.py [--cc qcc_zh_v1.exe] [--anchors 20] [--corpus 5] [--work <dir>]
退出码: 0 = 全等；1 = 有不等/取材不足；2 = 环境未就绪
"""
import argparse
import io
import os
import subprocess
import sys

sys.stdout.reconfigure(encoding='utf-8', errors='replace')
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
os.chdir(ROOT)
CORPUS_DIR = r'C:\Users\Administrator\Desktop\grok-build\scratch_gl'


def run(cmd, t=900):
    return subprocess.run(cmd, capture_output=True, timeout=t)


def load_core_feat():
    p = os.path.join(ROOT, 'scripts', 'run_tests_zh.py')
    s = io.open(p, encoding='utf-8', errors='replace').read()
    return s


def anchor_list(n):
    """从 run_tests_zh 的 CORE_FEAT 取候选（按出现顺序，去重）。"""
    s = load_core_feat()
    i = s.find('CORE_FEAT')
    j = s.find('def build_tests')
    seg = s[i:j] if i >= 0 and j > i else s
    out = []
    for m in __import__('re').finditer(r"'(scripts/[^']+\.c)'", seg):
        p = m.group(1)
        if p not in out:
            out.append(p)
    return out[: max(n * 6, n)]


def corpus_list():
    out = []
    if not os.path.isdir(CORPUS_DIR):
        return out
    for root, _, files in os.walk(CORPUS_DIR):
        for f in sorted(files):
            if f.endswith('.c'):
                out.append(os.path.join(root, f))
    return out


def one(cc, opt, asm, src, work, tag):
    """返 (ok, detail)。ok=False 表示 未优化链 无法取材（跳过）或 两链不等（红）。"""
    base = os.path.join(work, tag)
    strict = getattr(one, 'strict', False)
    r = run([cc, '-S', src, '-o', base + '.exe'])
    if r.returncode != 0:
        return (False, 'skip') if not strict else (False, 'unopt 编译失败')
    s = base + '.exe.asm'
    if not os.path.exists(s):
        return (False, 'skip')
    r = run([asm, s, '-o', base + '_n_a.exe'])
    if r.returncode != 0:
        return (False, 'skip')
    p1 = run([base + '_n_a.exe'], 300)
    r = run([opt, s, '-o', base + '_o.asm'])
    if r.returncode != 0:
        return False, 'opt 失败: ' + (r.stdout + r.stderr).decode('utf-8', 'replace')[:120]
    r = run([asm, base + '_o.asm', '-o', base + '_o_a.exe'])
    if r.returncode != 0:
        return False, 'asm(优化) 失败'
    p2 = run([base + '_o_a.exe'], 300)
    if p1.stdout != p2.stdout or p1.returncode != p2.returncode:
        return False, '输出 不等（rc %d/%d, %d vs %d 字节）' % (p1.returncode, p2.returncode, len(p1.stdout), len(p2.stdout))
    return True, 'SAME'


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cc', default='qcc_zh_v1.exe')
    ap.add_argument('--anchors', type=int, default=20)
    ap.add_argument('--corpus', type=int, default=5)
    ap.add_argument('--work', default=os.path.join(os.environ.get('TEMP', '.'), 'qcc_opt_equiv'))
    a = ap.parse_args()
    cc = a.cc if os.path.isabs(a.cc) else os.path.join(ROOT, a.cc)
    opt = os.path.join(ROOT, 'opt_zh.exe')
    if not os.path.exists(opt):
        r = subprocess.run([cc, os.path.join(ROOT, 'srclib', 'opt_zh.jy'), '-o', opt], capture_output=True)
        if r.returncode != 0:
            print('[FAIL] 出厂优化器 缺失 且 现编 失败'); return 2
    asm = os.path.join(ROOT, 'asm_zh.exe')
    if not all(os.path.exists(x) for x in (cc, opt, asm)):
        print('[FAIL] 环境未就绪'); return 2
    os.makedirs(a.work, exist_ok=True)
    fails = []

    def collect(cands, want, prefix, strict):
        got = []
        for k, src in enumerate(cands):
            if len(got) >= want:
                break
            one.strict = strict
            ok, det = one(cc, opt, asm, src, a.work, '%s%03d' % (prefix, k))
            if ok and det == 'SAME':
                got.append(src)
            elif det not in ('skip',):
                fails.append('%s: %s' % (os.path.basename(src), det))
        return got

    anchors = collect(anchor_list(a.anchors), a.anchors, 'a', False)
    print('锚: 取到 %d/%d 个可跑 且 两链 输出 相同' % (len(anchors), a.anchors))
    if len(anchors) < a.anchors:
        fails.append('锚 取材不足 (%d/%d)' % (len(anchors), a.anchors))

    corpus = collect(corpus_list(), a.corpus, 'c', False)
    print('语料: 取到 %d/%d 个可跑 且 两链 输出 相同' % (len(corpus), a.corpus))
    if len(corpus) < a.corpus:
        fails.append('语料 取材不足 (%d/%d)' % (len(corpus), a.corpus))

    print('=' * 60)
    if fails:
        print('等价 判据: FAIL -> %s' % '; '.join(fails[:6]))
        return 1
    print('等价 判据: PASS（20 锚 + 5 语料 两链 输出 逐字节 相同）')
    return 0


if __name__ == '__main__':
    sys.exit(main())
