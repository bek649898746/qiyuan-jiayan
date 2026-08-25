# -*- coding: utf-8 -*-
"""case15 成员访问迁移综合回归（big-param/嵌套链/&e->f/arr[i].field/ptr->field/ARROWFALL）
用法: python scripts/case15_regress.py [编译器...]
默认编译 host(qcc_x86.exe)+v1(v1.exe) 并运行对比输出
"""
import subprocess, os, sys, io
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')
root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
os.chdir(root)
out = os.path.join(root, 'scratch_test')
os.makedirs(out, exist_ok=True)
comps = sys.argv[1:] or ['qcc_x86.exe', 'v1.exe']
args = ['-D', '__STDC_VERSION__=199901L', '-D', '__GNUC__=5', '-D', '_WIN32=1', '-D', 'REG_STARTEND=1', '-I', 'compat_prelude.h']
tests = {
    'is_define.c': 'struct Inner { int a; char b; long long c; };\nint inner_sum(struct Inner s) { return s.a + (int)s.c; }\nint main(void) { return 0; }\n',
    'nested.c': '#include <stdio.h>\nstruct A { int x; };\nstruct B { struct A in; int y; };\nstruct C { struct B *pb; };\nint main(void) { struct B b; b.in.x = 7; b.y = 3; struct C c; c.pb = &b; printf("%d %d %d\\n", b.in.x, c.pb->in.x, c.pb->y); return 0; }\n',
    'amp_field.c': '#include <stdio.h>\nstruct S { int n; int m[4]; };\nint main(void) { struct S s; s.n = 5; int *p = &s.n; *p = 9; s.m[2] = 11; printf("%d %d\\n", s.n, s.m[2]); return 0; }\n',
    'arr_field.c': '#include <stdio.h>\nstruct Item { int id; char tag; long long val; };\nint main(void) { struct Item arr[3]; arr[0].id = 10; arr[0].tag = 97; arr[0].val = 100; arr[1].id = 20; arr[1].tag = 98; arr[1].val = 200; struct Item *pa[2]; pa[0] = &arr[0]; pa[1] = &arr[1]; printf("%d %d %lld | %d %d\\n", arr[0].id, arr[0].tag, arr[0].val, pa[1]->id, (int)pa[1]->val); return 0; }\n',
    'arr_bitfield.c': '#include <stdio.h>\nstruct B { unsigned a:3; unsigned b:5; int x; };\nint main(void) { struct B v[2]; v[0].a = 5; v[0].b = 20; v[0].x = 7; v[1].a = 3; v[1].b = 17; v[1].x = 9; printf("%u %u %d | %u %u %d\\n", v[0].a, v[0].b, v[0].x, v[1].a, v[1].b, v[1].x); return 0; }\n',
    'ptr_field.c': '#include <stdio.h>\nstruct Node { int val; char tag; long long big; struct Node *next; };\nint main(void) { struct Node n2; n2.val = 30; n2.tag = 122; n2.big = 300; n2.next = 0; struct Node n1; n1.val = 20; n1.tag = 121; n1.big = 200; n1.next = &n2; struct Node n0; n0.val = 10; n0.tag = 120; n0.big = 100; n0.next = &n1; printf("%d %d %lld | %d\\n", n0.next->val, n0.next->tag, n0.next->big, n0.next->next->val); return 0; }\n',
    'ptr_buf.c': '#include <stdio.h>\nstruct W { int *buf; int n; };\nint main(void) { int arr[4] = {1, 2, 3, 4}; struct W w; w.buf = arr; w.n = 4; printf("%d %d\\n", w.buf[0], w.buf[3]); return 0; }\n',
    'ptr_bitfield.c': '#include <stdio.h>\nstruct B { unsigned a:3; unsigned b:5; };\nint main(void) { struct B v; v.a = 5; v.b = 25; struct B *p = &v; printf("%u %u\\n", p->a, p->b); return 0; }\n',
    'arrowfall_typedef.c': '#include <stdio.h>\ntypedef struct Node { int val; char tag; } Node;\nvoid f(Node *p) { printf("%d %d\\n", p->val, p->tag); }\nint main(void) { Node n; n.val = 42; n.tag = 7; f(&n); return 0; }\n',
    'arrowfall_global.c': '#include <stdio.h>\nstruct G { int a; long long b; };\nstruct G *gp;\nint main(void) { struct G g; g.a = 5; g.b = 99; gp = &g; printf("%d %lld\\n", gp->a, gp->b); return 0; }\n',
}
fails = 0
for name, prog in tests.items():
    src = os.path.join(out, name)
    open(src, 'w').write(prog)
    exes = {}
    for comp in comps:
        tag = comp.split('.')[0]
        exe = os.path.join(out, tag + name + '.exe')
        r = subprocess.run([comp, '-o', exe, src] + args, capture_output=True, text=True, errors='replace', timeout=60)
        ok = r.returncode == 0
        print(name, tag, 'rc=', r.returncode, 'OK' if ok else 'FAIL')
        if not ok:
            fails += 1
        exes[tag] = (exe, ok)
    # 运行对比 (host 为基准)
    base = None
    for tag, (exe, ok) in exes.items():
        if ok:
            r = subprocess.run(['cmd', '/c', exe], capture_output=True, text=True, errors='replace', timeout=15)
            outtxt = r.stdout.replace('\r\n', '\n')
            if base is None:
                base = outtxt
            elif outtxt != base:
                print('  [DIFF-RUN] %s: [%s] != host [%s]' % (name, outtxt.strip(), base.strip()))
                fails += 1
print('FAILS:', fails)
sys.exit(1 if fails else 0)
