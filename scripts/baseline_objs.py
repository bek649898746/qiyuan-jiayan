# -*- coding: utf-8 -*-
"""baseline_objs —— 战线 4: 多库 **编译产物基线** (捕捉「编译过但产物静默变化」)。

  背景 (2026-09-18 第 102 站): 语料闸只判 rc（编译过/不过），对「编译过但生成的代码变了」
  **零感知** —— 而 C292/C299 那类修复恰恰是「rc 不变、行为变」。**产物哈希能看见这一层**。
  ⟹ 「基线不可后补」：现在不存，将来无法回看旧编译器到底生成过什么。

  用法:
    python scripts/baseline_objs.py save <qcc.exe>   # 生成/更新 基线 (按编译器 tag 分文件)
    python scripts/baseline_objs.py diff <qcc.exe>   # 与 同 tag 基线 对照, 列出变化文件

  基线位置: <scratch_gl>/baselines/objs_<TAG>.json   (TAG = 编译器 sha256 前 8 位)
  ⚠️ 产物随编译器版本变化, 故**按 tag 分文件**：只有「同一 tag」下的 diff 才有意义
     （典型用法：改了编译器某处后重编，看**无关文件的产物是否纹丝不动**）。

  语料树 (与本机一致, 可按需改 TREES):
    lua   ~/.openclaw/audit/luarun/lua              (+ -D __GNUC__=5, 与 lua_run.py 一致)
    zlib  <cargo registry>/libz-sys-1.1.22/src/zlib
"""
import hashlib, io, json, os, subprocess, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))          # qiyuan-jiayan
SCRATCH = r'C:\Users\Administrator\Desktop\grok-build\scratch_gl'
BASEDIR = os.path.join(SCRATCH, 'baselines')
PRE = os.path.join(ROOT, 'compat_prelude.h')
D = ['-D', '__STDC_VERSION__=199901L', '-D', '_WIN32=1', '-D', 'REG_STARTEND=1']

TREES = {
 'lua': (r'C:\Users\Administrator\.openclaw\audit\luarun\lua', ['-D', '__GNUC__=5']),
 'zlib': (r'C:\Users\Administrator\.cargo\registry\src\mirrors.tuna.tsinghua.edu.cn-4dc01642fd091eda'
          r'\libz-sys-1.1.22\src\zlib', []),
}

# git quick 子集 (与 git563.py --quick 同口径): 基线里最有价值的一块 —— 真实世界代码
GIT = r'C:\Users\Administrator\Desktop\git-2.45.2'
GIT_QUICK = [
 'builtin/commit.c', 'builtin/diff.c', 'builtin/add.c', 'diff.c', 'commit.c',
 'reflog-walk.c', 'trace2/tr2_dst.c', 'compat/regex/regcomp.c', 'compat/mingw.c',
 'compat/nedmalloc/nedmalloc.c', 'xdiff/xpatience.c', 'sha1dc/sha1.c',
 'reftable/block.c', 'grep.c', 'read-cache.c', 'preload-index.c', 'dir.c',
 'fsmonitor--daemon.c', 'builtin/pack-objects.c', 'diffcore-rename.c',
]
GIT_EXTRA = ['--lenient', '-D', '__GNUC__=5']
# C113 (2026-09-18 第 106 站): 环境变量 BASELINE_GIT_ALL=1 ⟹ 收录 **git 树全部 .c** (561 件, ~13 min),
# 而不是 20 件 quick 子集。动机: 产物基线已证明是「rc 全绿而机器码已错」的唯一可见通道
#   (第 104 站实测: C110 编 lua/lparser.c 时 L270 赋值右侧整段不发射, 而 rc=0)—— 覆盖面越广越好。
GIT_ALL = os.environ.get('BASELINE_GIT_ALL') == '1'


def build(qcc):
    tag = hashlib.sha256(io.open(qcc, 'rb').read()).hexdigest()[:8].upper()
    out = {}
    tmp = os.path.join(SCRATCH, '_bl')
    os.makedirs(tmp, exist_ok=True)
    os.makedirs(BASEDIR, exist_ok=True)
    for name, (tree, extra) in TREES.items():
        if not os.path.isdir(tree):
            print('  [skip] %s 不存在: %s' % (name, tree))
            continue
        for f in sorted(x for x in os.listdir(tree) if x.endswith('.c') and not x.startswith('_')):
            src = os.path.join(tree, f)
            o = os.path.join(tmp, '%s_%s.o' % (name, f[:-2]))
            r = subprocess.run([qcc] + D + extra + ['-c', src, '-o', o, '-I', PRE, '-I', tree],
                               capture_output=True, timeout=900)
            key = '%s/%s' % (name, f)
            if r.returncode != 0 or not os.path.exists(o):
                out[key] = 'FAIL'
                continue
            b = io.open(o, 'rb').read()
            out[key] = '%d:%s' % (len(b), hashlib.sha256(b).hexdigest()[:16])
            os.remove(o)
    # git 子集 (quick 20 件) 或 全量 (.c 递归, BASELINE_GIT_ALL=1)
    if GIT_ALL:
        todo = []
        for dp, dn, fns in os.walk(GIT):
            for f in fns:
                if f.endswith('.c'):
                    todo.append(os.path.relpath(os.path.join(dp, f), GIT).replace(os.sep, '/'))
        todo.sort()
    else:
        todo = GIT_QUICK
    for rel in todo:
        src = os.path.join(GIT, rel.replace('/', os.sep))
        if not os.path.exists(src):
            continue
        o = os.path.join(tmp, 'git_%s.o' % rel.replace('/', '_')[:-2])
        r = subprocess.run([qcc] + D + GIT_EXTRA + ['-c', src, '-o', o, '-I', PRE, '-I', GIT],
                           capture_output=True, timeout=900)
        key = 'git/%s' % rel
        if r.returncode != 0 or not os.path.exists(o):
            out[key] = 'FAIL'
            continue
        b = io.open(o, 'rb').read()
        out[key] = '%d:%s' % (len(b), hashlib.sha256(b).hexdigest()[:16])
        os.remove(o)
    return tag, out


def main():
    if len(sys.argv) < 3:
        raise SystemExit(__doc__)
    mode, qcc = sys.argv[1], os.path.abspath(sys.argv[2])
    tag, cur = build(qcc)
    path = os.path.join(BASEDIR, 'objs_%s.json' % tag)
    if mode == 'save':
        io.open(path, 'w', encoding='utf-8').write(
            json.dumps(cur, indent=1, sort_keys=True, ensure_ascii=False))
        ok = sum(1 for v in cur.values() if v != 'FAIL')
        print('baseline saved: %s  (%d files, %d ok)' % (path, len(cur), ok))
    elif mode == 'diff':
        if not os.path.exists(path):
            print('无本 tag 基线 (%s) —— 先 save' % path)
            return
        old = json.loads(io.open(path, encoding='utf-8').read())
        keys = sorted(set(old) | set(cur))
        ch = [k for k in keys if old.get(k) != cur.get(k)]
        print('对照 %s: 共 %d, 变化 %d' % (os.path.basename(path), len(keys), len(ch)))
        for k in ch[:40]:
            print('  %-30s %s -> %s' % (k, old.get(k, '<new>'), cur.get(k, '<gone>')))
    else:
        raise SystemExit('mode: save|diff')


main()
