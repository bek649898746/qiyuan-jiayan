# -*- coding: utf-8 -*-
"""第 455 站 第十轮: 生成「可调用性 符合性矩阵」探针（每条 直接调用 / 管道 两枚）。
   维度: 声明位置 {局部, 形参, 成员字段, 全局} × 形状 {标量, 1D数组, 2D数组} × 形态 {裸名, [i], [i][j], *, &}
   用法: 对每格 比较 「直接调用是否成功」 与 「管道是否被 A6028 拒」 —— 二者必须一致。"""
import os
D = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'matrix')
os.makedirs(D, exist_ok=True)

HDR_L = '整 加一(整 x) { 返 x + 1; }\n整 倍二(整 x) { 返 x * 2; }\n'
HDR_G = HDR_L + '整 (*gf)(整) = 加一;\n整 (*gt[2])(整) = {加一, 倍二};\n'

CASES = {}
# ---- 局部 标量 ----
CASES['L_scalar_bare'] = (HDR_L, '整 (*fp)(整) = 加一;', 'fp(5)', '5 |> fp')
CASES['L_scalar_deref'] = (HDR_L, '整 (*fp)(整) = 加一;', '(*fp)(5)', '5 |> *fp')
CASES['L_scalar_addr'] = (HDR_L, '整 (*fp)(整) = 加一;', '(&fp)(5)', '5 |> &fp')
# ---- 局部 1D 数组 ----
CASES['L_1d_bare'] = (HDR_L, '整 (*t[2])(整) = {加一, 倍二};', 't(5)', '5 |> t')
CASES['L_1d_elem'] = (HDR_L, '整 (*t[2])(整) = {加一, 倍二};', 't[0](5)', '5 |> t[0]')
CASES['L_1d_elem1'] = (HDR_L, '整 (*t[2])(整) = {加一, 倍二};', 't[1](5)', '5 |> t[1]')
# ---- 全局 标量 ----
CASES['G_scalar_bare'] = (HDR_G, '', 'gf(5)', '5 |> gf')
# ---- 全局 1D 数组 ----
CASES['G_1d_bare'] = (HDR_G, '', 'gt(5)', '5 |> gt')
CASES['G_1d_elem'] = (HDR_G, '', 'gt[0](5)', '5 |> gt[0]')
CASES['G_1d_elem1'] = (HDR_G, '', 'gt[1](5)', '5 |> gt[1]')
# ---- 形参 标量 ----
CASES['P_scalar_bare'] = (HDR_L, 'PARAM', 'fn(5)', '5 |> fn')
# ---- 成员 标量字段 ----
CASES['M_scalar_field'] = (HDR_L, 'MEMBER', 's.f(5)', '5 |> s.f')
# ---- 局部 2D 数组 ----
CASES['L_2d_row'] = (HDR_L, '整 (*t2[2][3])(整);', 't2[0](5)', '5 |> t2[0]')
CASES['L_2d_elem'] = (HDR_L, '整 (*t2[2][3])(整);', 't2[0][0](5)', '5 |> t2[0][0]')
CASES['L_2d_bare'] = (HDR_L, '整 (*t2[2][3])(整);', 't2(5)', '5 |> t2')

for cid, (hdr, decl, direct, pipe) in CASES.items():
    for kind, expr in (('d', direct), ('p', pipe)):
        body = ''
        if decl == 'PARAM':
            body = ('整 应用(整 (*fn)(整), 整 v) { 返 %s; }\n' % (expr if kind == 'd' else expr)) if False else None
        # 统一生成
        if decl == 'PARAM':
            fn_sig = '整 应用(整 (*fn)(整), 整 v)'
            stmt = ('整 甲 = 应用_体;' if False else None)
            src = (HDR_L + fn_sig + ' {\n    整 甲 = ' + expr + ';\n    格式化 "CELL ' + cid + ' %d\\n", 甲;\n    返 0;\n}\n'
                   + '函数 整 主(空) { 返 应用(加一, 5); }\n')
        elif decl == 'MEMBER':
            src = (HDR_L + '构 盒 { 整 (*f)(整); }\n'
                   + '函数 整 主(空) {\n    构 盒 s; s.f = 加一;\n    整 甲 = ' + expr
                   + ';\n    格式化 "CELL ' + cid + ' %d\\n", 甲;\n    返 0;\n}\n')
        else:
            src = (hdr + '函数 整 主(空) {\n    ' + decl + '\n    整 甲 = ' + expr
                   + ';\n    格式化 "CELL ' + cid + ' %d\\n", 甲;\n    返 0;\n}\n')
        with open(os.path.join(D, '%s_%s.c' % (cid, kind)), 'w', encoding='utf-8-sig', newline='\r\n') as f:
            f.write(src)
print('generated', len(CASES) * 2, 'probes in', D)
print('cases:', ' '.join(sorted(CASES)))
