#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""check_ptr_dot_method —— 指针用点号（成员访问 / 扩展方法调用）必须 A9001（ORD-20261027-22 · D94）

病灶（修前）：D72 的「指针用点号」检查打在**码生成**的成员访问分支；而扩展方法调用 `q.求和()`
  在**后缀链**就被重写成普通函数调用 ⟹ 根本不进入那个分支 ⟹ rc=0 零诊断 + 静默读垃圾
  （实测打印 822406808，应 15）。修法：判据前移到 `后缀链` 的 `.` 位（成员/方法 两个位共用一处）。

判据：**必须拒绝的**（指针 + `.`）逐例 rc≠0 且含 A9001；**必须放行且结果正确**的（结构体值 `.`、
  指针 `->`、`(*p).`）逐例 rc=0 且 stdout 与期望一致。全部驱动真实编译器产物。

用法：python scripts/check_ptr_dot_method.py --cc qcc_zh_v1.exe
"""
from qcc_tmp import tmp_dir, tmp_root
import argparse
import io
import os
import re
import shutil
import subprocess
import sys
import tempfile

sys.stdout.reconfigure(encoding='utf-8', errors='replace')
REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

HEAD = '''# 包含 <stdio.h>
构 点 { 整 x; 整 y; };
函数 整 点_和(构 点 *a) { 返 a->x + a->y; }
扩展 构 点 { 求和 = 点_和 };
'''

# (名字, 函数体, 期望 rc, 期望 stdout 片段/err 片段)
CASES = [
    ("ok_val_field",  HEAD + '函数 整 主(空) { 构 点 t; t.x = 3; t.y = 4; 格式化 "%d\\n", t.x + t.y; 返 0; }\n', 0, '7'),
    ("ok_val_method", HEAD + '函数 整 主(空) { 构 点 t; t.x = 7; t.y = 8; 格式化 "%d\\n", t.求和(); 返 0; }\n', 0, '15'),
    ("ok_ptr_arrow",  HEAD + '函数 整 主(空) { 构 点 t; t.x = 7; t.y = 8; 构 点 *q = &t; 格式化 "%d %d\\n", q->x, q->求和(); 返 0; }\n', 0, '7 15'),
    ("ok_deref_dot",  HEAD + '函数 整 主(空) { 构 点 t; t.x = 5; t.y = 6; 构 点 *q = &t; 格式化 "%d\\n", (*q).x; 返 0; }\n', 0, '5'),
    ("neg_ptr_field", HEAD + '函数 整 主(空) { 构 点 t; t.x = 1; t.y = 2; 构 点 *q = &t; 格式化 "%d\\n", q.x; 返 0; }\n', 1, 'A9001'),
    ("neg_ptr_method", HEAD + '函数 整 主(空) { 构 点 t; t.x = 7; t.y = 8; 构 点 *q = &t; 格式化 "%d\\n", q.求和(); 返 0; }\n', 1, 'A9001'),
    ("neg_param_method", HEAD + '函数 整 探(构 点 *p) { 返 p.求和(); }\n函数 整 主(空) { 构 点 t; t.x = 1; t.y = 2; 格式化 "%d\\n", 探(&t); 返 0; }\n', 1, 'A9001'),
    ("neg_param_field", HEAD + '函数 整 探(构 点 *p) { 返 p.x; }\n函数 整 主(空) { 构 点 t; t.x = 1; t.y = 2; 格式化 "%d\\n", 探(&t); 返 0; }\n', 1, 'A9001'),
]


def run(cmd, timeout=300):
    p = subprocess.run(cmd, capture_output=True, text=True,
                       encoding='utf-8', errors='replace', timeout=timeout)
    return p.returncode, (p.stdout or '') + (p.stderr or '')


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cc', default='qcc_zh_v1.exe')
    ap.add_argument('--keep', action='store_true')
    a = ap.parse_args()

    cc = a.cc if os.path.isabs(a.cc) else os.path.join(REPO, a.cc)
    if not os.path.exists(cc):
        print('[FAIL] 找不到编译器: %s' % cc)
        return 2

    tmp = tmp_dir('d94')
    bad = []
    try:
        for name, src, want_rc, want in CASES:
            f = os.path.join(tmp, name + '.jy')
            exe = os.path.join(tmp, name + '.exe')
            io.open(f, 'w', encoding='utf-8', newline='\n').write(src)
            rc, out = run([cc, f, '-o', exe])
            if want_rc == 0:
                if rc != 0 or not os.path.exists(exe):
                    bad.append('%s: 期望通过，实得 rc=%d（%s）' % (name, rc, out.strip()[:120]))
                    continue
                rrc, rout = run([exe])
                if want not in rout:
                    bad.append('%s: stdout 期望含 %r，实得 %r' % (name, want, rout.strip()[:80]))
            else:
                if rc == 0:
                    got = run([exe])[1].strip() if os.path.exists(exe) else '<无产物>'
                    bad.append('%s: 期望拒绝（%s），实得 rc=0（静默放行，运行输出 %r）' % (name, want, got[:60]))
                elif want not in out:
                    bad.append('%s: rc=%d 但错误里没有 %s（%s）' % (name, rc, want, out.strip()[:120]))
        if bad:
            print('[FAIL] 指针点号判据未通过 %d 例' % len(bad))
            for b in bad[:8]:
                print('   - ' + b)
            return 1
        print('[PASS] 指针点号：%d 例（4 拒绝 + 4 放行）全部符合契约（编译器=%s）'
              % (len(CASES), os.path.basename(cc)))
        return 0
    finally:
        if not a.keep:
            shutil.rmtree(tmp, ignore_errors=True)


if __name__ == '__main__':
    sys.exit(main())
