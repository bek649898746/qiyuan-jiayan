# -*- coding: utf-8 -*-
"""check_ir_runtime_parity.py —— IR 后端运行时语义验证（两版编译 → 运行 → 比 stdout）

判据：IR 后端运行时语义与 legacy 一致（10 类典型形态）。
目的：字节等价是「一致性」目标；本脚本回答更本质的问题：**IR 后端生成的代码语义是否正确**。
覆盖：指针后自增写、结构体成员、数组下标、指针数组、函数指针、浮点、switch、循环、递归、字符串。
"""
import os, subprocess, sys
sys.stdout.reconfigure(encoding='utf-8', errors='replace')
REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CC = os.path.join(REPO, 'qcc_zh_v1.exe')
WORK = os.path.join(REPO, 'build', 'rt_probe')
os.makedirs(WORK, exist_ok=True)

CASES = {
'pinc': '''
#include <stdio.h>
int main(void){ int a[4]; int *p; a[0]=0;a[1]=0;a[2]=0;a[3]=0; p=a; *p++=7; *p++=9;
  printf("%d %d %d %d %d\\n", a[0],a[1],a[2],a[3],(int)(p-a)); return 0; }
''',
'struct': '''
#include <stdio.h>
typedef struct { int x; int y; } P;
int main(void){ P p; P *q; p.x=3; p.y=4; q=&p; q->x = q->x + q->y;
  printf("%d %d\\n", p.x, p.y); return 0; }
''',
'index': '''
#include <stdio.h>
int main(void){ int a[5]; int i; int s;
  for(i=0;i<5;i++) a[i]=i*i; s=0; for(i=0;i<5;i++) s+=a[i];
  printf("%d %d\\n", s, a[3]); return 0; }
''',
'ptrarr': '''
#include <stdio.h>
int main(void){ int a[3]; int *tab[3]; int i; int s; a[0]=5;a[1]=6;a[2]=7;
  tab[0]=&a[0]; tab[1]=&a[1]; tab[2]=&a[2];
  s=0; for(i=0;i<3;i++) s += *tab[i];
  printf("%d %d\\n", s, *tab[2]); return 0; }
''',
'fnptr': '''
#include <stdio.h>
int add(int a, int b){ return a+b; }
int mul(int a, int b){ return a*b; }
int main(void){ int (*f)(int,int); int r;
  f = add; r = f(3,4); printf("%d ", r);
  f = mul; r = f(3,4); printf("%d\\n", r); return 0; }
''',
'floatx': '''
#include <stdio.h>
int main(void){ double a; double b; double c; a=1.5; b=2.25; c=a*b+a/b;
  printf("%.4f %.4f\\n", c, a+b); return 0; }
''',
'switchx': '''
#include <stdio.h>
int klas(int x){ switch(x){ case 0: return 10; case 1: return 20; case 2: return 30; default: return -1; } }
int main(void){ printf("%d %d %d %d\\n", klas(0), klas(1), klas(2), klas(9)); return 0; }
''',
'loops': '''
#include <stdio.h>
int main(void){ int i; int j; int s; s=0;
  for(i=0;i<4;i++){ for(j=0;j<3;j++){ if(j==2) continue; if(i==3) break; s += i*10+j; } }
  printf("%d\\n", s); return 0; }
''',
'recur': '''
#include <stdio.h>
int fib(int n){ if(n<2) return n; return fib(n-1)+fib(n-2); }
int main(void){ printf("%d %d %d\\n", fib(0), fib(7), fib(12)); return 0; }
''',
'strx': '''
#include <stdio.h>
#include <string.h>
int main(void){ char b[32]; char *s; int n;
  b[0]=0; strcpy(b, "hello"); s = b; n = (int)strlen(s);
  printf("%s %d %c\\n", s, n, s[1]); return 0; }
''',
}


def run_case(name, code):
    src = os.path.join(WORK, name + '.c')
    with open(src, 'w', encoding='utf-8') as f:
        f.write(code)
    outs = {}
    for tag, env_ir in (('L', False), ('I', True)):
        exe = os.path.join(WORK, '%s_%s.exe' % (name, tag))
        env = dict(os.environ)
        if env_ir:
            env['QCC_CODEGEN'] = 'ir'
        else:
            env.pop('QCC_CODEGEN', None)
        c = subprocess.run([CC, src, '-o', exe], capture_output=True, env=env, cwd=REPO)
        if c.returncode != 0:
            outs[tag] = ('COMPILE-FAIL', (c.stdout or b'')[-300:].decode('utf-8', 'replace'))
            continue
        r = subprocess.run([exe], capture_output=True, cwd=REPO, timeout=60)
        outs[tag] = (r.returncode, (r.stdout or b'').decode('utf-8', 'replace').strip())
    return outs



CASES['staticptrarr'] = '''
#include <stdio.h>
struct T { char c; long x; };
struct T a; struct T b;
struct T *p[2];
int f(int i) { return (int)p[i]->x; }
int main(void){ a.c=1; a.x=111; b.c=2; b.x=222; p[0]=&a; p[1]=&b;
  printf("%d %d\\n", f(0), f(1));
  return (f(0)==111 && f(1)==222) ? 0 : 7; }
'''

CASES['staticptrvar'] = '''
#include <stdio.h>
struct T { char c; long x; };
struct T a;
static struct T *q;
int main(void){ a.c=3; a.x=777; q=&a;
  printf("%d %d\\n", (int)q->x, (int)q->c); return 0; }
'''

CASES['staticstructarr'] = '''
#include <stdio.h>
struct T { char c; long x; };
static struct T arr[2];
int main(void){ arr[0].c=1; arr[0].x=11; arr[1].c=2; arr[1].x=22;
  printf("%d %d\\n", (int)arr[0].x, (int)arr[1].x); return 0; }
'''

CASES['staticstrarr'] = '''
#include <stdio.h>
static char *names[2];
int main(void){ names[0]="ab"; names[1]="cd";
  printf("%c %c\\n", names[0][0], names[1][0]); return 0; }
'''

CASES['localptrarr'] = '''
#include <stdio.h>
struct T { char c; long x; };
struct T a; struct T b;
int f(int i){ struct T *pp[2]; pp[0]=&a; pp[1]=&b; return (int)pp[i]->x; }
int main(void){ a.x=5; b.x=6; printf("%d %d\\n", f(0), f(1)); return 0; }
'''

CASES['structptrarr'] = '''
#include <stdio.h>
struct T { long x; };
struct Holder { struct T *slots[3]; int n; };
struct T a; struct T b; struct Holder h;
int main(void){ a.x=11; b.x=22; h.slots[0]=&a; h.slots[1]=&b; h.n=2;
  printf("%d %d %d\\n", (int)h.slots[0]->x, (int)h.slots[1]->x, h.n); return 0; }
'''

CASES['staticlocalptrarr'] = '''
#include <stdio.h>
struct T { char c; long x; };
struct T a; struct T b;
int f(int i){ static struct T *pp[2]; pp[0]=&a; pp[1]=&b; return (int)pp[i]->x; }
int main(void){ a.c=1; a.x=1; b.c=2; b.x=2;
  printf("%d %d\\n", f(0), f(1)); return 0; }
'''

CASES['fieldidx'] = open(os.path.join(REPO, '_evidence', '20261112_ir_longmarch', 'probe_cases', 'fieldidx.c'), encoding='utf-8').read()
CASES['struinc'] = open(os.path.join(REPO, '_evidence', '20261112_ir_longmarch', 'probe_cases', 'struinc.c'), encoding='utf-8').read()
CASES['setnil3'] = open(os.path.join(REPO, '_evidence', '20261112_ir_longmarch', 'probe_cases', 'setnil3.c'), encoding='utf-8').read()

CASES['staticinc'] = open(os.path.join(REPO, '_evidence', '20261112_ir_longmarch', 'probe_cases', 'staticinc.c'), encoding='utf-8').read()

# 刀981（2026-11-15 虾米）：**静态指针数组的变量下标写入** —— `pp[k] = &u[k]` 在循环里，
# 元素宽 8 的缩放在 IR 的静态基分支曾整段缺失 ⟹ 步长 1 ⟹ 0xC0000005。
# skeptic-0 报的「函数内 static 局部指针数组 + ->字段」即此类，此前判据没有载体。
CASES['staticptrarr_index_write'] = open(os.path.join(REPO, '_evidence', '20261112_ir_longmarch', 'probe_cases', 'staticptrarr_index_write.c'), encoding='utf-8').read()

# 刀984（2026-11-15 虾米）：**解引用作箭头基**（`(*q)->x`）—— 态14 曾把解引用基也临时免解，
# 整层 `*q` 被吞 ⟹ 返回指针低 32 位（4416424 应 99）。
CASES['ptrptr_deref'] = open(os.path.join(REPO, '_evidence', '20261112_ir_longmarch', 'probe_cases', 'ptrptr_deref.c'), encoding='utf-8').read()

# skeptic-0 报告的原形状（2026-11-15 复核）：`static struct T *pp[2]; pp[i]->x`，f 体加长到 ~0x96。
# 与 `staticlocalptrarr` 同族；单列一条是为了让审计能按报告名逐一核对（不放大判据口径）。
CASES['skeptic_staticlocal_ptrtoglobal'] = open(os.path.join(REPO, '_evidence', '20261112_ir_longmarch', 'probe_cases', 'skeptic_staticlocal_ptrtoglobal.c'), encoding='utf-8').read()


# 刀987（2026-11-15 虾米）：**静态/全局结构体成员的 ++/--** —— 已知未修，显式挂账。
# 病灶：`节点_增减` 的字节在捕获期生成，静态 disp 的基准（数据基址/码位）与回放期不同；
# 捕获值含字段偏移但基准错，按槽号重算基准对但丢字段偏移 ⟹ 二者都不完整（§472/§478）。
# 这里登记为 KNOWN_DIFF：DIFF 不算失败（可见、不隐藏），但一旦修好（变 SAME）必须删行，否则 FAIL。
CASES['static_member_incdec'] = '''
#include <stdio.h>
struct T { long x; int y; };
int f(void){ static struct T t; t.x=10; t.y=3; t.x++; t.y--; ++t.x; return (int)(t.x*10 + t.y); }
int main(void){ printf("%d\\n", f()); return 0; }
'''

# 已知未修的运行时分差异（诚实登记；单调下降：修好必须删行）
# skeptic 点名（2026-11-15）：全局结构体版本 `g.x=10;g.y=3;g.x++;g.y--;++g.x; return g.x*10+g.y;`
# 期望 122；刀987 前 IR 给 113。
CASES['member_incdec_global'] = '''
#include <stdio.h>
struct T{long x;int y;};
struct T g;
int f(void){ g.x=10; g.y=3; g.x++; g.y--; ++g.x; return (int)(g.x*10+g.y); }
int main(void){ printf("%d\\n", f()); return 0; }
'''

KNOWN_DIFF = set()   # 2026-11-15 刀987 后为空：static_member_incdec 已修好并转 SAME

ok = bad = 0
registered = 0
stale = []
for name in CASES:
    o = run_case(name, CASES[name])
    same = o['L'] == o['I']
    if same:
        ok += 1
        print('%-9s SAME   rc=%s out=%r' % (name, o['L'][0], o['L'][1][:40]))
        if name in KNOWN_DIFF:
            stale.append(name)
    else:
        if name in KNOWN_DIFF:
            registered += 1
            print('%-9s DIFF(已登记)  L=%r  I=%r' % (name, o['L'], o['I']))
        else:
            bad += 1
            print('%-9s DIFF   L=%r  I=%r' % (name, o['L'], o['I']))

print('\n运行时分同: %d/%d  DIFF=%d（其中已登记 %d）' % (ok, len(CASES), bad, registered))
if stale:
    print('[FAIL] KNOWN_DIFF 过期（已修好）⟹ 请从集合里删掉：%s' % ', '.join(stale))
print("PASS" if (bad == 0 and not stale) else "FAIL")
sys.exit(0 if (bad == 0 and not stale) else 1)
