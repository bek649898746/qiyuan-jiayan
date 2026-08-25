# -*- coding: utf-8 -*-
"""差异测试台 v2: qcc vs gcc 半随机 C 程序差异比对 (门禁版)

用法:
  python scripts/diff_gate.py [轮数] [起始seed] [seed数] [--family types,vararg,layout,ctrl,expr] [--qcc v1.exe]

设计目标 (大哥战略 2026-08-25):
  批量暴露 类型尺寸 / 变参 / 布局敏感 三族 bug (在编 Git 之前)。

v1 -> v2 升级:
  1. 假阳性修复: char 只用 0..127 / 除零分母保护 / 移位 0..31 (qcc=unsigned char 约定)
  2. 强转掩盖修复: 格式符按表达式真实类型选 (不统一 (long long) 强转)
  3. 按 bug 族分层生成 (types/vararg/layout/ctrl/expr)
  4. 结果分类 + 归档到 D 盘 + fail-closed (真 DIFF>0 退出码非零)

qcc 支持子集边界 (explore 2026-08-25 实证):
  - 变参: va_list/va_start/va_arg/va_end 支持; 变参 1-3 个; char/short 按 int 读
  - sizeof: 多级指针=8/前向声明指针=8/typedef/裸变量/数组 全覆盖
  - uint8_t..uint64_t/size_t/bool: compat_prelude.h 补齐 (必须 -I)
  - struct/union/位域/fnptr: 覆盖广; 规避深层嵌套赋值/union 混排
  - 黑名单: sizeof(不完整类型)/long double/%e/%g/%a/变参>3/double 变参
"""
import random, subprocess, os, sys, io, hashlib

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
GITDIR = r'C:\Users\Administrator\Desktop\git-2.45.2'
PRELUDE = os.path.join(ROOT, 'compat_prelude.h')
DEFAULT_QCC = os.path.join(ROOT, 'v1.exe')  # 当前自举编译器 (bug#26/#27 修复后不动点)
OUT_DIR = r'D:\qiyuan_home\diff_out'        # D 盘输出 (354GB), C 盘零负担
os.makedirs(OUT_DIR, exist_ok=True)

# ---------------- 生成器 (按 bug 族分层) ----------------

FAMILIES = ['types', 'vararg', 'layout', 'ctrl', 'expr']

# 非零分母保护: 除法/取模右操作数用 "lit" (安全字面量) 或 "(x)+lit"
SAFE_INT = [1, 2, 3, 5, 7, 11, 100]
SAFE_VAL = [0, 1, 2, 3, 7, 10, 100, 255, 1000, -1, -5]
CHAR_VAL = list(range(0, 128))              # char 只用 0..127 (qcc unsigned 约定)
UINT32_EDGE = [0, 1, 2, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFE, 0xFFFFFFFF]
LL64_EDGE = [0, 1, 2, 0x7FFFFFFFFFFFFFFF, 0x8000000000000000, 0xFFFFFFFFFFFFFFFF]

def rnd_fmt(r):
    """按真实类型选 printf 格式 (v2 强转掩盖修复)"""
    return {'int': '%d', 'uint': '%u', 'll': '%lld', 'ull': '%llu',
            'double': '%.1f', 'char': '%d', 'short': '%d'}[r]


class Gen:
    def __init__(self, rng):
        self.r = rng
        self.vn = 0

    def v(self):
        self.vn += 1
        return 'v%d' % self.vn

    def int_expr(self, depth=0, ty='int'):
        """生成 int/uint 表达式, 返回 (text, type). 保证无除零/移位>=32."""
        r = self.r
        if depth <= 0 or r.random() < 0.35:
            if ty == 'uint':
                return (r.choice(['%d' % x for x in UINT32_EDGE] + ['0x%X' % x for x in UINT32_EDGE]), 'uint')
            return (str(r.choice(SAFE_VAL)), 'int')
        op = r.choice(['+', '-', '*', '&', '|', '^', '/', '%', '<<', '>>'])
        a, at = self.int_expr(depth - 1, ty)
        if op in ('/', '%'):
            b = r.choice(['%d' % x for x in SAFE_INT])  # 非零分母
            bt = 'int'
        elif op in ('<<', '>>'):
            b = r.choice(['%d' % x for x in range(0, 32)])  # 移位 0..31
            bt = 'int'
        else:
            b, bt = self.int_expr(depth - 1, ty)
        # 混合符号: 保持类型 (int+uint 时转 uint 语义由 C 提升决定, 生成器不强制)
        out_ty = ty
        return ('(%s %s %s)' % (a, op, b), out_ty)

    def ll_expr(self, depth=0, ty='ll'):
        r = self.r
        if depth <= 0 or r.random() < 0.35:
            return (r.choice(['%d' % x for x in LL64_EDGE[:5]]), ty)
        op = r.choice(['+', '-', '*'])
        a, _ = self.ll_expr(depth - 1, ty)
        b = r.choice(['%d' % x for x in SAFE_INT + [0x7FFFFFFF, -0x7FFFFFFF]])
        return ('(%s %s %s)' % (a, op, b), ty)

    # ---- types 族: sizeof / 类型宽度 / 混合提升 / 回绕 ----
    def gen_types(self):
        r = self.r
        lines = ['#include <stdio.h>', '#include <stdint.h>', '#include <stddef.h>']
        # typedef + struct 声明 (sizeof 目标) — 共享标志
        has_ts = r.random() < 0.7
        if has_ts:
            lines.append('struct TS { int a; char b; long long c; };')
            lines.append('struct TFwd;')
            lines.append('typedef unsigned long long U64;')
        lines.append('int main(void) {')
        m = []
        # 局部变量
        types = ['int', 'unsigned', 'char', 'short', 'long long', 'unsigned long long',
                 'uint8_t', 'uint16_t', 'uint32_t', 'size_t', 'double']
        for t in r.sample(types, 4):
            n = self.v()
            if t == 'char':
                lines.append('    %s %s = %d;' % (t, n, r.choice(CHAR_VAL)))
                m.append(('    printf("%%d\\n", %s);' % n, 'int'))
            elif t == 'double':
                lines.append('    %s %s = %d;' % (t, n, r.randrange(0, 20)))
                m.append(('    printf("%%.1f\\n", %s);' % n, 'double'))
            else:
                lines.append('    %s %s = %d;' % (t, n, r.choice(SAFE_VAL)))
                m.append(('    printf("%%llu\\n", (unsigned long long)%s);' % n, 'ull'))
        # sizeof 表达式 (类型尺寸族核心)
        sz_targets = ['int', 'char', 'short', 'long long', 'double',
                      'int *', 'int **', 'void *', 'int[5]', '"hello"']
        if has_ts:
            sz_targets += ['struct TS', 'struct TS *', 'U64']
        for _ in range(3):
            t = r.choice(sz_targets)
            m.append(('    printf("%%d\\n", (int)sizeof(%s));' % t, 'int'))
        # 前向声明指针 sizeof (已知修复: =8)
        if has_ts:
            m.append(('    printf("%d\\n", (int)sizeof(struct TFwd *));', 'int'))
        # 混合提升: char/short 与 int 运算
        if r.random() < 0.7:
            c = self.v(); i = self.v()
            lines.append('    char %s = %d;' % (c, r.choice(CHAR_VAL)))
            lines.append('    int %s = %d;' % (i, r.choice(SAFE_VAL)))
            m.append(('    printf("%%d\\n", %s + %s * 2);' % (c, i), 'int'))
        # unsigned 回绕
        if r.random() < 0.6:
            u = self.v()
            lines.append('    unsigned %s = 0xFFFFFFFF;' % u)
            m.append(('    printf("%%u\\n", %s + 2);' % u, 'uint'))
        # long long 64 位
        if r.random() < 0.6:
            x = self.v()
            lines.append('    long long %s = 0x7FFFFFFFFFFFFFFFLL;' % x)
            m.append(('    printf("%%lld\\n", %s + 1);' % x, 'll'))
        # char 提升成 int 后宽度
        if r.random() < 0.6:
            c2 = self.v()
            lines.append('    unsigned char %s = 200;' % c2)
            m.append(('    printf("%%d\\n", (int)%s);' % c2, 'int'))
        # 输出语句 (去重排序保持确定性)
        m.append(('    return 0;', None))
        for text, _ in m:
            lines.append(text)
        lines.append('}')
        return '\n'.join(lines)

    # ---- vararg 族: 自定义变参函数 + printf 矩阵 + 5+ 参数调用 ----
    def gen_vararg(self):
        r = self.r
        lines = ['#include <stdio.h>', '#include <stdarg.h>']
        # 自定义变参函数 (1-3 个变参, int 读取 — explore 实证边界)
        lines.append('int vsum(int n, ...) {')
        lines.append('    va_list ap; va_start(ap, n); int s = 0;')
        lines.append('    for (int i = 0; i < n; i = i + 1) s = s + va_arg(ap, int);')
        lines.append('    va_end(ap); return s;')
        lines.append('}')
        # 5+ 参数普通调用 (槽位 bug 族)
        lines.append('long long add5(long long a, long long b, long long c, long long d, long long e) {')
        lines.append('    return a + b + c + d + e;')
        lines.append('}')
        lines.append('int main(void) {')
        m = []
        # 自定义变参调用 (2-3 个实参)
        for _ in range(2):
            n = r.randint(1, 3)
            args = ', '.join(str(r.choice(SAFE_VAL)) for _ in range(n))
            m.append(('    printf("%%d\\n", vsum(%d, %s));' % (n, args), 'int'))
        # 5 参数调用
        a5 = ', '.join(str(r.choice(SAFE_VAL)) for _ in range(5))
        m.append(('    printf("%%lld\\n", add5(%s));' % a5, 'll'))
        # printf 格式矩阵 (受支持格式符)
        fmts = ['%d', '%u', '%x', '%X', '%o', '%c', '%ld', '%lld', '%llu', '%zu', '%p', '%%']
        for _ in range(4):
            f = r.choice(fmts)
            if f == '%c':
                m.append(('    printf("%%c\\n", \'%c\');' % r.choice(['A', 'z', '0', ' ']), 'int'))
            elif f == '%p':
                m.append(('    printf("%%p\\n", (void*)0);', 'int'))
            elif f in ('%ld',):
                m.append(('    printf("%%ld\\n", (long)%d);' % r.choice(SAFE_VAL), 'll'))
            elif f in ('%lld',):
                m.append(('    printf("%%lld\\n", (long long)%d);' % r.choice(SAFE_VAL), 'll'))
            elif f == '%llu':
                m.append(('    printf("%%llu\\n", (unsigned long long)%d);' % r.choice(SAFE_VAL), 'ull'))
            elif f == '%zu':
                m.append(('    printf("%%zu\\n", sizeof(int));', 'ull'))
            else:
                m.append(('    printf("%%d\\n", %d);' % r.choice(SAFE_VAL), 'int'))
        # 混合类型变参 printf (char/short 提升)
        if r.random() < 0.7:
            i = self.v(); s = self.v(); d = self.v()
            lines.append('    int %s = %d;' % (i, r.choice(SAFE_VAL)))
            lines.append('    short %s = %d;' % (s, r.choice(SAFE_VAL)))
            lines.append('    double %s = %d;' % (d, r.randrange(0, 10)))
            m.append(('    printf("%%d %%d %%.1f\\n", %s, %s, %s);' % (i, s, d), 'double'))
        m.append(('    return 0;', None))
        for text, _ in m:
            lines.append(text)
        lines.append('}')
        return '\n'.join(lines)

    # ---- layout 族: 嵌套 struct / union / 位域 / fnptr 字段 / 指针步长 ----
    def gen_layout(self):
        r = self.r
        lines = ['#include <stdio.h>', '#include <stddef.h>']
        # 共享标志: 声明与使用必须同生共死 (避免用了未声明)
        has_inner = r.random() < 0.85
        has_union = r.random() < 0.6
        has_bf = r.random() < 0.6
        has_node = r.random() < 0.5
        has_fp = r.random() < 0.5
        has_byval = has_inner and r.random() < 0.6
        # 大 struct (模拟 layout_sensitive 高危模式: z_stream_s 30 字段等, 2026-08-25)
        has_big = r.random() < 0.5
        if has_big:
            self.big_nf = r.randrange(20, 40)
            self.big_flds = []
            for bi in range(self.big_nf):
                bt = r.choice(['int', 'char', 'long long', 'int', 'unsigned', 'int', 'short'])
                self.big_flds.append('%s f%d' % (bt, bi))
            lines.append('struct Big { %s; };' % '; '.join(self.big_flds))
        # 嵌套 struct (2 层)
        if has_inner:
            lines.append('struct Inner { int a; char b; long long c; };')
            lines.append('struct Outer { struct Inner in; int z; };')
        # union (int/char 同构 — explore: 规避混排对齐)
        if has_union:
            lines.append('union U { int i; char c[4]; };')
        # 位域
        if has_bf:
            lines.append('struct BF { unsigned b0:1; unsigned b1:3; unsigned b2:4; int full; };')
        # 指针字段 + 前向声明回填
        if has_node:
            lines.append('struct Node; struct Node { int v; struct Node *next; };')
        # fnptr 字段
        if has_fp:
            lines.append('int add2(int a, int b) { return a + b; }')
            lines.append('struct FP { int (*fn)(int, int); int tag; };')
        # 按值传参函数
        if has_byval:
            lines.append('int inner_sum(struct Inner s) { return s.a + (int)s.c; }')
        lines.append('int main(void) {')
        m = []
        # 嵌套 struct 赋值 + 成员访问 (≤2 层)
        if has_inner:
            o = self.v()
            lines.append('    struct Outer %s;' % o)
            lines.append('    %s.in.a = %d;' % (o, r.choice(SAFE_VAL)))
            lines.append('    %s.in.c = %d;' % (o, r.choice(SAFE_VAL)))
            lines.append('    %s.z = %d;' % (o, r.choice(SAFE_VAL)))
            m.append(('    printf("%%d\\n", %s.in.a + (int)%s.in.c + %s.z);' % (o, o, o), 'int'))
        # union 访问
        if has_union:
            u = self.v()
            lines.append('    union U %s;' % u)
            lines.append('    %s.i = %d;' % (u, r.choice(SAFE_VAL)))
            m.append(('    printf("%%d\\n", %s.i);' % u, 'int'))
        # 位域
        if has_bf:
            b = self.v()
            lines.append('    struct BF %s;' % b)
            lines.append('    %s.b0 = 1;' % b)
            lines.append('    %s.b1 = %d;' % (b, r.randrange(0, 8)))
            lines.append('    %s.b2 = %d;' % (b, r.randrange(0, 16)))
            m.append(('    printf("%%d\\n", %s.b0 + %s.b1 + %s.b2);' % (b, b, b), 'int'))
        # 链表 (递归 struct + 指针步长)
        if has_node:
            n1 = self.v(); n2 = self.v()
            lines.append('    struct Node %s = {%d, 0};' % (n1, r.choice(SAFE_VAL)))
            lines.append('    struct Node %s = {%d, &%s};' % (n2, r.choice(SAFE_VAL), n1))
            m.append(('    printf("%%d\\n", %s.next->v);' % n2, 'int'))
        # fnptr 字段
        if has_fp:
            fp = self.v()
            lines.append('    struct FP %s;' % fp)
            lines.append('    %s.fn = add2;' % fp)
            lines.append('    %s.tag = %d;' % (fp, r.choice(SAFE_VAL)))
            m.append(('    printf("%%d\\n", %s.fn(%d, %d));' % (fp, r.choice(SAFE_VAL), r.choice(SAFE_VAL)), 'int'))
        # struct 数组步长
        if has_inner:
            a = self.v()
            lines.append('    struct Inner %s[3];' % a)
            lines.append('    %s[0].a = %d;' % (a, r.choice(SAFE_VAL)))
            lines.append('    %s[1].a = %d;' % (a, r.choice(SAFE_VAL)))
            lines.append('    %s[2].a = %d;' % (a, r.choice(SAFE_VAL)))
            m.append(('    printf("%%d\\n", %s[0].a + %s[1].a + %s[2].a);' % (a, a, a), 'int'))
        # 指针数组步长 (已知 bug 族: 指针数组步长=结构体大小)
        if has_inner and r.random() < 0.4:
            pa = self.v(); n = self.v()
            lines.append('    int %s = 42;' % n)
            lines.append('    struct Inner *%s[2];' % pa)
            sv = self.v()
            lines.append('    struct Inner %s; %s.a = 1;' % (sv, sv))
            m.append(('    printf("%%d\\n", sizeof(%s) / sizeof(%s[0]));' % (pa, pa), 'int'))
        # 按值传参 struct
        if has_byval:
            iv = self.v()
            lines.append('    struct Inner %s;' % iv)
            lines.append('    %s.a = %d;' % (iv, r.choice(SAFE_VAL)))
            lines.append('    %s.c = %d;' % (iv, r.choice(SAFE_VAL)))
            m.append(('    printf("%%d\\n", inner_sum(%s));' % iv, 'int'))
        # 大 struct 定向访问 (高危布局模式)
        if has_big:
            bv = self.v()
            lines.append('    struct Big %s;' % bv)
            assigned = r.sample(range(self.big_nf), min(4, self.big_nf))
            for bi in assigned:
                ftype = self.big_flds[bi].split()[0]  # fix 2026-08-25: char 字段只用 CHAR_VAL(0..127), 避免 qcc 无符号char 255 vs gcc 有符号 -24 假差异
                val = r.choice(CHAR_VAL) if ftype == 'char' else r.choice(SAFE_VAL)
                lines.append('    %s.f%d = %d;' % (bv, bi, val))
            m.append(('    printf("%d\\n", (int)sizeof(struct Big));', 'int'))
            idx = r.sample(assigned, min(3, len(assigned)))  # 从已赋值字段选 (防未初始化读取 UB, 2026-08-25)
            m.append(('    printf("%%d\\n", %s);' % ' + '.join('%s.f%d' % (bv, i) for i in idx), 'int'))
        m.append(('    return 0;', None))
        for text, _ in m:
            lines.append(text)
        lines.append('}')
        return '\n'.join(lines)

    # ---- ctrl 族: 控制流 + 递归 + fnptr 调用 ----
    def gen_ctrl(self):
        r = self.r
        lines = ['#include <stdio.h>']
        # 递归 (限 5..12 防指数爆炸)
        has_fib = r.random() < 0.6
        if has_fib:
            lines.append('int fib(int n) { if (n < 2) return n; return fib(n - 1) + fib(n - 2); }')
        # fnptr 调用
        lines.append('int dblx(int x) { return x * 2; }')
        lines.append('int add3(int a, int b, int c) { return a + b + c; }')
        lines.append('int main(void) {')
        m = []
        # if/else
        for _ in range(2):
            a = r.choice(SAFE_VAL); b = r.choice(SAFE_VAL)
            op = r.choice(['<', '>', '<=', '>=', '==', '!='])
            m.append(('    if (%d %s %d) printf("T\\n"); else printf("F\\n");' % (a, op, b), 'int'))
        # while
        w = self.v()
        lines.append('    int %s = %d;' % (w, r.randrange(0, 5)))
        m.append(('    while (%s > 0) { printf("%%d\\n", %s); %s = %s - 1; }' % (w, w, w, w), 'int'))
        # for
        k = self.v(); s = self.v()
        lines.append('    int %s = 0;' % s)
        lines.append('    for (int %s = 0; %s < %d; %s = %s + 1) %s = %s + %s;' % (
            k, k, r.randrange(1, 5), k, k, s, s, k))
        m.append(('    printf("%%d\\n", %s);' % s, 'int'))
        # switch
        sw = self.v()
        lines.append('    int %s = %d;' % (sw, r.randrange(0, 4)))
        m.append(('    switch (%s) { case 0: printf("c0\\n"); break; case 1: printf("c1\\n"); break; '
                  'case 2: printf("c2\\n"); break; default: printf("cd\\n"); }' % sw, 'int'))
        # goto
        if r.random() < 0.5:
            m.append(('    goto skip_%d;' % self.vn, None))
            m.append(('    printf("no\\n");', None))
            m.append(('    skip_%d: printf("yes\\n");' % self.vn, 'int'))
        # 三元 + 短路
        t = self.v()
        lines.append('    int %s = %d;' % (t, r.choice(SAFE_VAL)))
        m.append(('    printf("%%d\\n", %s > 0 ? %s * 2 : %s + 100);' % (t, t, t), 'int'))
        # 递归
        if has_fib:
            m.append(('    printf("%%d\\n", fib(%d));' % r.randrange(5, 13), 'int'))
        # fnptr
        m.append(('    int (*fp)(int) = dblx; printf("%%d\\n", fp(%d));' % r.choice(SAFE_VAL), 'int'))
        m.append(('    printf("%%d\\n", add3(%d, %d, %d));' % (
            r.choice(SAFE_VAL), r.choice(SAFE_VAL), r.choice(SAFE_VAL)), 'int'))
        m.append(('    return 0;', None))
        for text, _ in m:
            lines.append(text)
        lines.append('}')
        return '\n'.join(lines)

    # ---- expr 族: 宽度与符号语义 (保留 v1 增强 + 修复) ----
    def gen_expr(self):
        r = self.r
        lines = ['#include <stdio.h>']
        lines.append('int main(void) {')
        m = []
        # 有符号一元负/正
        for _ in range(2):
            e, ty = self.int_expr(1)
            m.append(('    printf("%%d\\n", -(%s));' % e, 'int'))
        # unsigned 与 int 混合 (C 提升规则)
        u = self.v(); i = self.v()
        lines.append('    unsigned %s = %d;' % (u, r.choice(UINT32_EDGE[3:5])))
        lines.append('    int %s = %d;' % (i, r.choice([-1, -5, 3, 7])))
        m.append(('    printf("%%u\\n", %s + %s);' % (u, i), 'uint'))
        m.append(('    printf("%%d\\n", (%s + %s) > 0);' % (u, i), 'int'))
        # 除法/取模符号
        for _ in range(2):
            a = r.choice([-7, -5, 7, 5, 10])
            b = r.choice([2, 3, -2, -3])
            m.append(('    printf("%%d\\n", %d / %d);' % (a, b), 'int'))
            m.append(('    printf("%%d\\n", %d %% %d);' % (a, b), 'int'))
        # 移位 (0..31, 非负左操作数避免 UB)
        for _ in range(2):
            a = r.choice([1, 2, 7, 0x100])
            s = r.randrange(0, 32)
            m.append(('    printf("%%d\\n", %d << %d);' % (a, s), 'int'))
            m.append(('    printf("%%d\\n", %d >> %d);' % (a, s), 'int'))
        # 位运算 (符号扩展测试: 负数 & 0xFF)
        m.append(('    printf("%d\\n", (-5) & 0xFF);', 'int'))
        m.append(('    printf("%d\\n", (-5) | 0xF0);', 'int'))
        m.append(('    printf("%x\\n", (-5) & 0xFF);', 'int'))
        # 逻辑短路 (左操作数确定结果, 右操作数无副作用 — 避免 UB)
        m.append(('    printf("%d\\n", (1 || 0) ? 7 : 9);', 'int'))
        m.append(('    printf("%d\\n", (0 && 1) ? 7 : 9);', 'int'))
        # 比较
        m.append(('    printf("%d\\n", (0xFFFFFFFF > 0));', 'int'))  # unsigned vs signed 提升
        m.append(('    printf("%d\\n", (int)0xFFFFFFFF > 0);', 'int'))
        # long long 混合
        m.append(('    printf("%lld\\n", (long long)0x80000000 + (long long)0x80000000);', 'll'))
        m.append(('    printf("%lld\\n", 0xFFFFFFFFLL + 1LL);', 'll'))
        # double 转 int (已知 bug 族: cvttsd2si)
        if r.random() < 0.7:
            d = self.v()
            lines.append('    double %s = %d.5;' % (d, r.randrange(0, 10)))
            m.append(('    printf("%%d\\n", (int)%s);' % d, 'int'))
            m.append(('    printf("%%lld\\n", (long long)%s);' % d, 'll'))
        m.append(('    return 0;', None))
        for text, _ in m:
            lines.append(text)
        lines.append('}')
        return '\n'.join(lines)

    def gen_program(self, family):
        if family == 'types':
            return self.gen_types()
        if family == 'vararg':
            return self.gen_vararg()
        if family == 'layout':
            return self.gen_layout()
        if family == 'ctrl':
            return self.gen_ctrl()
        if family == 'expr':
            return self.gen_expr()
        raise ValueError('unknown family: %s' % family)


# ---------------- 运行器 (分类 + 归档 + fail-closed) ----------------

def compile_one(compiler, src, exe, qcc_args):
    try:
        r = subprocess.run([compiler, '-o', exe, src] + qcc_args,
                           capture_output=True, timeout=120)
    except subprocess.TimeoutExpired:
        return 'compile_timeout', '', ''
    if r.returncode != 0:
        return 'compile_fail', r.stdout.decode('utf-8', 'replace'), r.stderr.decode('utf-8', 'replace')
    return 'ok', '', ''


def run_one(exe, cwd):
    try:
        # cmd /c 启动: Python 直接 CreateProcess qcc 生成 PE 会 0xC0000005 (run_tests 教训 2026-08-08)
        p = subprocess.run(['cmd', '/c', exe], capture_output=True, timeout=8, cwd=cwd)
        out = p.stdout.decode('utf-8', 'replace').replace('\r\n', '\n')
        return 'ok', out, p.returncode
    except subprocess.TimeoutExpired:
        return 'run_timeout', '<TIMEOUT>', -999
    except Exception as e:
        return 'run_err', '<EXC:%s>' % e, -998


def classify_and_test(src_path, seed, family, qcc, qcc_args):
    """编译+运行+比对, 返回 (分类, 详情)"""
    q_exe = os.path.join(OUT_DIR, 't%d_%s_q.exe' % (seed, family))
    g_exe = os.path.join(OUT_DIR, 't%d_%s_g.exe' % (seed, family))
    # qcc 编译
    st, so, se = compile_one(qcc, src_path, q_exe, qcc_args)
    if st != 'ok':
        # gcc 是否也拒绝? 都拒 = skip; 仅 qcc 拒 = 疑似 bug (合法 C 编不了)
        gst, gso, gse = compile_one('gcc', src_path, g_exe, ['-O0', '-w'])
        if gst != 'ok':
            return 'skip', 'both compile fail'
        return 'qcc_compile_fail', (se or so)[-400:]
    # gcc 编译
    gst, gso, gse = compile_one('gcc', src_path, g_exe, ['-O0', '-w'])
    if gst != 'ok':
        return 'gcc_fail_only', (gse or gso)[-200:]
    # 运行
    qst, q_out, q_rc = run_one(q_exe, OUT_DIR)
    gst, g_out, g_rc = run_one(g_exe, OUT_DIR)
    if q_rc == -999 or g_rc == -999:
        return 'run_timeout', 'q_rc=%d g_rc=%d' % (q_rc, g_rc)
    if q_out == g_out and q_rc == g_rc:
        return 'pass', ''
    # 归档差异 (含输出快照)
    with open(os.path.join(OUT_DIR, 'fail_%d_%s.out.txt' % (seed, family)), 'w', encoding='utf-8') as f:
        f.write('qcc out:\n%s\nqcc rc=%d\n\ngcc out:\n%s\ngcc rc=%d\n' % (q_out, q_rc, g_out, g_rc))
    return 'diff', 'qcc: rc=%d out=%r | gcc: rc=%d out=%r' % (q_rc, q_out[:120], g_rc, g_out[:120])


def minimize(src_path, seed, family, qcc, qcc_args, max_passes=6):
    """简单行级最小化: 删掉一条 main 语句行, 若仍 DIFF 则保留删减 (delta 调试)"""
    lines = open(src_path, encoding='utf-8').read().split('\n')
    changed = True
    passes = 0
    while changed and passes < max_passes:
        changed = False
        passes += 1
        for i in range(len(lines) - 1, -1, -1):
            l = lines[i]
            if not l.strip() or l.strip().startswith('#'):
                continue
            cand = lines[:i] + lines[i + 1:]
            tmp = os.path.join(OUT_DIR, '_min_tmp.c')
            with open(tmp, 'w', encoding='utf-8') as f:
                f.write('\n'.join(cand))
            cat, det = classify_and_test(tmp, seed, family, qcc, qcc_args)
            if cat == 'diff':
                lines = cand
                changed = True
    with open(src_path, 'w', encoding='utf-8') as f:
        f.write('\n'.join(lines))


def main():
    if sys.stdout.encoding and sys.stdout.encoding.lower() not in ('utf-8', 'utf8'):
        sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')
    args = sys.argv[1:]
    only_seeds = None
    if '--only' in args:
        oi = args.index('--only')
        only_seeds = [int(x) for x in args[oi + 1].split(',') if x.strip().isdigit()]
        args = args[:oi] + args[oi + 2:]  # 移除 --only 与值, 兼容任意位置
    rounds = int(args[0]) if len(args) > 0 and args[0].isdigit() else 100
    start_seed = int(args[1]) if len(args) > 1 and args[1].isdigit() else 1
    n_seeds = int(args[2]) if len(args) > 2 and args[2].isdigit() else 1
    families = ['types', 'vararg', 'layout', 'ctrl', 'expr']
    qcc = DEFAULT_QCC
    do_minimize = False
    i = 3
    while i < len(args):
        if args[i] == '--family':
            families = [f for f in args[i + 1].split(',') if f in FAMILIES]
            i += 2
        elif args[i] == '--qcc':
            qcc = os.path.join(ROOT, args[i + 1])
            i += 2
        elif args[i] == '--minimize':
            do_minimize = True
            i += 1
        else:
            i += 1
    if not os.path.exists(qcc):
        print('[ERR] qcc not found: %s' % qcc)
        sys.exit(2)

    qcc_args = ['-D', '__STDC_VERSION__=199901L', '-D', '__GNUC__=5',
                '-D', '_WIN32=1', '-D', 'REG_STARTEND=1', '-I', PRELUDE]

    stats = {'pass': 0, 'diff': [], 'skip': 0, 'gcc_fail_only': 0,
             'qcc_compile_fail': 0, 'run_timeout': 0}
    tasks = []
    if only_seeds:
        # 增量模式: 重跑之前 DIFF 的 seed (从 D 盘读已保存 .c 用例, ~2-3 分钟)
        print('[增量] 重跑 seed:', only_seeds)
        for seed in only_seeds:
            prefix = 't%d_' % seed
            for f in sorted(os.listdir(OUT_DIR)):
                if f.startswith(prefix) and f.endswith('.c'):
                    fam = f[len(prefix):-2]
                    tasks.append((os.path.join(OUT_DIR, f), seed, fam))
    else:
        for s in range(n_seeds):
            for rnd in range(rounds):
                seed = start_seed + s * 100000 + rnd  # 每轮唯一 seed (可复现)
                family = random.Random(seed).choice(families)
                rng = random.Random(seed)  # 该 seed 的确定性生成器
                prog = Gen(rng).gen_program(family)
                src_path = os.path.join(OUT_DIR, 't%d_%s.c' % (seed, family))
                with open(src_path, 'w', encoding='utf-8') as f:
                    f.write(prog)
                tasks.append((src_path, seed, family))
    total = len(tasks)
    done = 0
    for src_path, seed, family in tasks:
        cat, det = classify_and_test(src_path, seed, family, qcc, qcc_args)
        # 清理本轮编译产物 (qcc exe ~256MB/个), 防止 D 盘爆盘 (fix 2026-08-25)
        for _p in ('t%d_%s_q.exe' % (seed, family), 't%d_%s_g.exe' % (seed, family)):
            try:
                os.remove(os.path.join(OUT_DIR, _p))
            except OSError:
                pass
        done += 1
        if cat == 'pass':
            stats['pass'] += 1
        elif cat == 'skip':
            stats['skip'] += 1
        elif cat == 'gcc_fail_only':
            stats['gcc_fail_only'] += 1
        elif cat == 'qcc_compile_fail':
            stats['qcc_compile_fail'] += 1
            print('[qcc_compile_fail] seed=%d fam=%s' % (seed, family))
            print(prog[:400])
        elif cat == 'run_timeout':
            stats['run_timeout'] += 1
        else:  # diff
            stats['diff'].append((seed, family, det))
            if do_minimize:
                minimize(src_path, seed, family, qcc, qcc_args)
            print('[DIFF] seed=%d fam=%s %s' % (seed, family, det[:200]))
            print('  saved: %s' % src_path)
        if done % 50 == 0:
            print('... %d/%d done, pass=%d diff=%d' % (
                done, total, stats['pass'], len(stats['diff'])), flush=True)

    print('\n=== 差异测试台 v2: %d 个程序 (%d 轮 x %d seed) 族=%s ===' % (
        total, rounds, n_seeds, ','.join(families)))
    print('PASS: %d  skip: %d  gcc_fail_only: %d  qcc_compile_fail: %d  run_timeout: %d' % (
        stats['pass'], stats['skip'], stats['gcc_fail_only'],
        stats['qcc_compile_fail'], stats['run_timeout']))
    n_diff = len(stats['diff'])
    print('真实差异: %d 个' % n_diff)
    for seed, family, det in stats['diff'][:12]:
        print('  - seed=%d fam=%s' % (seed, family))
    # fail-closed: 真差异 > 0 则退出码非零
    sys.exit(1 if n_diff > 0 else 0)


if __name__ == '__main__':
    main()
