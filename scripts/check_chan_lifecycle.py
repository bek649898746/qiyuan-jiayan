# -*- coding: utf-8 -*-
"""check_chan_lifecycle — 通道句柄生命周期闭环（ORD-20261102-84）。

## 判据（全部 3 次运行一致才算过）

  A 失效句柄：释放后（即使槽被新通道复用）再用 → exit 8 + stderr A5008，
    stdout 必须为空（绝不静默写进另一通道）
  B 伪造文件级槽：`整 h = 0; 释放通道(h);` → A5008，不得静默清掉文件级通道
  C 重复释放 → A5008
  D 伪造裸槽号 32 / 代际 0 句柄 64 → A5008
  E 10000 次（新建→送→收→释放）→ exit 0 + `REUSE10K OK`，不耗尽/不串台
  F 泄漏报告：存活句柄必须出现 `[LEAK] 通道句柄`；全部释放后 `[LEAK] 无泄漏`；
    顺序必须是「先泄漏行、后无泄漏」，且 3 次输出逐字节一致
  G 结构化并发块内释放本块新建句柄 → 编译期 A6093
  H 块外创建/块内使用/块外释放 → exit 0 + `SCOPE OK`
  I `释放通道` 文件级通道名 → 编译期拒绝（A1001）
  J 既有回归：`feat_zh_struct_concur25.c`（嵌套结构化块 + join 计数）、
    `feat_zh_structconc53_neg.c`（A6078）、`feat_chan_mix_zh.c` / `mix16` 不变

用法: python scripts/check_chan_lifecycle.py [--cc qcc_zh_v1.exe]
退出码：0 = 全绿；1 = 有红。
"""
import argparse
import io
import os
import subprocess
import sys
import sys as _sys_tmp
_sys_tmp.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qcc_tmp import tmp_dir  # noqa: E402  A1

sys.stdout.reconfigure(encoding='utf-8', errors='replace')

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
WORK = tmp_dir('scratch_chan_lifecycle')
RUNS = 3

SPECIAL = {
    # 名: (期望 exit, stdout 必含, stderr 必含, stdout 必空)
    'feat_chan_life_stale_abort_zh.c': (8, None, 'A5008', True),
    'feat_chan_life_forged_abort_zh.c': (8, None, 'A5008', True),
    'feat_chan_life_double_abort_zh.c': (8, None, 'A5008', True),
    'feat_chan_life_rawslot_abort_zh.c': (8, None, 'A5008', True),
    'feat_chan_life_gen0_abort_zh.c': (8, None, 'A5008', True),
    'feat_chan_life_reuse10k_ok_zh.c': (0, 'REUSE10K OK', None, False),
    'feat_chan_scope_release_ok_zh.c': (0, 'SCOPE OK', None, False),
    'feat_chan_scope_assign_ok_zh.c': (0, 'SCOPE ASSIGN OK', None, False),
    'feat_chan_scope_reassign_ok_zh.c': (0, 'REASSIGN OK', None, False),
    'feat_chan_scope_decl_outer_ok_zh.c': (0, 'DECL OUTER OK', None, False),
    'feat_chan_scope_multi_decl_outer_ok_zh.c': (0, 'MULTI OUTER OK', None, False),
    'feat_chan_pass_zh.c': (0, 'MTPASS OK', None, False),
    'feat_chan_handle_zh.c': (0, 'HANDLE 42', None, False),
    'feat_zh_struct_concur25.c': (0, '#25', None, False),
    'feat_chan_mix_zh.c': (0, 'Q 5 C1 20 C0 10', None, False),
    'feat_chan_mix16_zh.c': (0, 'SIXTEEN OK', None, False),
}

# 失效句柄：七个通道操作点逐一验证（判据 1 明列 送/背压送/收/就绪/就绪送/关闭通道/通道已关闭/释放通道）
STALE_OPS = {
    'send': '送 h, 99;',
    'bp_send': '背压 送 h, 99;',
    'recv': '整 v = 0;\n    收 h, v;',
    'ready': '整 r = 就绪(h);',
    'ready_send': '整 r = 就绪送(h);',
    'close': '关闭通道 h;',
    'closed': '整 r = 通道已关闭(h);',
    'release': '释放通道(h);',
}
STALE_TPL = '''# 包含 <stdio.h>
函数 整 主(空){
    整 h = 通道新建(2);
    释放通道(h);
    整 b = 通道新建(2); /* 同槽复用：旧句柄绝不能因此复活 */
    %s
    格式化 "SHOULD NOT REACH\\n", 0;
    返 0;
}
'''

# 槽复用后新通道必须仍能正常收发（与「旧句柄被拒」分处两个进程，互不干扰）
REUSE_SRC = '''# 包含 <stdio.h>
函数 整 主(空){
    整 a = 通道新建(2);
    释放通道(a);
    整 b = 通道新建(2);
    送 b, 7;
    整 x = 0;
    收 b, x;
    释放通道(b);
    格式化 "REUSE %d\\n", x;
    返 0;
}
'''
COMPILE_FAIL = {
    'feat_chan_scope_release_neg_zh.c': 'A6093',
    'feat_chan_scope_assign_neg_zh.c': 'A6093',
    'feat_chan_scope_alias_neg_zh.c': 'A6093',
    'feat_chan_scope_fnret_neg_zh.c': 'A6093',
    'feat_chan_scope_nested_neg_zh.c': 'A6093',
    'feat_chan_scope_decl_alias_neg_zh.c': 'A6093',
    'feat_chan_scope_decl_fnret_neg_zh.c': 'A6093',
    'feat_chan_scope_multi_decl_neg_zh.c': 'A6093',
    'feat_chan_scope_multi_decl_fnret_neg_zh.c': 'A6093',
    'feat_zh_structconc53_neg.c': 'A6078',
}

LEAK_SRC = '''# 包含 <stdio.h>
函数 整 主(空){
    整 a = 通道新建(2);
    整 b = 通道新建(2);
    释放通道(a);
    泄漏报告;
    释放通道(b);
    泄漏报告;
    返 0;
}
'''

REL_NAME_SRC = '''# 包含 <stdio.h>
通道 c, 4;
函数 整 主(空){
    释放通道(c);
    返 0;
}
'''


def build(cc, name, src_path=None, src_text=None):
    os.makedirs(WORK, exist_ok=True)
    if src_text is not None:
        src_path = os.path.join(WORK, name + '.jy')
        io.open(src_path, 'w', encoding='utf-8', newline='\n').write(src_text)
    exe = os.path.join(WORK, name + '.exe')
    try:
        os.remove(exe)
    except OSError:
        pass
    r = subprocess.run([cc, src_path, '-o', exe], cwd=REPO, capture_output=True, text=True,
                       encoding='utf-8', errors='replace', timeout=180)
    return r, exe


def run3(exe):
    seen = []
    for _ in range(RUNS):
        r = subprocess.run([exe], cwd=WORK, capture_output=True, text=True,
                           encoding='utf-8', errors='replace', timeout=120)
        seen.append((r.returncode, r.stdout or '', r.stderr or ''))
    return seen


def check_runtime(cc, name, expect_rc, out_sub, err_sub, out_empty):
    r, exe = build(cc, name, os.path.join(REPO, 'scripts', name))
    if r.returncode != 0:
        return 'FAIL', '编译失败 rc=%d %s' % (r.returncode, ((r.stdout or '') + (r.stderr or '')).strip()[:130])
    seen = run3(exe)
    prob = []
    first = seen[0]
    if first[0] != expect_rc:
        prob.append('exit 期望%d 实得%d' % (expect_rc, first[0]))
    if out_sub and out_sub not in first[1]:
        prob.append('stdout 缺 %r（实得 %r）' % (out_sub, first[1].strip()[:80]))
    if err_sub and err_sub not in first[2]:
        prob.append('stderr 缺 %r（实得 %r）' % (err_sub, first[2].strip()[:120]))
    if out_empty and first[1].strip():
        prob.append('stdout 应空（实得 %r）' % first[1].strip()[:80])
    if any(s != first for s in seen[1:]):
        prob.append('3 次运行不一致')
    return ('PASS' if not prob else 'FAIL'), ('; '.join(prob) if prob else
            'rc=%d out=%r err=%r' % (first[0], first[1].strip()[:50], first[2].strip()[:80]))


def check_compile_fail(cc, name, code, src_text=None):
    r, _ = build(cc, name, None if src_text is not None else os.path.join(REPO, 'scripts', name),
                 src_text=src_text)
    both = (r.stdout or '') + (r.stderr or '')
    if r.returncode == 0:
        return 'FAIL', '应编译失败但 rc=0'
    if code and code not in both:
        return 'FAIL', '诊断缺 %s（实得 %r）' % (code, both.strip()[:160])
    return 'PASS', 'compile_fail(%s) rc=%d' % (code, r.returncode)


def check_stale_ops(cc):
    legs = []
    for op, body in STALE_OPS.items():
        r, exe = build(cc, 'stale_op_' + op, src_text=STALE_TPL % body)
        if r.returncode != 0:
            legs.append(('stale_op_' + op, ('FAIL', '编译失败 rc=%d %s' % (
                r.returncode, ((r.stdout or '') + (r.stderr or '')).strip()[:130]))))
            continue
        seen = run3(exe)
        first = seen[0]
        prob = []
        if first[0] != 8:
            prob.append('exit 期望8 实得%d' % first[0])
        if 'A5008' not in first[2]:
            prob.append('stderr 缺 A5008（实得 %r）' % first[2].strip()[:120])
        if first[1].strip():
            prob.append('stdout 应空（实得 %r）' % first[1].strip()[:80])
        if any(s != first for s in seen[1:]):
            prob.append('3 次运行不一致')
        legs.append(('stale_op_' + op, ('PASS' if not prob else 'FAIL',
                                        '; '.join(prob) if prob else 'rc=8 A5008')))
    return legs


def check_reuse(cc):
    r, exe = build(cc, 'reuse_after_release', src_text=REUSE_SRC)
    if r.returncode != 0:
        return 'FAIL', '编译失败 rc=%d %s' % (r.returncode, ((r.stdout or '') + (r.stderr or '')).strip()[:130])
    seen = run3(exe)
    first = seen[0]
    prob = []
    if first[0] != 0:
        prob.append('exit 期望0 实得%d' % first[0])
    if 'REUSE 7' not in first[1]:
        prob.append('stdout 缺 REUSE 7（实得 %r）' % first[1].strip()[:80])
    if any(s != first for s in seen[1:]):
        prob.append('3 次运行不一致')
    return ('PASS' if not prob else 'FAIL'), ('; '.join(prob) if prob else 'rc=0 out=%r' % first[1].strip()[:40])


def check_leak(cc):
    r, exe = build(cc, 'leak_visible', src_text=LEAK_SRC)
    if r.returncode != 0:
        return 'FAIL', '编译失败 rc=%d %s' % (r.returncode, ((r.stdout or '') + (r.stderr or '')).strip()[:130])
    seen = run3(exe)
    prob = []
    first = seen[0]
    lines = [ln for ln in first[2].splitlines() if ln.strip()]
    if first[0] != 0:
        prob.append('exit 期望0 实得%d' % first[0])
    leak_idx = [i for i, ln in enumerate(lines) if '[LEAK] 通道句柄' in ln]
    none_idx = [i for i, ln in enumerate(lines) if '[LEAK] 无泄漏' in ln]
    if not leak_idx:
        prob.append('缺 [LEAK] 通道句柄 行（实得 %r）' % lines)
    if not none_idx:
        prob.append('缺 [LEAK] 无泄漏 行（实得 %r）' % lines)
    if leak_idx and none_idx and not (max(leak_idx) < min(none_idx)):
        prob.append('顺序错：无泄漏 出现在 存活句柄 之前')
    if first[2].count('[LEAK] 无泄漏') != 1:
        prob.append('无泄漏 行数 != 1（实得 %d）' % first[2].count('[LEAK] 无泄漏'))
    if any(s != first for s in seen[1:]):
        prob.append('3 次运行不一致')
    return ('PASS' if not prob else 'FAIL'), ('; '.join(prob) if prob else
            'rc=0 stderr=%r' % first[2].strip().replace('\n', ' | ')[:120])


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cc', default='qcc_zh_v1.exe')
    a = ap.parse_args()
    cc = a.cc if os.path.isabs(a.cc) else os.path.join(REPO, a.cc)
    print('# check_chan_lifecycle（ORD-20261102-84）  编译器: %s  每腿 %d 次' % (a.cc, RUNS))
    print()
    legs = []
    for name, (rc, out_sub, err_sub, out_empty) in SPECIAL.items():
        legs.append((name, check_runtime(cc, name, rc, out_sub, err_sub, out_empty)))
    legs.extend(check_stale_ops(cc))
    legs.append(('reuse_after_release', check_reuse(cc)))
    legs.append(('leak_visible', check_leak(cc)))
    for name, code in COMPILE_FAIL.items():
        legs.append((name, check_compile_fail(cc, name, code)))
    legs.append(('release_file_chan_name', check_compile_fail(cc, 'release_file_chan_name', 'A1001',
                                                              src_text=REL_NAME_SRC)))

    npass = sum(1 for _, (st, _) in legs if st == 'PASS')
    for name, (st, det) in legs:
        print('%-42s %-4s %s' % (name, st, det))
    print()
    print('PASS=%d FAIL=%d / %d' % (npass, len(legs) - npass, len(legs)))
    return 0 if npass == len(legs) else 1


if __name__ == '__main__':
    sys.exit(main())
