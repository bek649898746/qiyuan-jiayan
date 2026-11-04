# -*- coding: utf-8 -*-
"""check_typedef_list —— **typedef 声明符列表**（C11 6.7.6 声明符 / 6.7.8 类型定义）判据。

## 病根（单一）

`typedef` 语句 的 **声明符** 原 只 认 一个：
· 文件级 —— 首个 声明符 之后 的 `, *B` / `, C[N]` 整段 交给「跳到分号」兜底 ⟹ B/C **从未登记**；
· 块级   —— 四条 内联 分支（普通标量 / 匿名体 / 具名体 / 无体标签）**每条只认一个**，且
            `typedef 整 *P;` / `typedef 整 A[4];` 连首符 都不登记（判据 把 `*`/`[` 排除）。

后果是 **静默** 的：未登记的别名在使用点落「未知类型」兜底 = 4 字节整数 ⟹
`typedef 构 { 整 a; 整 b; } SP, *SPP;` 的 `SPP` 被当 `int` ⟹ `(long long)(spp + 1) - (long long)spp`
得 **1**（C 语义 **8**）。

## 判据（期望值 = gcc 14.2.0 派生）

文件级：匿名体 `SP, *SPP` · 具名体 `TP, *TPP` · 标量 `IA, IB` · 指针 `PA, PB` · 数组 `AR1[2], AR2[3]`
块级  ：同族四形态（匿名体 / 标量 / 指针 / 具名体）

修前（`21412505`）实测：文件级 4 项 编译失败（A1001 未声明）或步长 1；块级 3 项失败。

用法: python scripts/check_typedef_list.py [--cc qcc_zh_v1.exe]
退出码: 0 = 全对；1 = 有偏差；2 = 环境未就绪
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
WORK = tmp_dir('scratch_typedef_list')

SRC = '''# 包含 <stdio.h>
typedef 构 { 整 a; 整 b; } SP, *SPP;
typedef 构 TT { 整 a; 整 b; } TP, *TPP;
typedef 整 IA, IB;
typedef 整 *PA, *PB;
typedef 整 AR1[2], AR2[3];
typedef 构 SP2 { 整 a; 整 b; } SP2, *SP2P, **SP2PP;   /* 双星续符：指构序 必须仍指向 SP2 */
静态 整 倍二(整 x){ 返 x * 2; }
/* 函数声明符 与 其它声明符 混排（面板第四轮）：函数类型 / 函数指针 在前或在后，后续符 必须 仍 用 **基类型** */
typedef 整 FN1(整), AN1[2];
typedef 整 (*FP1)(整), BN1[2];
typedef 整 CN1[2], FN2(整);
typedef 整 DN1[2], (*GN2)(整);
函数 整 主(空){
    SP st; SPP spp = (SPP)&st;
    long long s1 = (long long)(spp + 1) - (long long)spp;  /* 8  · 匿名体 指针续符 步长 */
    long long s2 = 大小(SPP);                              /* 8  · 续符 是 指针 */
    long long s3 = 大小(TPP);                              /* 8  · 具名体 指针续符 */
    long long s4 = 大小(IA) + 大小(IB);                    /* 8  · 标量 续符 */
    long long s5 = 大小(PA) + 大小(PB);                    /* 16 · 指针 续符 */
    整 x = 0; PA pa = &x;
    long long s6 = (long long)(pa + 1) - (long long)pa;    /* 4  · 指针 续符 的 元素宽 */
    long long s7 = 大小(AR1);                              /* 8  · 数组 首符 */
    long long s8 = 大小(AR2);                              /* 12 · 数组 续符 */
    typedef 构 { 整 a; 整 b; } LSP, *LSPP;
    typedef 构 TQ { 整 a; 整 b; } LTP, *LTPP;
    typedef 整 LIA, LIB;
    typedef 整 *LPA, LPB;
    LSP lv; LSPP lp = (LSPP)&lv;
    long long s9  = (long long)(lp + 1) - (long long)lp;   /* 8  · 块级 匿名体 步长 */
    long long s10 = 大小(LSPP);                            /* 8 */
    long long s11 = 大小(LTPP);                            /* 8  · 块级 具名体 续符 */
    long long s12 = 大小(LIB) + 大小(LPB);                 /* 8  · 块级 标量续符(4) + `typedef 整 *LPA, LPB;` 的 LPB 是**标量**(4) */
    long long s13 = 大小(LIA);                             /* 4  · 块级 首符（控制格） */
    /* 双星续符（面板第三轮）：`SP2, *SP2P, **SP2PP` —— 指构序 必须仍指向 SP2，
       否则 `(*pp)->a` / `(**pp).b` 的成员链断（修前 得 地址量级垃圾）。 */
    SP2 sv; sv.a = 10; sv.b = 20; SP2 *ps = &sv; SP2PP pp = &ps;
    long long s14 = (long long)(*pp)->a;                   /* 10 */
    long long s15 = (long long)(**pp).b;                   /* 20 */
    long long s16 = (long long)(pp + 1) - (long long)pp;   /* 8  · 双星步长 = 指针宽 */
    /* 函数声明符 混排（面板第四轮）—— 文件级四格 */
    long long s17 = 大小(AN1);                             /* 8  · F(int), A[2] */
    long long s18 = 大小(BN1);                             /* 8  · (*F)(int), A[2] */
    FN2 *f2 = 倍二; long long s19 = f2(21);                /* 42 · A[2], F(int) 可调用 */
    GN2 g2 = 倍二; long long s20 = g2(21);                 /* 42 · A[2], (*G)(int) 可调用 */
    long long s21 = 大小(CN1) + 大小(DN1);                 /* 16 · 首符 数组 尺寸 不被 函数符 吃掉 */
    /* 块级同族四格 */
    typedef 整 LF1(整), LA1[2];
    typedef 整 (*LFP)(整), LB1[2];
    typedef 整 LC1[2], LF2(整);
    typedef 整 LD1[2], (*LG2)(整);
    LF2 *lf = 倍二; LG2 lg = 倍二;
    long long s22 = 大小(LA1) + 大小(LB1);                 /* 16 */
    long long s23 = lf(21) + lg(21);                       /* 84 */
    long long s24 = 大小(LC1) + 大小(LD1);                 /* 16 */
    若 (s1 == 8 && s2 == 8 && s3 == 8 && s4 == 8 && s5 == 16 && s6 == 4 && s7 == 8 && s8 == 12
        && s9 == 8 && s10 == 8 && s11 == 8 && s12 == 8 && s13 == 4
        && s14 == 10 && s15 == 20 && s16 == 8
        && s17 == 8 && s18 == 8 && s19 == 42 && s20 == 42 && s21 == 16
        && s22 == 16 && s23 == 84 && s24 == 16) { 格式化 "TL OK\\n", 0; }
    否 { 格式化 "TL BAD %lld %lld %lld %lld %lld %lld %lld %lld %lld %lld %lld %lld %lld %lld %lld %lld %lld %lld %lld %lld %lld %lld %lld %lld\\n",
        s1, s2, s3, s4, s5, s6, s7, s8, s9, s10, s11, s12, s13, s14, s15, s16,
        s17, s18, s19, s20, s21, s22, s23, s24; }
    返 0;
}
'''


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cc', default=os.environ.get('QCC_TEST_BIN') or 'qcc_zh_v1.exe')
    a = ap.parse_args()
    cc = a.cc if os.path.isabs(a.cc) else os.path.join(REPO, a.cc)
    os.makedirs(WORK, exist_ok=True)
    p = os.path.join(WORK, 'typedef_list.jy')
    exe = os.path.join(WORK, 'typedef_list.exe')
    io.open(p, 'w', encoding='utf-8', newline='\n').write(SRC)
    r = subprocess.run([cc, p, '-o', exe], cwd=REPO, capture_output=True, text=True,
                       encoding='utf-8', errors='replace', timeout=180)
    if r.returncode != 0:
        print('[编译红] rc=%d : %s' % (r.returncode, ((r.stdout or '') + (r.stderr or '')).strip()[:240]))
        return 1
    q = subprocess.run([exe], capture_output=True, text=True,
                       encoding='utf-8', errors='replace', timeout=60)
    out = (q.stdout or '').strip()
    print('[run] %s' % out)
    if out.startswith('TL OK'):
        print('[PASS] typedef 声明符列表（文件级 5 形态 + 块级 4 形态）逐项对齐 gcc')
        return 0
    print('[FAIL] typedef 声明符列表 有偏差（期望 s1..s13 = 8 8 8 8 16 4 8 12 8 8 8 8 4）')
    return 1


if __name__ == '__main__':
    sys.exit(main())
