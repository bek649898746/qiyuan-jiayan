"""D6 根因判据（2026-11-03 虾米）：四项已根治缺陷的回归探针。

覆盖：
  1. 字符串宏体 C11 6.4.5 语义 —— 定义期只存原始体、使用点展开；
     `#define V "Lua " R` 之后 `V " X"` 必须得到完整 "Lua 5.4.7 X"（原丢中间段）。
  2. 取址基 + 指针字段作数组基 —— `(&t)->p[i]` 必须解引用取指针值并按元素步长缩放
     （原取字段地址 + 步长 8）。
  3. 逗号声明 typedef 指针继承 —— `typedef struct {...} TV; typedef TV *StkId; StkId p, t;`
     的 `t` 必须与 `p` 同宽（原 4 字节 int）。
  4. jyld 64 KiB 预留越界 —— 文本 ≥ 64 KiB 的对象链接后字符串节不得被 CRT/thunk 覆盖
     （原来 9 组 printf 即输出机器码）。

用法：python scripts/check_d6_rootfix.py [--cc <编译器 exe>]
退出码 0 = 全过；1 = 有失败（逐项打印 FAIL）。
"""
import os
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
JYLD = os.path.join(ROOT, "srclib", "jyld.exe")


def pick_cc():
    for name in ("qcc_zh_v2.exe", "qcc_zh_v3.exe", "qcc_zh_seed.exe"):
        p = os.path.join(ROOT, name)
        if os.path.exists(p):
            return p
    return None


def run(cmd, cwd, timeout=180):
    return subprocess.run(cmd, cwd=cwd, stdout=subprocess.PIPE,
                          stderr=subprocess.STDOUT, timeout=timeout)


def compile_run(cc, work, name, src, want, use_jyld=False):
    cfile = os.path.join(work, name + ".c")
    with open(cfile, "w", encoding="utf-8", newline="\n") as f:
        f.write(src)
    exe = os.path.join(work, name + ".exe")
    if use_jyld:
        obj = os.path.join(work, name + ".o")
        r = run([cc, "-c", cfile, "-o", obj], work)
        if r.returncode != 0:
            return False, "compile rc=%d %s" % (r.returncode, r.stdout.decode("utf-8", "replace")[-160:])
        r = run([JYLD, obj, "-o", exe], work)
        if r.returncode != 0:
            return False, "link rc=%d %s" % (r.returncode, r.stdout.decode("utf-8", "replace")[-160:])
    else:
        r = run([cc, cfile, "-o", exe], work)
        if r.returncode != 0:
            return False, "compile rc=%d %s" % (r.returncode, r.stdout.decode("utf-8", "replace")[-160:])
    try:
        r = run([exe], work, timeout=30)
    except subprocess.TimeoutExpired:
        return False, "run TIMEOUT"
    out = r.stdout.decode("utf-8", "replace")
    if want not in out:
        return False, "got [%s] want contains [%s]" % (out.strip()[:160], want)
    return True, out.strip()[:80]


STRMACRO = r'''#include <stdio.h>
#include <string.h>
#define R "5.4.7"
#define V "Lua " R
int main(void) {
    const char *a = V;
    const char *b = V " X";
    printf("%d|%s|%d|%s\n", (int)strlen(a), a, (int)strlen(b), b);
    return 0;
}
'''
STRMACRO_WANT = "9|Lua 5.4.7|11|Lua 5.4.7 X"

MEMARR = r'''#include <stdio.h>
typedef struct TV { long long v; unsigned char tt; } TV;
typedef struct Tab { unsigned char flags, lsizenode; unsigned int alimit; TV *array; } Tab;
typedef union U { long long gc; Tab h; } U;
static TV vals[4];
static Tab reg;
static Tab *hvalue(U *o) { return &o->h; }
int main(void) {
    int i;
    TV *a, *b, *c;
    for (i = 0; i < 4; i++) vals[i].v = i + 1;
    reg.array = vals;
    a = &reg.array[1];
    b = &hvalue((U *)&reg)->array[1];
    c = &(*(&reg)).array[1];
    /* 直接比较指针值（本轮修复的语义）；指针差 另见 D6-PTRDIFF 挂账 */
    printf("%d %d %d\n", a == &vals[1], b == &vals[1], c == &vals[1]);
    return 0;
}
'''
MEMARR_WANT = "1 1 1"

COMMADECL = r'''#include <stdio.h>
typedef struct TV { long long v; unsigned char tt; } TV;
typedef TV *StkId;
int main(void) {
    StkId p, t, m;
    printf("%d %d %d\n", (int)sizeof(p), (int)sizeof(t), (int)sizeof(m));
    return 0;
}
'''
COMMADECL_WANT = "8 8 8"



ARRFIELD = r"""#include <stdio.h>
struct Up { long long a[5]; };
struct Cl { int n; struct Up *upvals[1]; };
static struct Cl gc;
static struct Up gu;
static int via_param(struct Cl *c, struct Up *u) {
    c->upvals[0] = u;
    return c->upvals[0] == u && c->upvals[0] == &gu;
}
int main(void) {
    struct Cl lc;
    lc.n = 1; gc.n = 2;
    gc.upvals[0] = &gu;
    lc.upvals[0] = &gu;
    printf("%d %d %d\n", (int)(gc.upvals[0] == &gu), (int)(lc.upvals[0] == &gu), via_param(&lc, &gu));
    return 0;
}
"""
ARRFIELD_WANT = "1 1 1"

GETCFAM = r"""#include <stdio.h>
int main(void) {
    FILE *w = fopen("d6getc.txt", "w");
    fputc('A', w);
    fclose(w);
    FILE *f = fopen("d6getc.txt", "r");
    int c = getc(f);
    clearerr(f);
    setvbuf(f, 0, 0, 0);
    int d = getc(f);
    fclose(f);
    printf("c=%d d=%d\n", c, d);
    return 0;
}
"""
GETCFAM_WANT = "c=65 d=-1"

FNMACROIF = r"""#include <stdio.h>
#include <limits.h>
#define HB(b) ((UINT_MAX >> ((b) - 1)) >= 1)
#define SZ 17
#if HB(SZ)
#define R1 111
#else
#define R1 222
#endif
#if HB(17) && HB((8 + 8 + 1))
#define R2 333
#else
#define R2 444
#endif
int main(void) {
    printf("R=%d%d\n", R1, R2);
    return 0;
}
"""
FNMACROIF_WANT = "R=111333"

CAST32UNS = r"""#include <stdio.h>
typedef unsigned int Instruction;
#define MASK1(n,p) ((~((~(Instruction)0)<<(n)))<<(p))
#define getarg(i,pos,size) ((int)(((i)>>(pos)) & MASK1(size,0)))
#define GETARG_sBx(i) (getarg(i, 15, 17) - 65535)
int main(void) {
    Instruction i = 0x8081u;
    long long b = GETARG_sBx(i);
    long long c = (int)(i >> 15) - 1;
    printf("%llx %llx\n", (unsigned long long)b, (unsigned long long)c);
    return 0;
}
"""
CAST32UNS_WANT = "ffffffffffff0002 0"


CALLBASE = r"""#include <stdio.h>
typedef unsigned int Instruction;
typedef struct Proto { int sizek; Instruction *code; } Proto;
static Proto gp;
static Proto *getp(void) { return &gp; }
int main(void) {
    Instruction code[8];
    Instruction *pc = &code[5];
    gp.code = code;
    printf("b2=%ld\n", (long)(pc - getp()->code));
    return 0;
}
"""
CALLBASE_WANT = "b2=5"

INLINEBODY = r"""#include <stdio.h>
typedef unsigned int Instruction;
static long local_case(Instruction *code) {
    struct L {
        union {
            struct { Instruction *pc; int x; } l;
            void *c;
        } u;
    } v;
    v.u.l.pc = &code[5];
    v.u.l.x = 0;
    return (long)(v.u.l.pc - code);
}
int main(void) {
    Instruction code[8];
    printf("local=%ld\n", local_case(code));
    return 0;
}
"""
INLINEBODY_WANT = "local=5"

def jyld64k_src(n=9):
    body = []
    for i in range(n):
        body.append('    const char *a%d = "L%d";\n' % (i, i))
    for i in range(n):
        body.append('    printf("a%d=%%d [%%s]\\n", (int)strlen(a%d), a%d);\n' % (i, i, i))
    return ("#include <stdio.h>\n#include <string.h>\nint main(void) {\n"
            + "".join(body) + "    return 0;\n}\n")


def main():
    cc = None
    if "--cc" in sys.argv:
        cc = sys.argv[sys.argv.index("--cc") + 1]
    else:
        cc = pick_cc()
    if not cc or not os.path.exists(cc):
        print("FAIL: no compiler exe (qcc_zh_v2.exe / qcc_zh_v3.exe / qcc_zh_seed.exe)")
        return 1
    print("[cc] %s" % os.path.basename(cc))
    ok_all = True
    with tempfile.TemporaryDirectory(prefix="d6rootfix_") as work:
        cases = [
            ("strmacro", STRMACRO, STRMACRO_WANT, False),
            ("memarr", MEMARR, MEMARR_WANT, False),
            ("commadecl", COMMADECL, COMMADECL_WANT, False),
            ("jyld64k", jyld64k_src(), "a8=2 [L8]", True),
            ("arrfield", ARRFIELD, ARRFIELD_WANT, False),
            ("getcfam", GETCFAM, GETCFAM_WANT, False),
            ("fnmacroif", FNMACROIF, FNMACROIF_WANT, False),
            ("cast32uns", CAST32UNS, CAST32UNS_WANT, False),
            ("callbase", CALLBASE, CALLBASE_WANT, False),
            ("inlinebody", INLINEBODY, INLINEBODY_WANT, False),
        ]
        for name, src, want, use_jyld in cases:
            ok, info = compile_run(cc, work, name, src, want, use_jyld)
            print("[%s] %s  %s" % ("PASS" if ok else "FAIL", name, info))
            ok_all = ok_all and ok
    print("== D6 根因判据 %s ==" % ("PASS" if ok_all else "FAIL"))
    return 0 if ok_all else 1


if __name__ == "__main__":
    sys.exit(main())
