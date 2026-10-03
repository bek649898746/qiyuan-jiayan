# -*- coding: utf-8 -*-
"""矩阵 v2 —— **全交叉积**: 声明位置{局部,形参,成员字段,全局} × 形状{标量,1D,2D}
   × 形态{标量:裸名/*/&; 数组:裸名/[i]/[i][j]}。每格生成 直调 与 管道 两枚探针。"""
import os, sys, io
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8')
D = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'matrix2')
os.makedirs(D, exist_ok=True)

H = '整 加一(整 x) { 返 x + 1; }\n整 倍二(整 x) { 返 x * 2; }\n'
S1 = '整 (*fp)(整) = 加一;'
S1D = '整 (*t[2])(整) = {加一, 倍二};'
S2D = '整 (*t2[2][3])(整) = {{加一, 倍二, 加一}, {倍二, 加一, 倍二}};'
GS1 = '整 (*gf)(整) = 加一;'
GS1D = '整 (*gt[2])(整) = {加一, 倍二};'
GS2D = '整 (*gt2[2][3])(整) = {{加一, 倍二, 加一}, {倍二, 加一, 倍二}};'
MEM = '构 盒 { 整 (*f)(整); 整 (*t[2])(整); 整 (*t2[2][3])(整); }\n'
MEMS = '构 盒 s; s.f = 加一; s.t[0] = 加一; s.t[1] = 倍二; s.t2[0][0] = 加一; s.t2[1][1] = 倍二;'

# (id, 声明块, 前置语句, 表达式)
CASES = []
# --- 局部 标量 ---
for form, ex in (('bare', 'fp(5)'), ('deref', '(*fp)(5)'), ('addr', '(&fp)(5)')):
    CASES.append(('LOC_scalar_' + form, H, S1, ex, 'fp', '5 |> ' + ('fp' if form == 'bare' else ('*fp' if form == 'deref' else '&fp'))))
# --- 局部 1D ---
CASES.append(('LOC_1d_bare', H, S1D, 't(5)', 't', '5 |> t'))
CASES.append(('LOC_1d_i', H, S1D, 't[0](5)', 't[0]', '5 |> t[0]'))
# --- 局部 2D ---
CASES.append(('LOC_2d_bare', H, S2D, 't2(5)', 't2', '5 |> t2'))
CASES.append(('LOC_2d_i', H, S2D, 't2[0](5)', 't2[0]', '5 |> t2[0]'))
CASES.append(('LOC_2d_ij', H, S2D, 't2[0][0](5)', 't2[0][0]', '5 |> t2[0][0]'))
# --- 全局 ---
CASES.append(('GLB_scalar_bare', H, GS1, 'gf(5)', 'gf', '5 |> gf'))
CASES.append(('GLB_scalar_deref', H, GS1, '(*gf)(5)', '(*gf)', '5 |> *gf'))
CASES.append(('GLB_scalar_addr', H, GS1, '(&gf)(5)', '(&gf)', '5 |> &gf'))
CASES.append(('GLB_1d_bare', H, GS1D, 'gt(5)', 'gt', '5 |> gt'))
CASES.append(('GLB_1d_i', H, GS1D, 'gt[0](5)', 'gt[0]', '5 |> gt[0]'))
CASES.append(('GLB_2d_bare', H, GS2D, 'gt2(5)', 'gt2', '5 |> gt2'))
CASES.append(('GLB_2d_i', H, GS2D, 'gt2[0](5)', 'gt2[0]', '5 |> gt2[0]'))
CASES.append(('GLB_2d_ij', H, GS2D, 'gt2[0][0](5)', 'gt2[0][0]', '5 |> gt2[0][0]'))
# --- 成员字段 ---
CASES.append(('MEM_scalar', H + MEM, MEMS, 's.f(5)', 's.f', '5 |> s.f'))
CASES.append(('MEM_1d_bare', H + MEM, MEMS, 's.t(5)', 's.t', '5 |> s.t'))
CASES.append(('MEM_1d_i', H + MEM, MEMS, 's.t[0](5)', 's.t[0]', '5 |> s.t[0]'))
CASES.append(('MEM_2d_i', H + MEM, MEMS, 's.t2[0](5)', 's.t2[0]', '5 |> s.t2[0]'))
CASES.append(('MEM_2d_ij', H + MEM, MEMS, 's.t2[0][0](5)', 's.t2[0][0]', '5 |> s.t2[0][0]'))
CASES.append(('ARR_scalar', H + MEM, MEMS + ' 构 盒 *p = &s;', 'p->f(5)', 'p->f', '5 |> p->f'))
CASES.append(('ARR_1d_i', H + MEM, MEMS + ' 构 盒 *p = &s;', 'p->t[0](5)', 'p->t[0]', '5 |> p->t[0]'))
CASES.append(('ARR_1d_bare', H + MEM, MEMS + ' 构 盒 *p = &s;', 'p->t(5)', 'p->t', '5 |> p->t'))
# --- 形参 ---
CASES.append(('LOC_1d_deref', H, S1D, '(*(t[0]))(5)', '*(t[0])', '5 |> *(t[0])'))
CASES.append(('LOC_pp_deref2', H, '整 (*fp)(整) = 加一; 整 (**pp)(整) = &fp;', '(**pp)(5)', '**pp', '5 |> **pp'))
CASES.append(('LOC_pp_deref1', H, '整 (*fp)(整) = 加一; 整 (**pp)(整) = &fp;', '(*pp)(5)', '*pp', '5 |> *pp'))
CASES.append(('PAR_scalar', H, '', None, None, None))
CASES.append(('PAR_1d_i', H, '', None, None, None))

for cid, h, decl, dex, dtxt, pex in CASES:
    if cid.startswith('PAR'):
        if cid == 'PAR_scalar':
            body_d = '整 应用(整 (*fn)(整), 整 v) { 返 fn(v); }\n函数 整 主(空) { 整 甲 = 应用(加一, 5); 格式化 "CELL %s %%d\\n", 甲; 返 0; }\n' % cid
            body_p = '整 应用(整 (*fn)(整), 整 v) { 返 v |> fn; }\n函数 整 主(空) { 整 甲 = 应用(加一, 5); 格式化 "CELL %s %%d\\n", 甲; 返 0; }\n' % cid
            dsrc, psrc = h + body_d, h + body_p
        else:
            body_d = '整 应用(整 (*fn[2])(整), 整 v) { 返 fn[0](v); }\n函数 整 主(空) { 整 (*t[2])(整) = {加一, 倍二}; 整 甲 = 应用(t, 5); 格式化 "CELL %s %%d\\n", 甲; 返 0; }\n' % cid
            body_p = '整 应用(整 (*fn[2])(整), 整 v) { 返 v |> fn[0]; }\n函数 整 主(空) { 整 (*t[2])(整) = {加一, 倍二}; 整 甲 = 应用(t, 5); 格式化 "CELL %s %%d\\n", 甲; 返 0; }\n' % cid
            dsrc, psrc = h + body_d, h + body_p
    elif cid.startswith('ARR'):
        dsrc = h + '函数 整 主(空) {\n    ' + MEMS + '\n    构 盒 *p = &s;\n    整 甲 = ' + dex + ';\n    格式化 "CELL ' + cid + ' %d\\n", 甲;\n    返 0;\n}\n'
        psrc = h + '函数 整 主(空) {\n    ' + MEMS + '\n    构 盒 *p = &s;\n    整 甲 = ' + pex + ';\n    格式化 "CELL ' + cid + ' %d\\n", 甲;\n    返 0;\n}\n'
    else:
        dsrc = h + '函数 整 主(空) {\n    ' + decl + '\n    整 甲 = ' + dex + ';\n    格式化 "CELL ' + cid + ' %d\\n", 甲;\n    返 0;\n}\n'
        psrc = h + '函数 整 主(空) {\n    ' + decl + '\n    整 甲 = ' + pex + ';\n    格式化 "CELL ' + cid + ' %d\\n", 甲;\n    返 0;\n}\n'
    open(os.path.join(D, cid + '_d.c'), 'w', encoding='utf-8-sig', newline='\r\n').write(dsrc)
    open(os.path.join(D, cid + '_p.c'), 'w', encoding='utf-8-sig', newline='\r\n').write(psrc)
print('generated', len(CASES), 'cells ->', len(CASES) * 2, 'probes in', D)
