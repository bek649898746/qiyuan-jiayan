# -*- coding: utf-8 -*-
"""git_base_rebuild —— 固化「git 重建基准编译器」对拍（批次3 T3-5）。

为什么需要它（`_LONG_MARCH_V2` 批次3 T3-5 原文）:
  「**固化「git 重建基准」对拍**：`git archive f6847c4f` + gcc 重建 `qcc_0827.exe` ⟹
   每次语料回归带基准 —— 否则退步不可见（本次基准曾丢失）」

做法（依据 `seed/_BOOTSTRAP.md` §「种子丢失时的重建（才需要 gcc）」）:
    gcc -O2 -Wall -Werror -Wno-misleading-indentation qcc_x86.c -o stage1.exe
    stage1.exe qcc_x86.c -o new.exe      # 一步漂移（gcc 版与自宿主版布局差一次）
    new.exe qcc_x86.c -o v2.exe          # == 固定点（收敛），此后不再需要 gcc
  ⟹ 三代的 SHA256[:8] 全部记录；`new == v2` 即证明**该提交的固定点**成立 ⟹ 可作语料回归基准。

用法:
    python scripts/git_base_rebuild.py                 # 默认 f6847c4f（T3-5 点名）
    python scripts/git_base_rebuild.py <commit>        # 任意提交
输出:
    1) 三代哈希 + 收敛判定（stdout）
    2) 基准 exe 落在 <scratch>/base_<commit>/（不污染工作区）
    3) 结果写 <scratch>/git_base_rebuild.log
"""
import hashlib
import io
import os
import shutil
import subprocess
import sys
import tempfile

JJ = r'C:\Users\Administrator\Desktop\qiyuan-jiayan'
COMMIT = sys.argv[1] if len(sys.argv) > 1 else 'f6847c4f'
WORK = os.path.join(tempfile.gettempdir(), 'git_base_%s' % COMMIT[:8])
LOG = os.path.join(tempfile.gettempdir(), 'git_base_rebuild.log')
steps = []


def L(s=''):
    print(s)
    steps.append(s)


def sha8(p):
    h = hashlib.sha256()
    h.update(open(p, 'rb').read())
    return h.hexdigest()[:8].upper()


def run(a, **kw):
    return subprocess.run(a, capture_output=True, **kw)


L('=== git 重建基准编译器：commit=%s ===' % COMMIT)
if os.path.isdir(WORK):
    shutil.rmtree(WORK, ignore_errors=True)
os.makedirs(WORK, exist_ok=True)

# ---- 1. git archive 取该提交的工作树
tar = os.path.join(WORK, 'src.tar')
r = run(['git', 'archive', '--format=tar', '-o', tar, COMMIT], cwd=JJ)
L('① git archive rc=%d  tar=%d B' % (r.returncode, os.path.getsize(tar) if os.path.exists(tar) else -1))
if r.returncode != 0:
    L('   stderr: ' + (r.stderr or b'').decode('utf-8', 'replace')[:200])
    raise SystemExit(1)

# ---- 2. 解包
r = run(['tar', '-xf', tar], cwd=WORK)
L('② tar -xf rc=%d' % r.returncode)

# ---- 3. 找到 qcc_x86.c（该提交里种子 C 源的位置）
cand = []
for root, dirs, files in os.walk(WORK):
    for f in files:
        if f in ('qcc_x86.c', 'qcc_zh.c'):
            cand.append(os.path.join(root, f))
L('③ 找到 C 源: %s' % [os.path.relpath(c, WORK) for c in cand][:4])
src = None
for c in cand:
    if os.path.basename(c) == 'qcc_x86.c':
        src = c
        break
src = src or (cand[0] if cand else None)
if not src:
    L('**未找到 qcc_x86.c（该提交布局不同）** ⟹ 记录并退出')
    raise SystemExit(2)

# ---- 4. gcc 编 stage1
stage1 = os.path.join(WORK, 'stage1.exe')
r = run(['gcc', '-O2', '-Wall', '-Werror', '-Wno-misleading-indentation', src, '-o', stage1],
        cwd=os.path.dirname(src))
L('④ gcc stage1 rc=%d  %s' % (r.returncode, ('sha=' + sha8(stage1)) if os.path.exists(stage1) else
                              (r.stderr or b'').decode('utf-8', 'replace')[:200]))
if r.returncode != 0:
    raise SystemExit(3)

# ---- 5. 自举两代
outs = []
for tag, cc in (('new', stage1), ('v2', None)):
    cc = cc or outs[-1]
    o = os.path.join(WORK, tag + '.exe')
    r = run([cc, src, '-o', o], cwd=os.path.dirname(src), timeout=900)
    ok = os.path.exists(o)
    L('⑤ %-4s <- %-10s rc=%-3d %s' % (tag, os.path.basename(cc), r.returncode,
                                      ('sha=' + sha8(o)) if ok else '(未产出)'))
    if not ok:
        raise SystemExit(4)
    outs.append(o)

# ---- 6. 收敛判定 + 落盘
L()
L('=== 结论 ===')
L('gcc stage1 = %s' % sha8(stage1))
L('new        = %s' % sha8(outs[0]))
L('v2         = %s' % sha8(outs[1]))
L('**收敛（new == v2）**: %s' % ('是 ⟹ 该提交固定点 = ' + sha8(outs[1]) if sha8(outs[0]) == sha8(outs[1])
                                 else '否 ⟹ 继续多跑一代'))
base = os.path.join(WORK, 'base_%s.exe' % COMMIT[:8])
shutil.copy(outs[1], base)
L('基准 exe: %s' % base)
io.open(LOG, 'w', encoding='utf-8').write('\n'.join(steps) + '\n')
L('[存] %s' % LOG)
