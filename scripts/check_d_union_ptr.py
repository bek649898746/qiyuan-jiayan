#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""D-UNION-PTR 判据 —— 联合体成员的**指针元数据 / 类型序**必须与结构体同源。

## 病灶

`联合字段()` 是联合成员的唯一登记口，它按 C 语义把偏移强制 0，并**明文清空**
`原进/符肆/主挂/高贰`（联合成员不设"指针位"）。而补写这些位的代码只存在于
**结构体分支**里 ⟹ 联合里带 `*` 的成员 原进=0、毛利=-1 ⟹ `u.p->v` 被当成
"取 &u + 成员偏移" 的取址表达式，直接返回 `&u`（实测 541455336，gcc 42）。

同族第二面：联合里 **typedef 结构体值成员**（`联 U { 别名 n; }`）缺 `毛利`
⟹ `u.n.v` 报 A9001「字段 n 不是结构体/联合」。

## 判据

12 个形态，全部 **gcc 真值 = 42**（每个 .c 都是自足的标准 C，等价的 C 写法见 CASES 注释）。
逐形态 编译 → 运行 → 比对 `R 42`；任一编译失败或取值不等即 FAIL。
形态覆盖五处解析入口：① 已知结构体 tag ② 前向声明 tag ③ typedef/型词
④ 嵌套内联具名体 ⑤ 嵌套内联匿名体 —— 各入口漏写元数据都会在这里红。

用法：
    python scripts/check_d_union_ptr.py             # 默认 qcc_zh_v1.exe
    python scripts/check_d_union_ptr.py --cc qcc_pd.exe
退出码：0 = 12/12 PASS，1 = 有 FAIL，2 = 环境未就绪
"""
import argparse
import os
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
WORK = os.path.join(ROOT, 'scratch_dunionptr')

HDR = '# 包含 <stdio.h>\n构 内 { 整 v; };\n'

CASES = {
    # ---- 对照（改前即绿，用于证明判据不过宽）----
    'A_int_union': (HDR + '联 U { 整 *p; 整 i; };\n'
                    '整 主(){ 整 值=42; 联 U u; u.p=&值; printf("R %d\\n", *u.p); 返 0; }\n',
                    'union U { int *p; }; *u.p'),
    'H_struct_ctl': (HDR + '构 S { 构 内 *p; 整 i; };\n'
                     '整 主(){ 构 内 界; 界.v=42; 构 S s; s.p=&界; printf("R %d\\n", s.p->v); 返 0; }\n',
                     'struct S { struct In *p; }; s.p->v'),
    'K_union_val': (HDR + '联 U { 构 内 n; 整 i; };\n'
                    '整 主(){ 联 U u; u.n.v=42; printf("R %d\\n", u.n.v); 返 0; }\n',
                    'union U { struct In n; }; u.n.v'),

    # ---- 病灶（改前红）----
    'B_union_structptr': (HDR + '联 U { 构 内 *p; 整 i; };\n'
                          '整 主(){ 构 内 界; 界.v=42; 联 U u; u.p=&界; printf("R %d\\n", u.p->v); 返 0; }\n',
                          'union U { struct In *p; }; u.p->v  → 原返回 &u'),
    'E_union_typedef': (HDR + '类型定义 构 内 别名;\n联 U { 别名 *p; 整 i; };\n'
                        '整 主(){ 构 内 界; 界.v=42; 联 U u; u.p=&界; printf("R %d\\n", u.p->v); 返 0; }\n',
                        'typedef struct In Alias; union U { Alias *p; }; u.p->v'),
    'F_union_fwd': ('# 包含 <stdio.h>\n构 内;\n联 U { 构 内 *p; 整 i; };\n构 内 { 整 v; };\n'
                    '整 主(){ 构 内 界; 界.v=42; 联 U u; u.p=&界; printf("R %d\\n", u.p->v); 返 0; }\n',
                    'struct In; union U { struct In *p; }; struct In { int v; }; u.p->v'),
    'G_anon_union': (HDR + '构 S { 整 头; 联 { 构 内 *p; 整 i; }; };\n'
                     '整 主(){ 构 内 界; 界.v=42; 构 S s; s.p=&界; printf("R %d\\n", s.p->v); 返 0; }\n',
                     'struct S { union { struct In *p; }; }; s.p->v'),
    'I_union_copy': (HDR + '联 U { 构 内 *p; 整 i; };\n'
                     '整 主(){ 构 内 界; 界.v=42; 联 U u; u.p=&界; 联 U 副; 副.p=u.p; '
                     'printf("R %d\\n", 副.p->v); 返 0; }\n',
                     'union U u, w; w.p = u.p; w.p->v'),
    'J_union_dblptr': (HDR + '联 U { 构 内 **pp; 整 i; };\n'
                       '整 主(){ 构 内 界; 界.v=42; 构 内 *q=&界; 联 U u; u.pp=&q; '
                       'printf("R %d\\n", u.pp[0]->v); 返 0; }\n',
                       'union U { struct In **pp; }; u.pp[0]->v'),
    'L_union_typedefval': (HDR + '类型定义 构 内 别名;\n联 U { 别名 n; 整 i; };\n'
                           '整 主(){ 联 U u; u.n.v=42; printf("R %d\\n", u.n.v); 返 0; }\n',
                           'typedef struct In Alias; union U { Alias n; }; u.n.v  → 原报 A9001'),
    'M_union_arrptr': (HDR + '联 U { 构 内 *p[2]; 整 i; };\n'
                       '整 主(){ 构 内 界; 界.v=42; 联 U u; u.p[0]=&界; printf("R %d\\n", u.p[0]->v); 返 0; }\n',
                       'union U { struct In *p[2]; }; u.p[0]->v'),
    'N_nested_unionptr': (HDR + '构 外 { 整 头; 联 深 { 构 内 *p; 整 i; } 身; };\n'
                          '整 主(){ 构 内 界; 界.v=42; 构 外 o; o.身.p=&界; '
                          'printf("R %d\\n", o.身.p->v); 返 0; }\n',
                          'struct W { union D { struct In *p; } body; }; o.body.p->v'),
}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cc', default='qcc_zh_v1.exe')
    ap.add_argument('--only', default='', help='只跑名字含该子串的形态')
    args = ap.parse_args()
    cc = args.cc if os.path.isabs(args.cc) else os.path.join(ROOT, args.cc)
    if not os.path.exists(cc):
        print('[SKIP] 环境未就绪: %s 不存在' % cc)
        return 2
    os.makedirs(WORK, exist_ok=True)
    print('# D-UNION-PTR 判据  cc=%s  (gcc 真值 = 42)' % os.path.basename(cc))
    total = bad = 0
    for name, (src, note) in CASES.items():
        if args.only and args.only not in name:
            continue
        total += 1
        p = os.path.join(WORK, name + '.c')
        with open(p, 'w', encoding='utf-8', newline='\n') as f:
            f.write(src)
        exe = os.path.join(WORK, name + '.exe')
        r = subprocess.run([cc, p, '-o', exe], cwd=ROOT, capture_output=True,
                           env=dict(os.environ, QCC_SMALL_HEAP='1'), timeout=300)
        if r.returncode != 0:
            bad += 1
            msg = (r.stderr or r.stdout).decode('utf-8', 'replace').strip().splitlines()
            print('[%-20s] FAIL 编译 rc=%d  %s' % (name, r.returncode, (msg[0] if msg else '')[:110]))
            continue
        e = subprocess.run([exe], capture_output=True, timeout=60)
        out = e.stdout.decode('utf-8', 'replace').strip()
        good = (e.returncode == 0 and out == 'R 42')
        if not good:
            bad += 1
        print('[%-20s] %s  %-8r  期望 %-6r  # %s'
              % (name, 'PASS' if good else 'FAIL', out, 'R 42', note))
    print('# 汇总: %d/%d PASS' % (total - bad, total))
    return 0 if bad == 0 else 1


if __name__ == '__main__':
    sys.exit(main())
