# -*- coding: utf-8 -*-
"""STATE-SURFACES-NEG —— 「全仓终态账/坐标闸」的**负向自测**（证明它会红，不是永远绿）。

为什么要有它（skeptic 第八轮）：
  `check_state_surfaces.py` 曾经有两处**死范围** ——
    ① `tracked()` 整块跳过 `_evidence/`，于是 `coord_scoped()` 里「本轮证据 md」那条分支永远不可达；
    ② `_ATLAS_ENTRY.md` 虽被跟踪却从不做坐标判定。
  光看「闸 PASS」无法区分「真的全对」与「根本没扫」，所以这里用**种子夹具**证明：
  一旦状态面上出现 (a) 过期的台账计数 或 (b) 过期的 8 位坐标，闸必须 rc=1 并指名该文件。

做法：把**已发布的那支闸**连同 `_GOAL_STATE.json` 复制进临时树，在临时树里种假数据、`git init`，
再用同一个解释器跑它 —— 测的是 shipped 代码，不是复写一遍逻辑。

用法:
  python scripts/test_state_surfaces_negative.py            # 三个场景（两红一绿）
  python scripts/test_state_surfaces_negative.py --old <旧版脚本>   # 额外对照：旧版对同一夹具应**漏报**
退出: 0 = 全部断言成立；1 = 有断言不成立
"""
import argparse
import io
import os
import shutil
import subprocess
import sys
import tempfile

sys.stdout.reconfigure(encoding='utf-8', errors='replace')
REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
LEDGER = os.path.join(os.path.dirname(REPO), 'grok-build')   # 闸与状态源都在台账仓
GATE = os.path.join(LEDGER, 'scripts', 'check_state_surfaces.py')
STATE = os.path.join(LEDGER, '_GOAL_STATE.json')
EV = '_evidence/20261027_orders_closeout'


def build_tree(root, stale_count=False, stale_coord=False):
    """在 root 里搭一个最小仓库：闸 + 状态 JSON + 状态面夹具。"""
    os.makedirs(os.path.join(root, 'scripts'))
    shutil.copy2(GATE, os.path.join(root, 'scripts', 'check_state_surfaces.py'))
    shutil.copy2(STATE, os.path.join(root, '_GOAL_STATE.json'))
    os.makedirs(os.path.join(root, EV))

    count_line = ('| 台账 | **159 = in_progress 0 / open 0 / blocked 0 / fixed 120 / settled 39** |'
                  if stale_count else
                  '| 台账 | 见 `_GOAL_STATE.json` |')
    coord_line = ('| 编译器仓 HEAD | `deadbeef` |' if stale_coord else
                  '| 编译器仓 HEAD | 见 `_GOAL_STATE.json` |')
    io.open(os.path.join(root, EV, 'README.md'), 'w', encoding='utf-8', newline='\n').write(
        '# 夹具 · 本轮证据说明\n\n' + count_line + '\n' + coord_line + '\n')
    io.open(os.path.join(root, '_ATLAS_ENTRY.md'), 'w', encoding='utf-8', newline='\n').write(
        '# 夹具 · 首读页\n\n' + ('| 编译器仓 HEAD | `cafebabe` |\n' if stale_coord
                                else '| 编译器仓 HEAD | 见 `_GOAL_STATE.json` |\n'))
    io.open(os.path.join(root, 'FINAL_RESPONSE.md'), 'w', encoding='utf-8', newline='\n').write(
        '# 夹具 · 根指针\n\n见 `_GOAL_STATE.json`\n')
    subprocess.run(['git', 'init', '-q'], cwd=root, check=True,
                   capture_output=True)
    subprocess.run(['git', 'add', '-A'], cwd=root, check=True, capture_output=True)


def run_gate(root, script=None):
    sp = script or os.path.join(root, 'scripts', 'check_state_surfaces.py')
    # 旧版脚本可能不在树内：把它临时放进树的 scripts/ 下再跑（其 REPO 仍解析为树根）
    if script:
        dst = os.path.join(root, 'scripts', 'check_state_surfaces.py')
        shutil.copy2(script, dst)
        sp = dst
    p = subprocess.run([sys.executable, '-X', 'utf8', sp], cwd=root,
                       capture_output=True, text=True, encoding='utf-8', errors='replace')
    return p.returncode, ((p.stdout or '') + (p.stderr or '')).strip()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--old', default=None, help='旧版闸脚本路径（用于证明它曾漏报）')
    a = ap.parse_args()
    fails = []

    def case(name, stale_count, stale_coord, want_rc, want_marks):
        root = tempfile.mkdtemp(prefix='qy_surf_')
        try:
            build_tree(root, stale_count, stale_coord)
            rc, out = run_gate(root)
            ok = (rc == want_rc) and all(m in out for m in want_marks)
            print('[%s] %s rc=%d（期望 %d）%s' % ('PASS' if ok else 'FAIL', name, rc, want_rc,
                                                 '' if ok else ' | ' + out.replace('\n', ' / ')[:220]))
            if not ok:
                fails.append(name)
            return rc, out
        finally:
            shutil.rmtree(root, ignore_errors=True)

    case('干净夹具 → 应当绿', False, False, 0, ('PASS',))
    case('种过期计数 → 应当红', True, False, 1, ('FAIL', 'README.md'))
    case('种过期坐标 → 应当红', False, True, 1, ('FAIL', '编译器仓'))

    if a.old:
        root = tempfile.mkdtemp(prefix='qy_surf_old_')
        try:
            build_tree(root, stale_count=True, stale_coord=True)
            rc, out = run_gate(root, script=a.old)
            # 旧版对同一夹具应**漏报**（rc=0）——这正是本轮修掉的那处死范围。
            print('[%s] 旧版对同一夹具：rc=%d（期望 0 = 漏报，证明修复前真的是死范围）'
                  % ('PASS' if rc == 0 else 'FAIL', rc))
            if rc != 0:
                fails.append('old-hole')
        finally:
            shutil.rmtree(root, ignore_errors=True)

    print('== STATE-SURFACES-NEG: %s ==' % ('PASS' if not fails else 'FAIL -> ' + ','.join(fails)))
    return 0 if not fails else 1


if __name__ == '__main__':
    sys.exit(main())
