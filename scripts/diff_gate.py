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

FAMILIES = ['types', 'vararg', 'layout', 'ctrl', 'expr', 'alignas', 'ginit', 'strlib', 'math', 'vla']

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

    # ---- alignas 族: step14-21 新特性普检 ----
    # _Alignas 对齐语义 (step21) / &arr[i]-&arr[j] 指针差 + *(p+1) 解引用宽度 (step20)
    # 语句表达式 (step19) / GNU case 范围 (step15) / return 三元 double (step14)
    def gen_alignas(self):
        r = self.r
        lines = ['#include <stdio.h>', '#include <stddef.h>']
        m = []
        # ---- _Alignas 字段对齐 (step21): N 只增不减, 避免 gcc "cannot reduce" 约束违规 ----
        a1 = r.choice([4, 8, 16])
        lines.append('struct AG1 { _Alignas(%d) char c; int i; };' % a1)
        m.append(('    printf("%d\\n", (int)sizeof(struct AG1));', 'int'))
        m.append(('    printf("%d\\n", (int)((char*)&((struct AG1*)0)->i - (char*)0));', 'int'))
        a2 = r.choice([4, 8])
        lines.append('struct AG2 { char c; _Alignas(%d) short s; };' % a2)
        m.append(('    printf("%d\\n", (int)sizeof(struct AG2));', 'int'))
        m.append(('    printf("%d\\n", (int)((char*)&((struct AG2*)0)->s - (char*)0));', 'int'))
        lines.append('struct AG3 { _Alignas(8) char a; _Alignas(4) short b; char c; };')
        m.append(('    printf("%d\\n", (int)sizeof(struct AG3));', 'int'))
        m.append(('    printf("%d\\n", (int)((char*)&((struct AG3*)0)->b - (char*)0));', 'int'))
        # 对齐 struct 实例读写 (验证字段偏移正确不崩)
        g1 = self.v()
        pre_defs = []  # 函数定义收集 — 必须在 main 之前 (fix 2026-08-25: 原 rtdbl append 进 main 体内 → C 无嵌套函数 → qcc 空输出)
        lines.append('int main(void) {')
        lines.append('    struct AG1 %s;' % g1)
        lines.append('    %s.c = 5; %s.i = %d;' % (g1, g1, r.choice(SAFE_VAL)))
        m.append(('    printf("%%d\\n", %s.c + %s.i);' % (g1, g1), 'int'))
        # 多 _Alignas struct 数组步长 (sizeof round-up 一致性)
        aga = self.v()
        lines.append('    struct AG2 %s[3];' % aga)
        lines.append('    %s[0].s = 7; %s[2].s = 9;' % (aga, aga))
        m.append(('    printf("%%d\\n", %s[0].s + %s[2].s);' % (aga, aga), 'int'))
        # ---- &arr[i]-&arr[j] 指针差 (step20): int/double/struct 元素 ----
        ia = self.v(); da = self.v()
        lines.append('    int %s[8];' % ia)
        lines.append('    double %s[6];' % da)
        for k in range(4):
            lines.append('    %s[%d] = %d;' % (ia, k, r.choice(SAFE_VAL)))
            lines.append('    %s[%d] = %d.5;' % (da, k, r.choice(SAFE_VAL)))
        m.append(('    printf("%%d\\n", (int)(&%s[5] - &%s[2]));' % (ia, ia), 'int'))
        m.append(('    printf("%%d\\n", (int)(&%s[5] - &%s[0]));' % (ia, ia), 'int'))
        m.append(('    printf("%%d\\n", (int)(&%s[2] - &%s[5]));' % (ia, ia), 'int'))
        m.append(('    printf("%%d\\n", (int)(&%s[5] - &%s[2]));' % (da, da), 'int'))
        m.append(('    printf("%%d\\n", (int)(&%s[5] - &%s[4]));' % (da, da), 'int'))
        # 变量索引 + 混合变量指针差
        ip = self.v()
        lines.append('    int *%s = &%s[2];' % (ip, ia))
        m.append(('    printf("%%d\\n", (int)(%s - &%s[0]));' % (ip, ia), 'int'))
        m.append(('    printf("%%d\\n", (int)(&%s[5] - %s));' % (ia, ip), 'int'))
        # ---- *(p+1) 指针算术解引用宽度 (step20) ----
        lines.append('    %s[3] = 123;' % ia)
        lines.append('    %s[4] = 456;' % da)
        m.append(('    printf("%%d\\n", *(%s + 1));' % ip, 'int'))
        m.append(('    printf("%%d\\n", *(&%s[2] + 1));' % ia, 'int'))
        m.append(('    printf("%%d\\n", *(int*)((char*)&%s[2] + 4));' % ia, 'int'))
        # 64 位指针差路径
        m.append(('    printf("%%lld\\n", (long long)(&%s[5] - &%s[2]));' % (ia, ia), 'll'))
        m.append(('    printf("%%lld\\n", (long long)(&%s[5] - &%s[1]));' % (da, da), 'll'))
        # ---- 语句表达式 (step19) ----
        if r.random() < 0.8:
            sx = self.v()
            lines.append('    int %s = 3;' % sx)
            m.append(('    printf("%%d\\n", ({ int t = %s * 2; t + 1; }));' % sx, 'int'))
            m.append(('    printf("%d\\n", ({ int u = 7; int v = 9; u > v ? u : v; }));', 'int'))
            m.append(('    printf("%%d\\n", ({ int t = %s; t; }) * 3);' % sx, 'int'))
        # ---- GNU case 范围 (step15) ----
        if r.random() < 0.7:
            cv = self.v()
            lines.append('    int %s = %d;' % (cv, r.choice([1, 3, 5, 9])))
            lines.append('    int %s_r = 0;' % cv)
            lines.append('    switch (%s) {' % cv)
            lines.append('    case 1 ... 3: %s_r = 10; break;' % cv)
            lines.append('    case 5 ... 8: %s_r = 20; break;' % cv)
            lines.append('    default: %s_r = 30; break;' % cv)
            lines.append('    }')
            m.append(('    printf("%%d\\n", %s_r);' % cv, 'int'))
        # ---- return 三元 double (step14) ----
        if r.random() < 0.8:
            pre_defs.append('double rtdbl(int x) { return x > 0 ? 1.5 : -1.5; }')
            rv = self.v()
            lines.append('    int %s = %d;' % (rv, r.choice([-1, 2, 0])))
            m.append(('    printf("%%.1f\\n", rtdbl(%s));' % rv, 'dbl'))
        m.append(('    return 0;', None))
        for text, _ in m:
            lines.append(text)
        lines.append('}')
        # pre_defs 插到 main 之前 (rtdbl 等辅助函数定义)
        out = []
        for ln in lines:
            if ln == 'int main(void) {':
                out.extend(pre_defs)
            out.append(ln)
        return '\n'.join(out)

    # ---- ginit 族: 静态数组初始化普检 (step25: 覆盖 ginit 路径 — step24 曾现 9 元素 static const double 崩溃) ----
    def gen_ginit(self):
        r = self.r
        lines = ['#include <stdio.h>']
        m = []
        def arr_vals(ty, n):
            if ty == 'double':
                if r.random() < 0.5:
                    return ', '.join('%d.5' % (i + 1) for i in range(n))
                return ', '.join('%d.%dE-%d' % (i + 1, (i * 7) % 10, (i % 5) + 1) for i in range(n))
            if ty == 'char':
                return ', '.join(str(i % 127) for i in range(n))
            if ty == 'long long':
                return ', '.join('%dLL' % ((i + 1) * 1000) for i in range(n))
            if ty == 'short':
                return ', '.join(str((i + 1) * 3) for i in range(n))
            return ', '.join(str((i + 1) * 7) for i in range(n))
        # 全局静态数组: const/非 const × 各类型 × 大小 1..250 (跨 arr_chain 块链 200 阈值)
        gnames = []  # (nm, kind, n)
        for k in range(r.randrange(1, 4)):
            n = r.randrange(1, 250)
            ty = r.choice(['int', 'double', 'char', 'long long', 'short'])
            const = r.choice(['', 'const '])
            nm = 'GA%d' % k
            gnames.append((nm, ty, n))
            lines.append('static %s%s %s[] = { %s };' % (const, ty, nm, arr_vals(ty, n)))
        # struct 数组 (brace_fields 初始化)
        has_struct = False
        if r.random() < 0.7:
            lines.append('struct GS { int a; int b; };')
            has_struct = True
            n = r.randrange(1, 20)
            vals = ', '.join('{%d, %d}' % (i + 1, i * 3) for i in range(n))
            lines.append('static struct GS SA[] = { %s };' % vals)
            gnames.append(('SA', 'struct', n))
        # 零初始化全局数组 (含 struct 数组)
        for k in range(r.randrange(0, 3)):
            n = r.randrange(1, 12)
            if has_struct and r.random() < 0.5:
                lines.append('static struct GS ZS%d[%d];' % (k, n))
                gnames.append(('ZS%d' % k, 'zs', n))
            else:
                ty = r.choice(['int', 'double', 'char'])
                lines.append('static %s ZB%d[%d];' % (ty, k, n))
                gnames.append(('ZB%d' % k, ty, n))
        # 静态 struct 变量 (brace_fields 单 struct)
        if has_struct and r.random() < 0.6:
            lines.append('static struct GS SV = { %d, %d };' % (r.randrange(1, 50), r.randrange(1, 50)))
            gnames.append(('SV', 'svar', 1))
        # 静态指针数组 (const char *arr[])
        if r.random() < 0.5:
            n = r.randrange(2, 6)
            names = ['"aa"', '"bb"', '"cc"', '"dd"', '"ee"', '"ff"', '"gg"']
            lines.append('static const char *PSTR[] = { %s };' % ', '.join(names[:n]))
            gnames.append(('PSTR', 'ptr', n))
        # 字符串二维数组 char arr[][4]
        if r.random() < 0.5:
            n = r.randrange(2, 5)
            names = ['"ab"', '"cd"', '"ef"', '"gh"', '"ij"']
            lines.append('static char CHARR[][4] = { %s };' % ', '.join(names[:n]))
            gnames.append(('CHARR', 'str', n))
        # 多维 int 数组 (2D brace_arr_init 递归)
        if r.random() < 0.5:
            d1 = r.randrange(1, 5); d2 = r.randrange(1, 6)
            vals = ', '.join(str((i % 7) + 1) for i in range(d1 * d2))
            lines.append('static int M2D[%d][%d] = { %s };' % (d1, d2, vals))
            gnames.append(('M2D', 'int2d', d1, d2))
        # 函数内 static 数组
        if r.random() < 0.8:
            lines.append('double gfn(int i) {')
            for k in range(r.randrange(1, 3)):
                n = r.randrange(1, 16)
                lines.append('    static const double F%d[] = { %s };' % (k, arr_vals('double', n)))
            lines.append('    return F0[i % 4];')
            lines.append('}')
        # main: 读元素验证 (含数组间无污染检查)
        lines.append('int main(void) {')
        for item in gnames:
            nm = item[0]; ty = item[1]; n = item[2]
            if ty == 'int2d':
                d1 = item[2]; d2 = item[3]
                m.append(('    printf("%%d\\n", %s[%d][%d]);' % (nm, r.randrange(0, d1), r.randrange(0, d2)), 'int'))
            elif ty == 'svar':
                m.append(('    printf("%%d\\n", %s.a + %s.b);' % (nm, nm), 'int'))
            elif ty == 'ptr':
                m.append(('    printf("%%d\\n", %s[%d][0]);' % (nm, r.randrange(0, n)), 'int'))
            elif ty == 'str':
                m.append(('    printf("%%d\\n", %s[%d][0]);' % (nm, r.randrange(0, n)), 'int'))
            elif ty == 'zs':
                m.append(('    printf("%%d\\n", %s[%d].a + %s[%d].b);' % (nm, r.randrange(0, n), nm, r.randrange(0, n)), 'int'))
            elif ty == 'struct':
                m.append(('    printf("%%d\\n", %s[%d].a + %s[%d].b);' % (nm, r.randrange(0, n), nm, r.randrange(0, n)), 'int'))
            elif ty == 'double':
                m.append(('    printf("%%.9g\\n", %s[%d]);' % (nm, r.randrange(0, n)), 'dbl'))
            elif ty == 'long long':
                m.append(('    printf("%%lld\\n", %s[%d]);' % (nm, r.randrange(0, n)), 'll'))
            elif ty == 'char':
                m.append(('    printf("%%d\\n", %s[%d]);' % (nm, r.randrange(0, n)), 'int'))
            elif ty == 'short':
                m.append(('    printf("%%d\\n", %s[%d]);' % (nm, r.randrange(0, n)), 'int'))
            else:
                m.append(('    printf("%%d\\n", %s[%d]);' % (nm, r.randrange(0, n)), 'int'))
        if 'gfn' in lines[-1] and r.random() < 0.7:
            m.append(('    printf("%%.9g\\n", gfn(2));', 'dbl'))
        m.append(('    return 0;', None))
        for text, _ in m:
            lines.append(text)
        lines.append('}')
        return '\n'.join(lines)

    # ---- strlib 族: 字符串库函数普检 (string 30+ 函数 — diff_gate 此前未覆盖) ----
    def gen_strlib(self):
        r = self.r
        lines = ['#include <stdio.h>', '#include <string.h>']
        m = []
        pool = ['"hello world"', '"abc"', '"xyz"', '""', '"a"', '"hello"', '"world"', '"hell"', '"heaven"', '"HELLO"', '"ababab"', '"  spaced  "']
        s1 = r.choice(pool)
        s2 = r.choice(pool)
        ch = r.choice(['a', 'l', 'o', 'x', 'h', 'e', ' '])
        n = r.randrange(0, 12)
        lines.append('char BUF[64];')
        lines.append('char BUF2[64];')
        lines.append('int main(void) {')
        lines.append('    const char *s1 = %s;' % s1)
        lines.append('    const char *s2 = %s;' % s2)
        # strlen
        m.append(('    printf("%d\\n", (int)strlen(s1));', 'int'))
        # strcpy + strlen
        lines.append('    strcpy(BUF, s1);')
        m.append(('    printf("%s|%d\\n", BUF, (int)strlen(BUF));', 'str'))
        # strncpy (手动 NUL) + strncat
        lines.append('    strncpy(BUF, s2, %d); BUF[%d] = 0;' % (n, n))
        m.append(('    printf("%s\\n", BUF);', 'str'))
        lines.append('    strcpy(BUF, s1);')
        lines.append('    strncat(BUF, s2, %d);' % n)
        m.append(('    printf("%s\\n", BUF);', 'str'))
        # strcat
        lines.append('    strcpy(BUF, s1);')
        lines.append('    strcat(BUF, s2);')
        m.append(('    printf("%s\\n", BUF);', 'str'))
        # strcmp / strncmp 三元比较
        m.append(('    printf("%d\\n", strcmp(s1, s2) > 0 ? 1 : (strcmp(s1, s2) < 0 ? -1 : 0));', 'int'))
        m.append(('    printf("%%d\\n", strncmp(s1, s2, %d) > 0 ? 1 : (strncmp(s1, s2, %d) < 0 ? -1 : 0));' % (n, n), 'int'))
        # strchr / strrchr (偏移或 -1)
        m.append(('    printf("%%d\\n", strchr(s1, \'%c\') ? (int)(strchr(s1, \'%c\') - s1) : -1);' % (ch, ch), 'int'))
        m.append(('    printf("%%d\\n", strrchr(s1, \'%c\') ? (int)(strrchr(s1, \'%c\') - s1) : -1);' % (ch, ch), 'int'))
        # strstr / strcspn / strspn
        m.append(('    printf("%d\\n", strstr(s1, s2) ? (int)(strstr(s1, s2) - s1) : -1);', 'int'))
        m.append(('    printf("%d\\n", (int)strcspn(s1, s2));', 'int'))
        m.append(('    printf("%d\\n", (int)strspn(s1, "aeiou h"));', 'int'))
        # strtok (每次重置缓冲)
        lines.append('    strcpy(BUF, s1);')
        m.append(('    printf("%s\\n", strtok(BUF, " ,"));', 'str'))
        # memcpy / memmove / memchr / memset
        lines.append('    memcpy(BUF, s1, strlen(s1) + 1);')
        m.append(('    printf("%d\\n", (int)strlen(BUF));', 'int'))
        lines.append('    memmove(BUF + 1, BUF, strlen(BUF) + 1);')
        m.append(('    printf("%d\\n", (int)strlen(BUF));', 'int'))
        m.append(('    printf("%%d\\n", memchr(s1, \'%c\', strlen(s1)) ? (int)((char*)memchr(s1, \'%c\', strlen(s1)) - s1) : -1);' % (ch, ch), 'int'))
        lines.append('    memset(BUF, %d, 6); BUF[6] = 0;' % r.randrange(65, 90))
        m.append(('    printf("%s\\n", BUF);', 'str'))
        # strdup (mingw 无 strndup, 只用 strdup)
        lines.append('    char *dup = strdup(s1);')
        m.append(('    printf("%s\\n", dup);', 'str'))
        m.append(('    return 0;', None))
        for text, _ in m:
            lines.append(text)
        lines.append('}')
        return '\n'.join(lines)

    # ---- math 族: 数学函数随机覆盖 (step24 单独验证 41 项, 随机化补盲) ----
    def gen_math(self):
        r = self.r
        lines = ['#include <stdio.h>', '#include <math.h>', 'int main(void) {']
        m = []
        unary1 = ['erf', 'erfc', 'lgamma', 'tgamma', 'log2', 'exp2', 'cbrt', 'log1p', 'expm1',
                  'round', 'trunc', 'floor', 'ceil', 'fabs', 'sqrt', 'log', 'log10', 'exp',
                  'cos', 'sin', 'tan', 'cosh', 'sinh', 'tanh']
        binary = ['fmax', 'fmin', 'fdim', 'hypot', 'atan2', 'fmod', 'copysign', 'pow']
        # 值池: 避开 sqrt/log 负数输入; lgamma/tgamma 避开 0 与负整数 (极点);
        # 半整数可测反射公式; pow 指数随机整数 (负底安全).
        v_pos = ['0.25', '0.5', '1.0', '1.5', '2.0', '3.5', '7.25', '10.0', '0.01', '100.0']
        v_mix = ['-3.5', '-2.5', '-1.5', '-0.5', '0.5', '1.5', '2.5', '3.5', '5.0', '7.5', '-5.0', '0.25', '-0.25']
        v_lt = ['-3.5', '-2.5', '-1.5', '-0.5', '0.5', '1.5', '2.5', '3.5', '5.0', '7.5', '-5.0']
        for _ in range(r.randrange(6, 12)):
            fn = r.choice(unary1)
            if fn in ('sqrt', 'log', 'log10', 'log2', 'log1p'):
                v = r.choice(v_pos)
            elif fn in ('lgamma', 'tgamma'):
                v = r.choice(v_lt)
            else:
                v = r.choice(v_pos if r.random() < 0.5 else v_mix)
            m.append(('    printf("%%.9g\\n", %s(%s));' % (fn, v), 'dbl'))
        for _ in range(r.randrange(2, 6)):
            fn = r.choice(binary)
            a = r.choice(v_mix)
            b = r.choice(v_mix)
            if fn == 'pow' and r.random() < 0.5:
                b = r.choice(['2', '3', '-2', '-3', '0'])  # 整数指数
            m.append(('    printf("%%.9g\\n", %s(%s, %s));' % (fn, a, b), 'dbl'))
        m.append(('    return 0;', None))
        for text, _ in m:
            lines.append(text)
        lines.append('}')
        return '\n'.join(lines)

    # ---- vla 族: 变长数组 (step31 实现后随机化覆盖; 一维 + 表达式维度 + 传参) ----
    def gen_vla(self):
        r = self.r
        lines = ['#include <stdio.h>']
        m = []
        # 传参求和函数 (VLA 参数衰减成指针)
        lines.append('static int vsum(int n, int arr[n]) { int s = 0, i; for (i = 0; i < n; i++) s += arr[i]; return s; }')
        lines.append('static int vsumc(char *p, int n) { int s = 0, i; for (i = 0; i < n; i++) s += p[i]; return s; }')
        lines.append('int main(void) {')
        for k in range(r.randrange(2, 4)):
            n = r.randrange(2, 9)
            m1 = r.randrange(1, 4)
            m2 = r.randrange(1, 4)
            ty = r.choice(['int', 'char', 'double', 'long long'])
            nm = 'VA%d' % k
            nk = 'n%d' % k  # 每 VLA 独立维度变量 (fix: 原共名 n 重复定义)
            # 维度: 变量 nk 或表达式 nk+m 或 m*nk (avoid nk-m → 越界写)
            dim = r.choice(['%s' % nk, '%s + %d' % (nk, m1), '%s * %d' % (nk, m2)])
            lines.append('    int %s = %d;' % (nk, n))
            lines.append('    %s %s[%s];' % (ty, nm, dim))
            if ty == 'char':
                lines.append('    for (int _i = 0; _i < %s; _i++) %s[_i] = (char)(_i %% 7 + 1);' % (nk, nm))
            elif ty == 'double':
                lines.append('    for (int _i = 0; _i < %s; _i++) %s[_i] = _i * 0.5;' % (nk, nm))
            elif ty == 'long long':
                lines.append('    for (int _i = 0; _i < %s; _i++) %s[_i] = (long long)_i * 1000;' % (nk, nm))
            else:
                lines.append('    for (int _i = 0; _i < %s; _i++) %s[_i] = _i * 3;' % (nk, nm))
            if ty == 'double':
                m.append(('    printf("%%.9g\\n", %s[%s - 1]);' % (nm, nk), 'dbl'))
            elif ty == 'long long':
                m.append(('    printf("%%lld\\n", %s[%s - 1]);' % (nm, nk), 'll'))
            elif ty == 'char':
                m.append(('    printf("%%d\\n", %s[%s - 1]);' % (nm, nk), 'int'))
            else:
                m.append(('    printf("%%d\\n", %s[%s - 1] + %s[0]);' % (nm, nk, nm), 'int'))
            if ty == 'int':
                m.append(('    printf("%%d\\n", vsum(%s, %s));' % (nk, nm), 'int'))
            elif ty == 'char':
                m.append(('    printf("%%d\\n", vsumc(%s, %s));' % (nm, nk), 'int'))
        # VLA 与普通变量混排 (地址不越界)
        lines.append('    int ng = 4;')
        lines.append('    int vb[ng];')
        lines.append('    int guard = 123;')
        lines.append('    for (int _i = 0; _i < ng; _i++) vb[_i] = _i;')
        lines.append('    printf("%d\\n", vb[3] + guard);')
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
        if family == 'alignas':
            return self.gen_alignas()
        if family == 'ginit':
            return self.gen_ginit()
        if family == 'strlib':
            return self.gen_strlib()
        if family == 'math':
            return self.gen_math()
        if family == 'vla':
            return self.gen_vla()
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
        gst, gso, gse = compile_one('gcc', src_path, g_exe, ['-O0', '-w', '-lm'])
        if gst != 'ok':
            return 'skip', 'both compile fail'
        return 'qcc_compile_fail', (se or so)[-400:]
    # gcc 编译 (math 族需要 -lm; 非 math 用例无害)
    gst, gso, gse = compile_one('gcc', src_path, g_exe, ['-O0', '-w', '-lm'])
    if gst != 'ok':
        return 'gcc_fail_only', (gse or gso)[-200:]
    # 运行
    qst, q_out, q_rc = run_one(q_exe, OUT_DIR)
    gst, g_out, g_rc = run_one(g_exe, OUT_DIR)
    if q_rc == -999 or g_rc == -999:
        return 'run_timeout', 'q_rc=%d g_rc=%d' % (q_rc, g_rc)
    # NaN 符号位是 IEEE 实现细节 (gcc tgamma(-1) 输出 'nan', qcc '-nan(ind)'; msvcrt vs qcc 位模式不同) — 归一化 (fix step29)
    def _norm_nan(s):
        import re as _re
        s = _re.sub(r'[-+]?nan\(ind\)', 'NAN', s)
        s = _re.sub(r'[-+]?nan\b', 'NAN', s)
        return s
    if _norm_nan(q_out) == _norm_nan(g_out) and q_rc == g_rc:
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
    families = ['types', 'vararg', 'layout', 'ctrl', 'expr', 'alignas', 'ginit', 'strlib', 'math', 'vla']
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
