# -*- coding: utf-8 -*-
"""check_array_dim_undef — ARRAY-DIM-UNDEF 判据（2026-10-21 虾米 · A6111）

病灶：数组维度 引用了 **未定义 的 宏/标识符** 时，常量求值 失败 ⟹ 调用点 **静默 按 1 个元素** 分配
      （实测 `整 表[乙维];` + `#define 乙维 (未定义维度宏 + 3)` ⟹ `大小(表)` = 4）。
      后续 下标 越界 写，现场 离 病灶 很远 —— round19 为此 浪费 半天。

判据（三件，全绿才算 PASS）：
 ① 负锚 `feat_zh_array_dim_undef_neg.c` ⟹ **rc != 0** 且 stderr 含 **A6111**
    （修前：rc=0 + 静默 4 字节数组）
 ② 正锚 `feat_zh_array_dim_vla_ok.c`（运行期维度 VLA）⟹ rc=0 且运行输出 `VLA 1 5`
    （证明 A6111 没有误伤 VLA 路径）
 ③ 反向对照：同一份源把 未定义宏 换成 已定义宏 ⟹ rc=0（证明 拒绝 只由「未定义」导致）

用法: python scripts/check_array_dim_undef.py [--cc <exe>]
退出: 0 PASS / 1 FAIL
"""
import argparse
import os
import subprocess
import sys
import tempfile

sys.stdout.reconfigure(encoding='utf-8', errors='replace')
REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
os.chdir(REPO)

NEG = os.path.join('scripts', 'feat_zh_array_dim_undef_neg.c')
POS = os.path.join('scripts', 'feat_zh_array_dim_vla_ok.c')

CTRL = '''#define 乙维 (已定义维度宏 + 3)
#define 已定义维度宏 40
整 表[乙维];
整 主(空) { 返 (整)大小(表); }
'''


def run(cc, src, exe, extra=None):
    args = [cc] + (extra or []) + [src, '-o', exe]
    return subprocess.run(args, capture_output=True, text=True, encoding='utf-8', errors='replace')


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cc', default='qcc_zh_seed.exe')
    a = ap.parse_args()
    d = tempfile.mkdtemp(prefix='adim_')
    ok = True

    # ① 负锚：必须 响亮
    r = run(a.cc, NEG, os.path.join(d, 'neg.exe'))
    out = (r.stdout or '') + (r.stderr or '')
    good = (r.returncode != 0 and 'A6111' in out)
    print('① 负锚 rc=%d A6111=%s -> %s' % (r.returncode, 'A6111' in out, 'PASS' if good else 'FAIL'))
    if not good:
        print('   out: %s' % out.strip()[:300])
    ok &= good

    # ② 正锚：VLA 不误伤
    r = run(a.cc, POS, os.path.join(d, 'pos.exe'))
    out = (r.stdout or '') + (r.stderr or '')
    ro = subprocess.run([os.path.join(d, 'pos.exe')], capture_output=True, text=True,
                        encoding='utf-8', errors='replace') if r.returncode == 0 else None
    run_out = ((ro.stdout or '') + (ro.stderr or '')).strip() if ro else ''
    good = (r.returncode == 0 and ro is not None and ro.returncode == 0 and 'VLA 1 5' in run_out)
    print('② 正锚(VLA) rc=%d run=%r -> %s' % (r.returncode, run_out[:40], 'PASS' if good else 'FAIL'))
    if not good:
        print('   out: %s' % out.strip()[:300])
    ok &= good

    # ③ 反向对照：已定义宏 照常通过
    ctrl = os.path.join(d, 'ctrl.c')
    open(ctrl, 'w', encoding='utf-8').write(CTRL)
    r = run(a.cc, ctrl, os.path.join(d, 'ctrl.exe'))
    good = (r.returncode == 0)
    print('③ 对照(已定义宏) rc=%d -> %s' % (r.returncode, 'PASS' if good else 'FAIL'))
    if not good:
        print('   out: %s' % ((r.stdout or '') + (r.stderr or '')).strip()[:300])
    ok &= good

    print('=== ARRAY-DIM-UNDEF 判据: %s ===' % ('PASS' if ok else 'FAIL'))
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())
