# -*- coding: utf-8 -*-
"""qcc_x86 fuzz 测试 v2: 修正 struct 字段复用 + 函数实参个数 + 除数非零。
用法: python scripts/fuzz_qcc.py [轮数] [种子] [qcc|v2]
   第三个参数选编译器: qcc = qcc_x86 (宿主), v2 = 自宿主编译器 (攻镜像缺口用)。"""
import os, random, subprocess, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
QCC = os.path.join(ROOT, 'qcc_x86.exe')

class Gen:
    def __init__(self, rng):
        self.r = rng
        self.fn_count = 0
        self.fns = []          # (name, arity)

    def ident(self, base, used=set()):
        while True:
            nm = base + str(self.r.randrange(1000))
            if nm not in used:
                used.add(nm)
                return nm

    def int_lit(self, nonzero=False):
        choices = [1, 2, 3, 5, 7, 10, 100, 255, 1000]
        if nonzero:
            return str(self.r.choice(choices))
        return str(self.r.choice(choices + [0, -1, -5, -100]))

    def expr_int(self, depth=0, for_div=False):
        r = self.r
        if depth > 2 or r.random() < 0.4:
            return self.int_lit(nonzero=for_div)
        op = r.choice(['+', '-', '*', '/', '%', '&', '|', '^'])
        if op in ('/', '%'):
            # denominator must be nonzero: use a positive literal or guarded expr
            return '(%s %s %s)' % (self.expr_int(depth + 1), op, self.int_lit(nonzero=True))
        return '(%s %s %s)' % (self.expr_int(depth + 1), op, self.expr_int(depth + 1))

    def expr_cmp(self, depth=0):
        r = self.r
        op = r.choice(['<', '>', '<=', '>=', '==', '!='])
        return '(%s %s %s)' % (self.expr_int(depth + 1), op, self.expr_int(depth + 1))

    def expr_bool(self):
        r = self.r
        return r.choice([
            self.expr_cmp(),
            '(%s && %s)' % (self.expr_cmp(), self.expr_cmp()),
            '(%s || %s)' % (self.expr_cmp(), self.expr_cmp()),
            '!(%s)' % self.expr_cmp(),
        ])

    def gen_func(self):
        r = self.r
        self.fn_count += 1
        fname = 'fn%d' % self.fn_count
        arity = r.randrange(1, 3)
        self.fns.append((fname, arity))
        params = []
        pused = set()
        for _ in range(arity):
            params.append('int %s' % self.ident('p', pused))
        body = []
        vused = set(pused)
        acc = self.ident('s', vused)
        body.append('int %s = %s;' % (acc, self.expr_int()))
        loopnm = self.ident('i', vused)
        for _ in range(r.randrange(1, 3)):
            nm = self.ident('v', vused)
            vused.add(nm)
            body.append('int %s = %s;' % (nm, self.expr_int()))
        body.append('for (int %s = 0; %s < %s; %s = %s + 1) {' % (
            loopnm, loopnm, self.int_lit(nonzero=True), loopnm, loopnm))
        body.append('  %s = %s + (%s);' % (acc, acc, self.expr_int()))
        body.append('}')
        body.append('return %s;' % acc)
        return 'int %s(%s) {\n%s\n}\n' % (fname, ', '.join(params), '\n'.join(body))

    def gen_program(self):
        r = self.r
        parts = ['#include <stdio.h>']
        gsz = r.randrange(3, 9)
        parts.append('static int garr[%d] = {%s};' % (
            gsz, ', '.join(self.int_lit() for _ in range(gsz))))
        if r.random() < 0.5:
            parts.append('static long long gll = %s;' % self.int_lit())
        # struct: fields generated ONCE, reuse the SAME names
        sfields = []
        used_fields = set()
        for _ in range(r.randrange(1, 4)):
            ft = r.choice(['int', 'int', 'long long', 'char', 'double'])
            sfields.append((ft, self.ident('f', used_fields)))
        parts.append('struct S0 { %s };' % ' '.join('%s %s;' % f for f in sfields))
        helpers = [self.gen_func() for _ in range(r.randrange(0, 3))]
        parts.extend(helpers)
        main_lines = ['int main(void) {']
        used = set()
        # struct usage: pick one field, assign + read the SAME field
        if r.random() < 0.7 and sfields:
            sv = self.ident('st', used); used.add(sv)
            ft, fname = r.choice(sfields)
            main_lines.append('struct S0 %s;' % sv)
            if ft == 'char':
                # qcc char 是 unsigned 约定 — 用 0..127 避开符号扩展分歧
                main_lines.append('%s.%s = %d;' % (sv, fname, r.randrange(0, 128)))
            else:
                main_lines.append('%s.%s = %s;' % (sv, fname, self.int_lit()))
            if ft == 'int':
                main_lines.append('printf("%%d\\n", %s.%s);' % (sv, fname))
            elif ft == 'long long':
                main_lines.append('printf("%%lld\\n", %s.%s);' % (sv, fname))
            elif ft == 'char':
                main_lines.append('printf("%%d\\n", %s.%s);' % (sv, fname))
            else:  # double
                main_lines.append('printf("%%.1f\\n", %s.%s);' % (sv, fname))
        # local array: initialize ALL elements, then read any two (avoid uninitialized reads)
        ai = self.ident('a', used); used.add(ai)
        main_lines.append('int %s[4];' % ai)
        for i in range(4):
            main_lines.append('%s[%d] = %s;' % (ai, i, self.expr_int()))
        main_lines.append('printf("%%d\\n", %s[%d] + %s[%d]);' % (ai, r.randrange(4), ai, r.randrange(4)))
        # global array read
        main_lines.append('printf("%%d\\n", garr[%d]);' % r.randrange(0, gsz))
        # ll
        if r.random() < 0.6:
            ll = self.ident('x', used); used.add(ll)
            main_lines.append('long long %s = %s;' % (ll, self.int_lit()))
            main_lines.append('printf("%%lld\\n", %s * 2 + 1);' % ll)
        # if/else
        main_lines.append('if (%s) { printf("%%d\\n", %s); } else { printf("%%d\\n", %s + 1); }' % (
            self.expr_bool(), self.int_lit(), self.int_lit()))
        # while
        wl = self.ident('w', used); used.add(wl)
        main_lines.append('int %s = 0;' % wl)
        main_lines.append('while (%s < %s) { %s = %s + %s; }' % (
            wl, self.int_lit(), wl, wl, self.int_lit(nonzero=True)))
        main_lines.append('printf("%%d\\n", %s);' % wl)
        # for loop
        if r.random() < 0.6:
            fv = self.ident('k', used); used.add(fv)
            acc2 = self.ident('su', used); used.add(acc2)
            main_lines.append('int %s = 0;' % acc2)
            main_lines.append('for (int %s = 0; %s < %s; %s = %s + 1) { %s = %s + %s; }' % (
                fv, fv, self.int_lit(nonzero=True), fv, fv, acc2, acc2, self.int_lit()))
            main_lines.append('printf("%%d\\n", %s);' % acc2)
        # switch
        if r.random() < 0.6:
            sw = self.ident('sw', used); used.add(sw)
            main_lines.append('int %s = %s;' % (sw, self.int_lit()))
            case_v = self.int_lit()
            main_lines.append('switch (%s %% 4) { case 0: printf("%%d\\n", %s); break; case 1: printf("%%d\\n", %s + 1); break; case 2: printf("%%d\\n", %s + 2); break; default: printf("%%d\\n", %s + 3); }' % (
                sw, case_v, case_v, case_v, case_v))
        # 2D array
        if r.random() < 0.4:
            m2 = self.ident('m', used); used.add(m2)
            main_lines.append('int %s[2][3];' % m2)
            main_lines.append('%s[0][0] = %s;' % (m2, self.int_lit()))
            main_lines.append('%s[1][2] = %s;' % (m2, self.int_lit()))
            main_lines.append('printf("%%d\\n", %s[0][0] + %s[1][2]);' % (m2, m2))
        # unsigned arithmetic
        if r.random() < 0.5:
            uv = self.ident('u', used); used.add(uv)
            main_lines.append('unsigned %s = %s;' % (uv, self.int_lit()))
            main_lines.append('printf("%%u\\n", %s * 3);' % uv)
        # helper call with exact arity
        if self.fns:
            fname, arity = r.choice(self.fns)
            args = ', '.join(self.int_lit() for _ in range(arity))
            main_lines.append('printf("%%d\\n", %s(%s));' % (fname, args))
        # char: 只用 0..127 (qcc 的 char 是 unsigned 约定, ≥0x80 与 gcc 符号扩展分歧 — 已知实现定义差异)
        if r.random() < 0.5:
            ch = self.ident('ch', used)
            main_lines.append('char %s = %s;' % (ch, r.choice(["'A'", "'z'", "'0'", "' '" ])))
            main_lines.append('printf("%%c\\n", %s);' % ch)
        main_lines.append('return 0;')
        main_lines.append('}')
        parts.append('\n'.join(main_lines))
        return '\n'.join(parts)

def run_compiler(compiler, workdir, tag):
    exe = os.path.join(workdir, 'q.exe')
    r = subprocess.run([compiler, os.path.join(workdir, 't.c'), '-o', exe],
                       capture_output=True, text=True, timeout=60)
    if r.returncode != 0:
        return (tag + '-COMPILE-FAIL', r.stderr[:300], None)
    r = subprocess.run([exe], capture_output=True, text=True, timeout=10)
    return (tag + '-RUN', r.returncode, r.stdout)

def run_gcc(workdir):
    exe = os.path.join(workdir, 'g.exe')
    r = subprocess.run(['gcc', '-O0', '-w', os.path.join(workdir, 't.c'), '-o', exe],
                       capture_output=True, text=True, timeout=60)
    if r.returncode != 0:
        return ('GCC-COMPILE-FAIL', r.stderr[:300], None)
    r = subprocess.run([exe], capture_output=True, text=True, timeout=10)
    return ('GCC-RUN', r.returncode, r.stdout)

def main():
    rounds = int(sys.argv[1]) if len(sys.argv) > 1 else 200
    seed = int(sys.argv[2]) if len(sys.argv) > 2 else 828
    # 第三个参数可选: 指定编译器 (默认 qcc_x86; 'v2' = 自宿主编译器)
    which = sys.argv[3] if len(sys.argv) > 3 else 'qcc'
    compiler = QCC
    if which == 'v2':
        compiler = os.path.join(ROOT, 'v2.exe')
    tag = 'v2' if which == 'v2' else 'qcc'
    rng = random.Random(seed)
    workdir = os.path.join(ROOT, 'scratch_test', 'fuzz')
    os.makedirs(workdir, exist_ok=True)
    fails = 0
    skipped = 0
    for i in range(rounds):
        prog = Gen(rng).gen_program()
        with open(os.path.join(workdir, 't.c'), 'w', encoding='utf-8') as f:
            f.write(prog)
        qr = run_compiler(compiler, workdir, tag)
        if qr[0] == tag + '-COMPILE-FAIL':
            # 编译器拒绝的合法程序 = 真 bug
            fails += 1
            print('=== 第 %d 轮: %s 编译失败 ===' % (i + 1, tag))
            print(prog)
            print(tag, ':', qr[1])
            if fails >= 5:
                break
            continue
        gr = run_gcc(workdir)
        if gr[0] == 'GCC-COMPILE-FAIL':
            skipped += 1
            continue  # gcc 拒绝(生成器边缘) → 跳过
        ok = (qr[1] == gr[1] and qr[2] == gr[2])
        if not ok:
            fails += 1
            print('=== 第 %d 轮: 输出/退出码差异 ===' % (i + 1))
            print(prog)
            print(tag, ':', qr)
            print('gcc :', gr)
            if fails >= 5:
                break
    print('共 %d 轮, 差异 %d 个, gcc 拒绝 %d (seed=%d, compiler=%s)' % (rounds, fails, skipped, seed, tag))
    sys.exit(1 if fails else 0)

if __name__ == '__main__':
    main()
