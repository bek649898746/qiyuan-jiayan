#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""check_gate_env —— **闸的环境前置** 判据 (AUDIT-P2-4, 2026-10-21 启元审计递刀)。

## 缺陷（递刀原文）
隔离 worktree / 干净克隆里 **没有 qcc_zh_v*.exe** 时，闸②⑤⑥⑦ **不是 SKIP 而是假红**：
检查器 各自 默认 `qcc_zh_v1.exe`，`subprocess.run` 直接抛 `FileNotFoundError` ⟹ rc≠0 ⟹
`run_tests_zh.py` 把「**没装环境**」判成「**代码坏了**」。假红比不跑更坏：它教人忽略闸。

## 判据（三条，双向）
1. **整体档**：无编译器的树里跑 `run_tests_zh.py` ⟹ rc=**2**（环境类，区别于 FAIL=1）
   + stdout 含 `[SKIP] 环境未就绪` + **不得**出现 `FileNotFoundError`。
2. **单闸档**：七个闸检查器 各自 在缺件的树里 独立跑 ⇹ **不得 rc=1**（假红），且 stdout 必须含 `[SKIP]` 标记；
   已迁移到新口径的检查器（`MUST_RC2`）必须 **rc=2**（环境未就绪，不判绿也不判红）。
   由来（2026-10-24 判据方打回）：原来「缺件 rc=0」会让「环境没装」冒充「闸已过」。
   真树 + 真编译器 ⟹ 单锚跑 rc=0 **且输出不得含 `[SKIP]`**；闸⑥ 真跑 rc=0 且无 `[SKIP]`。

用法: python scripts/check_gate_env.py
退出码: 0 = 全过 / 1 = 有用例未过
"""
import os
import shutil
import subprocess
import sys
import tempfile

sys.stdout.reconfigure(encoding='utf-8', errors='replace')
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TMP = os.path.join(tempfile.gettempdir(), '_gate_env_chk')
SK = '[SKIP]'

CHECKERS = [
    'check_c_mode_smoke.py',      # 闸②
    'check_neg_anchors.py',       # 闸③
    'check_dwarf289.py',          # 闸④
    'check_jyld_link_smoke.py',   # 闸⑤
    'check_drectve248.py',        # 闸⑥
    'check_r630_lvm.py',          # 闸⑦
    'check_ir_cap_neg.py',        # 闸⑧
]

fails = []


def clean_env():
    e = dict(os.environ)
    e.pop('QCC_TEST_BIN', None)
    return e


def run(cmd, cwd=None, env=None, timeout=300):
    return subprocess.run(cmd, cwd=cwd, env=env, capture_output=True, timeout=timeout)


def out_of(r):
    return (r.stdout + r.stderr).decode('utf-8', 'replace')


def check(cond, msg):
    if cond:
        print('  ✅ ' + msg)
    else:
        print('  ❌ ' + msg)
        fails.append(msg)


# ---------- 用例 1: 整轮 无编译器 ⟹ [SKIP] + rc=2 ----------
print('[1] 整轮档: 无编译器的树')
d1 = os.path.join(TMP, 'empty_tree', 'scripts')
shutil.rmtree(os.path.join(TMP, 'empty_tree'), ignore_errors=True)
os.makedirs(d1)
shutil.copy2(os.path.join(ROOT, 'scripts', 'run_tests_zh.py'), d1)
r = run([sys.executable, os.path.join(d1, 'run_tests_zh.py'), '--only', 'scripts/feat_zh_2phase30_ok.c'],
        cwd=os.path.join(TMP, 'empty_tree'), env=clean_env(), timeout=120)
o = out_of(r)
print('    rc=%d' % r.returncode)
check(r.returncode == 2, 'rc == 2 (环境类, 与 FAIL=1 可区分)')
check(SK + ' 环境未就绪' in o, '输出含 `[SKIP] 环境未就绪`')
check('FileNotFoundError' not in o, '不再出现 FileNotFoundError')

# ---------- 用例 2: 单个检查器 缺件 ⟹ [SKIP] + rc=0 ----------
print('[2] 单闸档: 七个检查器 各自 缺件')
d2root = os.path.join(TMP, 'no_qcc')
d2 = os.path.join(d2root, 'scripts')
shutil.rmtree(d2root, ignore_errors=True)
os.makedirs(d2)
MUST_RC2 = {'check_jyld_link_smoke.py'}   # 2026-10-24: 已迁移到 rc=2 口径（环境未就绪 ≠ 绿）
for c in CHECKERS:
    shutil.copy2(os.path.join(ROOT, 'scripts', c), d2)
    r = run([sys.executable, os.path.join(d2, c)], cwd=d2root, env=clean_env(), timeout=180)
    o = out_of(r)
    rc_ok = (r.returncode == 2) if c in MUST_RC2 else (r.returncode in (0, 2))
    ok = (rc_ok and SK in o and 'FileNotFoundError' not in o)
    check(ok, '%-26s rc=%d %s' % (c, r.returncode, (SK + ' 已标注') if SK in o else '**无 SKIP 标记**'))
    if not ok:
        print('      ' + '\n      '.join([x for x in o.strip().split('\n') if x.strip()][-4:]))

# ---------- 用例 3: 就绪档 —— 反向校验（不许一律 SKIP） ----------
print('[3] 就绪档: 真树 + 真编译器 (反向, 防「一律 SKIP」假绿)')
r = run([sys.executable, os.path.join(ROOT, 'scripts', 'run_tests_zh.py'), '--only',
         'scripts/feat_zh_2phase30_ok.c', '--parallel', '1'], cwd=ROOT, timeout=300)
o = out_of(r)
print('    --only rc=%d' % r.returncode)
check(r.returncode == 0, '单锚 rc == 0 (编译器/锚 正常时 不 SKIP)')
check(SK not in o, '输出不含 `[SKIP]`')

for c in ('check_drectve248.py', 'check_c_mode_smoke.py'):
    r = run([sys.executable, os.path.join(ROOT, 'scripts', c)], cwd=ROOT, timeout=900)
    o = out_of(r)
    print('    %s rc=%d' % (c, r.returncode))
    check(r.returncode == 0, '%s 真跑 rc == 0' % c)
    check(SK not in o, '%s 输出不含 `[SKIP]`' % c)

print('')
if fails:
    print('[check_gate_env] ❌ %d 条 未通过' % len(fails))
    for f in fails:
        print('  - ' + f)
    sys.exit(1)
print('[check_gate_env] ✅ 全过 (缺件 ⟹ 可区分 SKIP · 就绪 ⟹ 真跑, 双向成立)')
sys.exit(0)
