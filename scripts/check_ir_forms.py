#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""check_ir_forms —— IR 形态族判据（一形态一最小探针；一族一条判据）。

判据（每族）：同一份探针分别以 **legacy** 与 **QCC_CODEGEN=ir** 编 `-c`：
  ① 两边 rc 都必须 0；
  ② 两边产物 sha256 **逐字节相同**；
  ③ 若 IR 侧失败，stderr **必须**含 `IR-BACKEND-UNIMPL`（响亮拒收）或 `[IR-REG]`（显式登记）
     —— 证明「没有静默回退 legacy」。
全部族通过 ⟹ exit 0；任一族不达 ⟹ exit 1，并打印族名（红判据：本文件就是形态清单的 ratchet）。

用法:
  python scripts/check_ir_forms.py [--cc qcc_zh_v2.exe] [--only member,ptradd]
退出码: 0 = 全族通过；1 = 有族不达；2 = 环境未就绪
"""
import argparse
import hashlib
import io
import os
import re
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qcc_tmp import tmp_dir  # noqa: E402

if hasattr(sys.stdout, 'reconfigure'):
    sys.stdout.reconfigure(encoding='utf-8', errors='replace')

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# 族名 → (探针, 说明)。族名与编译器 `IR形态名()` 的诊断口径对齐。
FORMS = [
    ('member',   'scripts/probe/ir_form_member.c',  '载:成员（结构体字段装载）'),
    ('member_read', 'scripts/probe/ir_form_member_read.c', '载:成员（非 printf 消费 — 真装/真存）'),
    ('memberstore', 'scripts/probe/ir_form_memberstore.c', '存:成员（`s.f = v` 帧位移 / `p->f = v` 槽 + 域偏移）'),
    ('arrowread', 'scripts/probe/ir_form_arrowfall_read.c', '载:成员（指针局部变量 `p->f`，含 1/2/8 字节宽）'),
    ('preinc', 'scripts/probe/ir_form_preinc.c', '前置自增（`++x`，型 26 的 L1 记录）'),
    ('comma', 'scripts/probe/ir_form_comma.c', '逗号表达式（`(a, b)`，型 2 的 L1 记录）'),
    ('ptrindex', 'scripts/probe/ir_form_ptrindex.c', '载:下标（指针变量基 `p[i]`，态 2）'),
    ('ucmp', 'scripts/probe/ir_form_ucmp.c', '整比的无符号性（`setb` vs `setl`）'),
    ('staticindex', 'scripts/probe/ir_form_staticindex.c', '载:下标（静态数组基 `ga[i]`，态 12）'),
    ('arrstore', 'scripts/probe/ir_form_arrstore.c', '存:下标（循环内 + 变量索引 + 二元右值）'),
    ('derefstore', 'scripts/probe/ir_form_derefstore.c', '存:解引用（`*p = v` 标量 4/8）'),
    ('layout', 'scripts/probe/ir_layout_probe.c', '布局：结构体局部 + 指向它的指针（P0 栈槽口径守卫）'),
    ('member_chain', 'scripts/probe/ir_form_member_chain.c', '载:成员链（`p->b.a.v` 三层嵌套，态 6）'),
    ('ptrchain', 'scripts/probe/ir_form_ptrchain.c', '载:成员链（`p->n->a`，**中间层是指针字段** ⟹ 逐级解引用；刀九十五）'),
    ('index',    'scripts/probe/ir_form_index.c',   '载:下标（基+索引缩放）'),
    ('ptradd',   'scripts/probe/ir_form_ptradd.c',  '指针加（p+n 元素缩放）'),
    ('call2',    'scripts/probe/ir_form_call2.c',   '调用:多实参'),
    ('div',      'scripts/probe/ir_form_div.c',     '二元:除/余（含 __qcc_div_check）'),
    ('addrof',   'scripts/probe/ir_form_addrof.c',  '取址:非局部（成员地址）'),
    ('addrof_ext', 'scripts/probe/ir_form_addrof_ext.c', '取址:非局部（局部成员 + 指针成员）'),
    ('trunc',    'scripts/probe/ir_form_trunc.c',   '截断/符号扩展'),
    ('f2i',      'scripts/probe/ir_form_f2i.c',     '浮转整'),
    ('zerocall', 'scripts/probe/ir_form_zerocall.c', '零实参调用（已修，回归守卫）'),
    ('cmp',      'scripts/probe/ir_form_cmp.c',     '整比六种条件（已修，回归守卫）'),
    ('fcmp', 'scripts/probe/ir_form_fcmp.c', '浮比六种条件（抽浮比 / 总优一）'),
]


def sha(p):
    h = hashlib.sha256()
    with open(p, 'rb') as f:
        for chunk in iter(lambda: f.read(1 << 20), b''):
            h.update(chunk)
    return h.hexdigest()


def build(cc, src, out, ir, work):
    env = dict(os.environ)
    env['QCC_RT_PATH'] = os.path.join(ROOT, 'srclib', 'qcc_rt.c')
    env.pop('QCC_CODEGEN', None)
    if ir:
        env['QCC_CODEGEN'] = 'ir'
    r = subprocess.run([cc, src, '-c', '-o', out], cwd=work, capture_output=True, env=env)
    msg = (r.stdout or b'').decode('utf-8', 'replace') + (r.stderr or b'').decode('utf-8', 'replace')
    return r.returncode, msg


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cc', default='qcc_zh_v2.exe')
    ap.add_argument('--only', default='')
    a = ap.parse_args()
    cc = a.cc if os.path.isabs(a.cc) else os.path.join(ROOT, a.cc)
    if not os.path.exists(cc):
        print('[环境未就绪] 缺编译器 %s' % cc)
        return 2
    only = [x.strip() for x in a.only.split(',') if x.strip()]
    work = tmp_dir('ir_forms')
    rows = []
    bad = []
    for name, rel, note in FORMS:
        if only and name not in only:
            continue
        src = os.path.join(ROOT, rel)
        if not os.path.exists(src):
            rows.append((name, 'MISS', '-', '-', '缺探针 %s' % rel))
            bad.append(name)
            continue
        ol = os.path.join(work, name + '_lg.o')
        oi = os.path.join(work, name + '_ir.o')
        rc_l, msg_l = build(cc, src, ol, False, work)
        rc_i, msg_i = build(cc, src, oi, True, work)
        if rc_l != 0:
            rows.append((name, 'LGRED', '-', '-', (msg_l.strip().split('\n')[-1] if msg_l.strip() else '')[:90]))
            bad.append(name)
            continue
        if rc_i != 0:
            loud = ('IR-BACKEND-UNIMPL' in msg_i) or ('[IR-REG]' in msg_i)
            m = re.search(r'形态=(\S+)', msg_i)
            form = m.group(1) if m else '(无形态名)'
            rows.append((name, 'RED', form, '-', ('响亮拒收' if loud else '**静默失败**')))
            bad.append(name)
            continue
        sl, si = sha(ol), sha(oi)
        if sl != si:
            rows.append((name, 'DIFF', '-', sl[:8] + '/' + si[:8], '字节不同'))
            bad.append(name)
        else:
            rows.append((name, 'OK', '-', sl[:8], ''))

    print('== IR 形态族判据（cc=%s） ==' % os.path.basename(cc))
    print('  %-9s %-6s %-16s %-18s %s' % ('族', '状态', '编译器形态名', 'sha8 legacy/ir', '备注'))
    for name, st, form, sh, note in rows:
        print('  %-9s %-6s %-16s %-18s %s' % (name, st, form, sh, note))
    ok = len(rows) - len(bad)
    print('\n形态族：%d/%d 通过' % (ok, len(rows)))
    if bad:
        print('[红] 未通过族：%s' % '、'.join(bad))
        print('     （本判据即形态清单 ratchet：每实现一族 ⟹ 该行转 OK；新形态 ⟹ 必须在此登记）')
        return 1
    print('[通过 PASS] 全部形态族 IR 与 legacy 逐字节一致')
    return 0


if __name__ == '__main__':
    sys.exit(main())
