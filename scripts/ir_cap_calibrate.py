# -*- coding: utf-8 -*-
"""R720b 容量校准器 —— 复算「CFG 前驱表 槽容 32」的实测依据（decisions.log 裁四）。

口径：用 现役引擎 + `QCC_DUMP=ir` 编译 **真实语料**，读 dump 的
      `-- IR CFG 前驱表 容量自查 … 槽容=C · 块数=N · 实测 最大入度=M · 溢出块=K --` 行。
判据：**溢出块 == 0** 且 **实测最大入度 <= 槽容**（任一块 溢出 ⟹ 槽容 不足，须 扩容/走 跃表）。

用法：
  python scripts/ir_cap_calibrate.py [引擎.exe] [git语料根目录] [文件1.c 文件2.c ...]
  默认引擎 = qcc_zh_v2.exe · 默认语料 = C:\\Users\\Administrator\\Desktop\\git-2.45.2
"""
import os, re, subprocess, sys, time

sys.stdout.reconfigure(encoding='utf-8', errors='replace')
QJ = r'C:\Users\Administrator\Desktop\qiyuan-jiayan'
CC = sys.argv[1] if len(sys.argv) > 1 else os.path.join(QJ, 'qcc_zh_v2.exe')
if not os.path.isabs(CC):
    CC = os.path.join(QJ, CC)          # Windows: 相对名 按 父进程 cwd 解析 ⟹ 显式 拼 工作区
GIT = sys.argv[2] if len(sys.argv) > 2 else r'C:\Users\Administrator\Desktop\git-2.45.2'
PRE = os.path.join(QJ, 'compat_prelude.h')
OUT = os.path.join(os.environ.get('TEMP', '.'), 'qiyuan-ir-knives', 'cap')
os.makedirs(OUT, exist_ok=True)

DEFAULT = ['convert.c', 'patch-delta.c', 'sequencer.c', 'builtin/stash.c',
           'config.c', 'apply.c', 'compat/mingw.c', 'sha1dc/sha1.c']
files = sys.argv[3:] or DEFAULT

# 与 git 全量闸 同口径（compile_git_par.ps1）
DARGS = ['-D', '__STDC_VERSION__=199901L', '-D', '__GNUC__=5', '-D', '_WIN32=1', '-D', 'REG_STARTEND=1']
INC = ['-I', PRE, '-I', os.path.join(GIT, 'compat', 'regex'), '-I', GIT]
env = dict(os.environ, QCC_DUMP='ir')

RX = re.compile(r'槽容=(\d+) · 块数=(\d+) · 实测 最大入度=(\d+) · 溢出块=(\d+)')
rows, worst, bad = [], 0, []
print('引擎 = %s' % CC)
print('语料 = %s' % GIT)
print()
for rel in files:
    src = os.path.join(GIT, rel.replace('/', os.sep))
    if not os.path.exists(src):
        print('%-22s ⚪ 源 缺失' % rel); continue
    dst = os.path.join(OUT, rel.replace('/', '_') + '.o')
    t0 = time.time()
    r = subprocess.run([CC] + DARGS + INC + ['-c', src, '-o', dst], capture_output=True, cwd=GIT, env=env)
    out = ((r.stdout or b'') + (r.stderr or b'')).decode('utf-8', 'replace')
    m = RX.search(out)
    dt = time.time() - t0
    if not m:
        print('%-22s rc=%d %.1fs  ⚪ 无 容量自查 行' % (rel, r.returncode, dt)); continue
    slot, nblk, maxin, ovf = (int(x) for x in m.groups())
    worst = max(worst, maxin)
    if ovf or maxin > slot:
        bad.append(rel)
    rows.append((rel, slot, nblk, maxin, ovf))
    print('%-22s rc=%d %5.1fs 槽容=%-3d 块数=%-5d 最大入度=%-3d 溢出块=%d' % (rel, r.returncode, dt, slot, nblk, maxin, ovf))
print()
print('实测 最大入度（本批 上界）= %d' % worst)
print('溢出/超容 文件 = %s' % (bad or '无'))
print('[%s] 槽容 校准 ⟹ %s' % ('PASS' if not bad else 'FAIL',
      '容量充足（%d <= 槽容）' % worst if not bad else '须 扩容 / 走 跃表'))
sys.exit(0 if not bad else 1)
