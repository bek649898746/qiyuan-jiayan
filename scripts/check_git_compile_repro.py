# -*- coding: utf-8 -*-
"""check_git_compile_repro —— 「用当前编译器重编 git」的可复现判据（ORD-33）。

判据（全部走真实编译器子进程，不 mock）：
  ① **10 个核心 .c** 在现行配置（`-D _WIN32=1`）下全部 rc=0：
     diff.c · dir.c · strbuf.c · read-cache.c · object.c · hex.c ·
     builtin/add.c · builtin/commit.c · builtin/hash-object.c · compat/stat.c
     （这是施工单 §七.2 的抽样面；diff.c 是 A6109 的原始现场）
  ② `diff.c` 在**三种配置**下均 rc=0：A `_WIN32` / B `+__MINGW32__` / C `+_MSC_VER`
  ③ **同一源码、同一配置、同一编译器连编两次，产物逐字节相同**
  ④ 任一出现 `A6109 重复定义` ⟹ 失败（原始病灶）
  ⑤ **树不存在 ⟹ 失败（rc=1）**，不再 rc=0 静默跳过 —— 否则闸会在没有 git 树的机器上「假绿」。

用法: python scripts/check_git_compile_repro.py [--cc qcc_zh_v1.exe] [--tree <git 根>]
"""
import argparse
import hashlib
import os
import subprocess
import sys
sys.stdout.reconfigure(encoding='utf-8', errors='replace')

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(HERE)
PRELUDE = os.path.join(REPO, 'compat_prelude.h')
TMP = os.environ.get('QCC_TMPDIR') or os.path.join(REPO, 'scratch_gitrepro')

CORE = ['diff.c', 'dir.c', 'strbuf.c', 'read-cache.c', 'object.c', 'hex.c',
        'builtin/add.c', 'builtin/commit.c', 'builtin/hash-object.c', 'compat/stat.c',
        'builtin/diff.c']
CONFIGS = [
    ('A_win32', ['_WIN32=1']),
    ('B_mingw', ['_WIN32=1', '__MINGW32__=1']),
    ('C_msvc', ['_WIN32=1', '_MSC_VER=1900']),
]


def sha8(path):
    h = hashlib.sha256()
    with open(path, 'rb') as f:
        for chunk in iter(lambda: f.read(1 << 20), b''):
            h.update(chunk)
    return h.hexdigest()[:8]


def build(cc, tree, src, defs, out):
    args = [cc, '-D', '__STDC_VERSION__=199901L', '-D', '__GNUC__=5']
    for d in defs:
        args += ['-D', d]
    args += ['-D', 'REG_STARTEND=1', '-I', PRELUDE, '-c', src, '-o', out]
    p = subprocess.run(args, cwd=tree, capture_output=True, text=True, encoding='utf-8', errors='replace')
    return p.returncode, (p.stdout or '') + (p.stderr or '')


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cc', default=os.environ.get('QCC_TEST_BIN') or 'qcc_zh_v1.exe')
    ap.add_argument('--tree', default=os.environ.get('QCC_GIT_TREE') or
                   os.path.join(os.path.dirname(REPO), 'git-2.45.2'))
    a = ap.parse_args()
    cc = a.cc if os.path.isabs(a.cc) else os.path.join(REPO, a.cc)
    tree = os.path.abspath(a.tree)
    if not os.path.exists(cc):
        print('[FAIL] 编译器不存在: %s' % cc)
        return 1
    if not (os.path.isdir(tree) and os.path.exists(os.path.join(tree, 'diff.c'))):
        print('[FAIL] git 树不存在: %s —— 本判据不做静默跳过（置 QCC_GIT_TREE 指向适配树）' % tree)
        return 1
    os.makedirs(TMP, exist_ok=True)
    fails = []

    # ① 10 个核心文件（现行配置）
    for src in CORE:
        out = os.path.join(TMP, src.replace('/', '_') + '.o')
        rc, log = build(cc, tree, src, CONFIGS[0][1], out)
        if 'A6109' in log:
            fails.append('%s: A6109 重复定义' % src)
        elif rc != 0:
            first = (log.strip().splitlines() or [''])[0][:110]
            fails.append('%s: rc=%d %s' % (src, rc, first))

    # ② diff.c 与 builtin/diff.c **各自** 三种配置（plan 步骤 2 原文：含 builtin/diff.c 的三种配置）
    hashes = {}
    for src in ('diff.c', 'builtin/diff.c'):
        tag = src.replace('/', '_')
        for name, defs in CONFIGS:
            out = os.path.join(TMP, '%s_%s.o' % (tag, name))
            rc, log = build(cc, tree, src, defs, out)
            if 'A6109' in log:
                fails.append('%s/%s: A6109 重复定义' % (src, name))
            elif rc != 0:
                fails.append('%s/%s: rc=%d' % (src, name, rc))
            elif os.path.exists(out) and src == 'diff.c':
                hashes[name] = sha8(out)
    if 'A_win32' in hashes:
        out2 = os.path.join(TMP, 'diff_A_again.o')
        rc, log = build(cc, tree, 'diff.c', CONFIGS[0][1], out2)
        if rc != 0 or sha8(out2) != hashes['A_win32']:
            fails.append('可复现: 连编两次产物不同或失败')

    if fails:
        print('[FAIL] git 重编可复现判据: %d 项' % len(fails))
        for f in fails:
            print('   - ' + f)
        return 1
    print('[PASS] git 重编: %d/%d 核心文件 rc=0 · diff.c + builtin/diff.c 各三配置 rc=0（A=%s B=%s C=%s）· 连编两次逐字节相同'
          % (len(CORE), len(CORE), hashes.get('A_win32'), hashes.get('B_mingw'), hashes.get('C_msvc')))
    return 0


if __name__ == '__main__':
    sys.exit(main())
