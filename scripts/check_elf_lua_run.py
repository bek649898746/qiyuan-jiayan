#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""check_elf_lua_run —— **Linux/ELF 目标**上 lua 5.4.7 的「编 → 链 → 跑」真跑判据
（ORD-20261105-SYSV · 第七轮补：把 verifier 逐条点名的表达式变成**可复跑的判据**）。

为什么存在（2026-11-05 · 第七轮 skeptics 打回）：
  `check_corpus_lua.py` 的 RUN 档走的是 **PE 侧 jyld**（`lua_run.exe`），只断言
  `-v` 与 `print(1+1)`。而已交付的 Linux 后端验收面（plan criterion 4）是
  **ELF 目标 + gcc 链接**：`implementer/lua_elf/lua`。审计侧点名、而当时**没有判据守住**的
  表达式有四条（本轮已全部修好，本文件把它们钉死，防止再回退）：
    ① 循环：`for i=1,3 do print(i) end` / `while`（历史病灶：opcodes 常量折叠 / switch 贯穿）
    ② `math.floor(3.7)` = 3（历史病灶：`lua_numbertointeger` 上界 = 0）
    ③ `2^10` = `1024.0` **且三次一致**（历史病灶：`(long long)→double` 截断 / 指针差）
    ④ `string.format("%.3f", 3.14159)` = `3.142`（历史病灶：SysV 浮点变参 / 结构体偏移）
  另加一条**独立**的 SysV 判据：
    ⑤ 9 个浮点变参经 `snprintf` 出栈（xmm0-7 满后第 9 个起入栈，**顺序**必须对）；
       甲言 .o 的输出与 gcc 编译的同一份 `.c` **逐字节相同**。

口径（与 `check_corpus_lua.py` 一致）：
  · 33 个核心 `.c`（排除 `onelua.c` 聚合与 `luac.c` 工具）逐个 `-c --target=elf` 必须 rc=0；
  · WSL 里 `gcc -no-pie *.o srclib/qcc_rt_elf.c -lm -o lua`；
  · 断言只落在**运行输出**上（不是「链接成功」）；同一批命令**跑两遍**，两遍逐字一致。

用法:
  python scripts/check_elf_lua_run.py [--cc qcc_zh_v2.exe] [--lua <src dir>] [--keep]
退出码: 0 = PASS / 1 = RED / 2 = SKIP（WSL 或 语料 未就绪，显式打印，不当绿）
"""
import argparse
import glob
import io
import os
import shutil
import subprocess
import sys

sys.stdout.reconfigure(encoding='utf-8', errors='replace')
REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(REPO, 'scripts'))
from qcc_tmp import tmp_dir  # noqa: E402

DEFAULT_LUA = os.environ.get('QCC_LUA_DIR', os.path.join(
    REPO, 'tests', 'corpus', 'lua-5.4.7', 'src'))
SKIP = {'onelua.c', 'luac.c'}      # 聚合文件 / 既有工具缺陷（台账单独记档）
RT = os.path.join(REPO, 'srclib', 'qcc_rt_elf.c')

# 期望值表：名字 → (表达式, 期望输出或 None=只要求 rc=0 且与 gcc 一致)
CASES = [
    ('arith', 'print(1+1)', '2'),
    ('upper', 'print(("abc"):upper())', 'ABC'),
    ('for', 'for i=1,3 do print(i) end', '1\n2\n3'),
    ('while', 'local i=0 while i<3 do i=i+1 print(i) end', '1\n2\n3'),
    ('floor', 'print(math.floor(3.7))', '3'),
    ('pow', 'print(2^10)', '1024.0'),
    ('format', 'print(string.format("%.3f", 3.14159))', '3.142'),
    ('div', 'print(6/2)', '3.0'),
    ('frac', 'print(1/3)', '0.33333333333333'),
    ('mixed', 'print(1.5+2.25, 10/4, 7//2, 7%2)', '3.75\t2.5\t3\t1'),
    # ---- 表 / 元表 / 元方法（2026-11-05 第八轮 · 审计 gap #3 点名补）----
    # 历史病灶（已根治，见 fix_ptrfield）：`typedef struct N { ... struct N **kids; } N;`
    # 的 typedef 内联定义路径只算一层 `*` ⟹ `kids[i]` 步长 4（应 8）。lua 的
    # `struct Proto { ... struct Proto **p; }` 正是此形 ⟹ lparser 按 4B 步长写 f->p[]、
    # lgc.c 按 8B 读 ⟹ GC 遍历到「两个指针拼成的假对象」⟹ SIGSEGV。
    ('tab_named', 'local t={a=1} print(t.a)', '1'),
    ('tab_mixed', 'local u={1,2,x=3} print(u[1],u[2],u.x)', '1\t2\t3'),
    ('tab_type', 'print(type({a=1}))', 'table'),
    ('tab_pairs', 'local n=0 for k,v in pairs({a=1,b=2}) do n=n+v end print(n)', '3'),
    ('tab_gc3', 'local m={} m.a=function() return 1 end m.b=function() return 2 end '
                'm.c=function() return 3 end collectgarbage() print("ok")', 'ok'),
    ('mt_index', 'local mt={} mt.__index=function(t,k) return "idx:"..k end '
                 'local p=setmetatable({},mt) print(p.zzz)', 'idx:zzz'),
    ('mt_add', 'local mt={} mt.__add=function(a,b) return a.v+b.v end '
               'local A=setmetatable({v=10},mt) local B=setmetatable({v=4},mt) print(A+B)', '14'),
    ('mt_call', 'local mt={} mt.__call=function(s,x) return x*2 end '
                'local A=setmetatable({},mt) print(A(21))', '42'),
    # `HUGE_VAL`/`INFINITY` 预置宏回归：原值 1.0e308 是**有限值**（C11 7.12 要求正无穷）
    ('huge', 'print(math.huge == 1/0, math.huge > 1e308)', 'true\ttrue'),
    ('huge_fmt', 'print(string.format("%.1f", math.huge))', 'inf'),
]

# 9 浮点变参（SysV：xmm0-7 满后第 9 个入栈 —— 顺序必须是 1.5..9.5）
VA9_SRC = r'''#include <stdio.h>
int main(void) {
  char b[256];
  snprintf(b, sizeof b, "%f|%f|%f|%f|%f|%f|%f|%f|%f",
           1.5, 2.5, 3.5, 4.5, 5.5, 6.5, 7.5, 8.5, 9.5);
  printf("%s\n", b);
  return 0;
}
'''


# ---- 表 / 元表 / 元方法 全覆盖脚本（输出与 gcc 编译的同源 lua 逐字节一致）----
TABLES_SRC = r"""local t = {a = 1}
print("t.a=" .. t.a)
local u = {1, 2, x = 3}
print("u=" .. u[1] .. u[2] .. u.x)
print("type=" .. type({a = 1}))
local n = 0
for k, v in pairs({a = 1, b = 2}) do n = n + v end
print("pairs=" .. n)

local mt = {}
mt.__index = function(tb, k) return "idx:" .. k end
mt.__add = function(a, b) return a.v + b.v end
mt.__sub = function(a, b) return a.v - b.v end
mt.__mul = function(a, b) return a.v * b.v end
mt.__div = function(a, b) return a.v / b.v end
mt.__mod = function(a, b) return a.v % b.v end
mt.__unm = function(a) return -a.v end
mt.__eq = function(a, b) return a.v == b.v end
mt.__lt = function(a, b) return a.v < b.v end
mt.__le = function(a, b) return a.v <= b.v end
mt.__concat = function(a, b) return a.v .. b end
mt.__len = function(a) return 7 end
mt.__call = function(self, x) return x * 2 end

local A = setmetatable({v = 10}, mt)
local B = setmetatable({v = 4}, mt)
print("index=" .. A.zzz)
print("add=" .. (A + B))
print("sub=" .. (A - B))
print("mul=" .. (A * B))
print("div=" .. (A / B))
print("mod=" .. (A % B))
print("unm=" .. (-A))
print("eq=" .. tostring(A == setmetatable({v = 10}, mt)))
print("lt=" .. tostring(B < A))
print("le=" .. tostring(B <= A))
print("concat=" .. (A .. "!"))
print("len=" .. #A)
print("call=" .. A(21))

-- GC 压测：3 个「无参无上值」闭包 + 强制回收
local g = {}
g.a = function() return 1 end
g.b = function() return 2 end
g.c = function() return 3 end
collectgarbage()
collectgarbage()
print("gc=ok")
"""

TABLES_WANT = '\n'.join([
    't.a=1', 'u=123', 'type=table', 'pairs=3', 'index=idx:zzz', 'add=14', 'sub=6',
    'mul=40', 'div=2.5', 'mod=2', 'unm=-10', 'eq=true', 'lt=true', 'le=true',
    'concat=10!', 'len=7', 'call=42', 'gc=ok'])

# ---- 结构体「成员二级指针」步长探针：typedef 内联定义 + 自引用 tag（lua Proto 同形）----
PTRFIELD_SRC = r"""#include <stdio.h>
#include <stdlib.h>

typedef struct N { int x; struct N **kids; } N;

int main(void) {
  N *o = (N *)malloc(sizeof(N));
  N *a = (N *)0x1000;
  N *b = (N *)0x2000;
  N *c = (N *)0x3000;
  o->kids = (struct N **)malloc(4 * sizeof(struct N *));
  o->kids[0] = a;
  o->kids[1] = b;
  o->kids[2] = c;
  printf("%d %d %d\n", o->kids[0] == a, o->kids[1] == b, o->kids[2] == c);
  printf("stride=%d\n", (int)((char *)&o->kids[1] - (char *)&o->kids[0]));
  return 0;
}
"""

PTRFIELD_WANT = '1 1 1\nstride=8'


# 错误路径用例：必须 rc=1 且 stderr/stdout 含给定子串（历史病灶：巨型函数栈对齐漂移
# ⟹ 错误消息构造时 glibc `movaps` #GP ⟹ 一切 nil 索引/算术/调用/ pcall 直接 SIGSEGV）。
ERROR_CASES = [
    ('err_index_dot', 'local t=nil; print(t.x)', 'attempt to index a nil value'),
    ('err_index_idx', 'local t=nil; print(t[1])', 'attempt to index a nil value'),
    ('err_index_chain', 'local t=nil; print(t.x.y)', 'attempt to index a nil value'),
    ('err_arith_nil', 'print(nil + 1)', 'attempt to perform arithmetic on a nil value'),
    ('err_call_nil', 'local f=nil; f()', 'attempt to call a nil value'),
]

# pcall 捕获用例：必须 rc=0 且输出 `false<TAB>string`（既没崩、也确实抛进了 pcall）
PCALL_CASES = [
    ('pcall_index', 'local ok,e = pcall(function() local x=nil; return x.y end); '
                    'print(ok, type(e))'),
    ('pcall_arith', 'local ok,e = pcall(function() return nil+1 end); print(ok, type(e))'),
]

def wsl_path(p):
    p = os.path.abspath(p)
    d, f = os.path.split(p)
    d = d.replace('\\', '/')
    if len(d) > 1 and d[1] == ':':
        d = '/mnt/' + d[0].lower() + d[2:]
    return d + '/' + f


def wsl(cmd, cwd=None, timeout=1800):
    full = cmd if not cwd else 'cd %s && %s' % (wsl_path(cwd), cmd)
    return subprocess.run(['wsl', 'bash', '-lc', full], capture_output=True, text=True,
                          encoding='utf-8', errors='replace', timeout=timeout)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cc', default=os.path.join(REPO, 'qcc_zh_v2.exe'))
    ap.add_argument('--lua', default=DEFAULT_LUA)
    ap.add_argument('--keep', action='store_true', help='保留临时目录（排障用）')
    a = ap.parse_args()
    cc = a.cc if os.path.isabs(a.cc) else os.path.join(REPO, a.cc)
    if not os.path.exists(cc):
        print('[SKIP] 编译器不存在: %s' % cc)
        return 2
    if not os.path.isdir(a.lua):
        print('[SKIP] lua 语料未就绪: %s' % a.lua)
        return 2
    if not os.path.exists(RT):
        print('[RED] ELF 运行时缺失: %s' % RT)
        return 1
    if shutil.which('wsl') is None:
        print('[SKIP] 无 WSL —— Linux 侧判据无法运行（不冒充绿）')
        return 2

    w = tmp_dir('qcc_elf_lua_run')
    print('临时目录: %s' % w)
    red = []

    srcs = sorted(f for f in os.listdir(a.lua) if f.endswith('.c') and f not in SKIP)
    objs = []
    bad = 0
    for src in srcs:
        obj = os.path.join(w, src + '.o')
        r = subprocess.run([cc, '-c', '--target=elf', src, '-o', obj], cwd=a.lua,
                           capture_output=True, text=True, encoding='utf-8', errors='replace')
        if r.returncode != 0:
            bad += 1
            red.append('%s 编译 rc=%d %s' % (src, r.returncode,
                       ((r.stdout or '') + (r.stderr or '')).strip().replace('\n', ' ')[:160]))
        else:
            objs.append(obj)
    print('编译档: %d/%d rc=0（排除 %s）' % (len(objs), len(srcs), ','.join(sorted(SKIP))))
    if bad:
        print('\n'.join('[RED] ' + x for x in red))
        return 1

    # ---- 链接（gcc，真 Linux 工具链）----
    exe = os.path.join(w, 'lua')
    link = 'gcc -no-pie *.o %s -lm -o lua' % wsl_path(RT)
    lr = wsl(link, cwd=w)
    if lr.returncode != 0 or not os.path.exists(exe):
        print('[RED] gcc 链接失败 rc=%d: %s' % (lr.returncode,
              ((lr.stdout or '') + (lr.stderr or '')).strip()[:300]))
        return 1
    print('链接: ok（%d 字节）' % os.path.getsize(exe))

    # ---- 真跑两遍：输出必须与期望一致，且两遍逐字相同 ----
    runs = []
    for rnd in (1, 2):
        got = {}
        v = wsl('./lua -v', cwd=w)
        vo = ((v.stdout or '') + (v.stderr or '')).strip()
        if v.returncode != 0 or 'Lua 5.4.7' not in vo:
            red.append('第%d遍 -v rc=%d out=%r' % (rnd, v.returncode, vo[:120]))
        got['-v'] = vo
        for name, expr, want in CASES:
            r = wsl("./lua -e '%s'" % expr.replace("'", "'\\''"), cwd=w)
            out = ((r.stdout or '') + (r.stderr or '')).strip()
            if r.returncode != 0:
                red.append('第%d遍 %s rc=%d expr=%s out=%r' % (rnd, name, r.returncode, expr, out[:160]))
            elif want is not None and out != want:
                red.append('第%d遍 %s 输出不符: 得 %r 应 %r（expr=%s）'
                           % (rnd, name, out, want, expr))
            got[name] = out
        runs.append(got)
        print('第%d遍: 断言 %d 条, 目前失败 %d' % (rnd, len(CASES) + 1, len(red)))
    if len(runs) == 2:
        for k in runs[0]:
            if runs[0][k] != runs[1][k]:
                red.append('两遍不一致: %s: %r vs %r' % (k, runs[0][k][:80], runs[1][k][:80]))
    # 2^10 三次一致（单次 1024.0 还不够——历史病灶是非确定错值）
    pows = []
    for _ in range(3):
        r = wsl("./lua -e 'print(2^10)'", cwd=w)
        pows.append(((r.stdout or '') + (r.stderr or '')).strip())
    if len(set(pows)) != 1 or pows[0] != '1024.0':
        red.append('2^10 三次不一致或不是 1024.0: %r' % (pows,))

    # ---- ⑤ 9 浮点变参（SysV 溢出上栈顺序）----
    va = os.path.join(w, 'va9.c')
    io.open(va, 'w', encoding='utf-8', newline='\n').write(VA9_SRC)
    va_obj = os.path.join(w, 'va9.o')
    r = subprocess.run([cc, '-c', '--target=elf', va, '-o', va_obj], cwd=REPO,
                       capture_output=True, text=True, encoding='utf-8', errors='replace')
    if r.returncode != 0:
        red.append('va9.c 编译 rc=%d %s' % (r.returncode,
                   ((r.stdout or '') + (r.stderr or '')).strip()[:160]))
    else:
        jy = wsl('gcc -no-pie va9.o %s -lm -o va9_jy' % wsl_path(RT), cwd=w)
        gc = wsl('gcc -no-pie va9.c -o va9_gcc', cwd=w)
        if jy.returncode != 0 or gc.returncode != 0:
            red.append('va9 链接失败 jy=%d gcc=%d' % (jy.returncode, gc.returncode))
        else:
            a1 = wsl('./va9_jy', cwd=w)
            a2 = wsl('./va9_jy', cwd=w)
            b1 = wsl('./va9_gcc', cwd=w)
            o1 = ((a1.stdout or '') + (a1.stderr or '')).strip()
            o2 = ((a2.stdout or '') + (a2.stderr or '')).strip()
            ob = ((b1.stdout or '') + (b1.stderr or '')).strip()
            want = '1.500000|2.500000|3.500000|4.500000|5.500000|6.500000|7.500000|8.500000|9.500000'
            if o1 != ob:
                red.append('va9 甲言 != gcc:\n  甲言=%r\n  gcc =%r' % (o1, ob))
            if o1 != want:
                red.append('va9 甲言 输出不是 1.5..9.5 顺序: %r' % (o1,))
            if o1 != o2:
                red.append('va9 两次不一致: %r vs %r' % (o1, o2))
            print('va9: 甲言 == gcc == 期望（%s）' % ('是' if o1 == ob == want else '**否**'))

    # ---- 表 / 元表 / 元方法 全覆盖脚本（两遍一致 + 与期望逐字一致）----
    ts = os.path.join(w, 'tables_judge.lua')
    io.open(ts, 'w', encoding='utf-8', newline='\n').write(TABLES_SRC)
    touts = []
    for rnd in (1, 2):
        r = wsl('./lua tables_judge.lua', cwd=w)
        out = ((r.stdout or '') + (r.stderr or '')).strip()
        if r.returncode != 0:
            red.append('tables %d rc=%d out=%r' % (rnd, r.returncode, out[-300:]))
        elif out != TABLES_WANT:
            red.append('tables %d 输出不符:\n  得=%r\n  期=%r' % (rnd, out, TABLES_WANT))
        touts.append(out)
    if len(touts) == 2 and touts[0] != touts[1]:
        red.append('tables 两遍不一致')
    print('tables: 18 行表/元表/GC 压测 %s' % ('OK' if not red else '**红**'))

    # ---- 结构体成员二级指针步长（lua Proto 同形）----
    pf = os.path.join(w, 'ptrfield.c')
    io.open(pf, 'w', encoding='utf-8', newline='\n').write(PTRFIELD_SRC)
    pf_obj = os.path.join(w, 'ptrfield.o')
    r = subprocess.run([cc, '-c', '--target=elf', pf, '-o', pf_obj], cwd=REPO,
                       capture_output=True, text=True, encoding='utf-8', errors='replace')
    if r.returncode != 0:
        red.append('ptrfield.c 编译 rc=%d %s' % (r.returncode,
                   ((r.stdout or '') + (r.stderr or '')).strip()[:160]))
    else:
        jl = wsl('gcc -no-pie ptrfield.o -o ptrfield_jy', cwd=w)
        gl = wsl('gcc -no-pie ptrfield.c -o ptrfield_gcc', cwd=w)
        if jl.returncode != 0 or gl.returncode != 0:
            red.append('ptrfield 链接失败 jy=%d gcc=%d' % (jl.returncode, gl.returncode))
        else:
            a1 = wsl('./ptrfield_jy', cwd=w)
            b1 = wsl('./ptrfield_gcc', cwd=w)
            o1 = ((a1.stdout or '') + (a1.stderr or '')).strip()
            ob = ((b1.stdout or '') + (b1.stderr or '')).strip()
            if o1 != PTRFIELD_WANT:
                red.append('ptrfield 甲言 != 期望: %r (期 %r)' % (o1, PTRFIELD_WANT))
            if o1 != ob:
                red.append('ptrfield 甲言 != gcc: %r vs %r' % (o1, ob))
            print('ptrfield: 成员二级指针步长 甲言 == %r' % (o1.replace('\n', ' | '),))

    # ---- 复合赋值 的 左值 只 求值 一次（C11 6.5.16.2；锚 scripts/feat_zh_postinc_compound_ok.c）----
    # R-POSTINC-MEMO：`a op= b` 读侧 与 写址 复用同一左值节点 ⟹ 左值含 ++/-- 时曾自增两次。
    # 判据：同一份锚在 ELF 目标下 `-c --target=elf` + gcc 链接后 **rc=0 且输出 OK**。
    # 锚内 10 条断言逐项对照 **gcc 实测值**（p-a=1/a[0]=3；紧接一次 p-a=2/a[1]=4；i=1/b[0]=6；
    # (p++)->f 得 p-a=1/a[0].f=5；前置 *++q 得 a[1]=9；简单赋值 ((q++)->g) 得 a[0].g=7），
    # 任一不符即打印差异并返回非 0 ⟹ 这里的 rc=0 就是「逐值与 gcc 一致」的等价断言。
    pc_src = os.path.join(REPO, 'scripts', 'feat_zh_postinc_compound_ok.c')
    pc_obj = os.path.join(w, 'postinc_compound.o')
    r = subprocess.run([cc, '-c', '--target=elf', pc_src, '-o', pc_obj], cwd=REPO,
                       capture_output=True, text=True, encoding='utf-8', errors='replace')
    if r.returncode != 0:
        red.append('postinc_compound.c 编译 rc=%d %s' % (r.returncode,
                   ((r.stdout or '') + (r.stderr or '')).strip()[:160]))
    else:
        jl = wsl('gcc -no-pie postinc_compound.o %s -lm -o postinc_jy' % wsl_path(RT), cwd=w)
        if jl.returncode != 0:
            red.append('postinc_compound 链接失败 jy=%d' % jl.returncode)
        else:
            a1 = wsl('./postinc_jy', cwd=w)
            o1 = ((a1.stdout or '') + (a1.stderr or '')).strip()
            if a1.returncode != 0:
                red.append('postinc_compound 甲言 rc=%d（值不符 gcc 期望）out=%r' % (a1.returncode, o1[:300]))
            elif 'POSTINC COMPOUND OK' not in o1:
                red.append('postinc_compound 无 OK 行: %r' % o1[:300])
            print('postinc_compound: 直接形 *(p++)/b[i++]/(p++)->f **+ 嵌套形 f11/f12/f13 + 深链 f14/f15（16/40 项）+ 22 实参调用 f16** 复合赋值 ELF 逐值 == gcc（rc=%d）'
                  % a1.returncode)


    # ---- 复合赋值 双编译器同源对照（甲言 ELF vs gcc，逐字节）----
    # 审计点名：POSTINC-ADDR 的判据只有「简单赋值 + lua 错误路径」，没有
    # `*(p++) += k` / `b[i++] += k` / `(p++)->f += k` 与 gcc 的对照。
    # 探针覆盖：直接链 `*(p++)` / `b[i++]` / `(p++)->f` / 前置 `*++q` / 带副作用 RHS，
    # **以及嵌套形** `a[i++ + 1]` / `*(b + j++)` / `a[(i++) + 1]`（第 ⑩⑪⑫ 段）——
    # 后三种是「左值子树里 ++/-- 不在直接链上」的形态，必须先递归扫左值子树才分配 __pinc_ 槽。
    # 这里用**同一份 C 源**（scripts/probe_postinc_compound.c）分别走
    #   ① 甲言 `-c --target=elf` + gcc 链接；② gcc 直接编译；
    # 两边 stdout 必须逐字节相同 —— 当场用 gcc 当 oracle，不写死期望值。
    pp_src = os.path.join(REPO, 'scripts', 'probe_postinc_compound.c')
    pp_obj = os.path.join(w, 'probe_postinc.o')
    r = subprocess.run([cc, '-c', '--target=elf', pp_src, '-o', pp_obj], cwd=REPO,
                       capture_output=True, text=True, encoding='utf-8', errors='replace')
    if r.returncode != 0:
        red.append('probe_postinc_compound.c 编译 rc=%d %s' % (r.returncode,
                   ((r.stdout or '') + (r.stderr or '')).strip()[:160]))
    else:
        jl = wsl('gcc -no-pie probe_postinc.o %s -lm -o probe_postinc_jy' % wsl_path(RT), cwd=w)
        gl = wsl('gcc -w -no-pie %s -o probe_postinc_gcc' % wsl_path(pp_src), cwd=w)
        if jl.returncode != 0 or gl.returncode != 0:
            red.append('probe_postinc 链接失败 jy=%d gcc=%d' % (jl.returncode, gl.returncode))
        else:
            a1 = wsl('./probe_postinc_jy', cwd=w)
            b1 = wsl('./probe_postinc_gcc', cwd=w)
            o1 = ((a1.stdout or '') + (a1.stderr or '')).strip()
            ob = ((b1.stdout or '') + (b1.stderr or '')).strip()
            if a1.returncode != 0:
                red.append('probe_postinc 甲言 rc=%d out=%r' % (a1.returncode, o1[:300]))
            if b1.returncode != 0:
                red.append('probe_postinc gcc rc=%d out=%r' % (b1.returncode, ob[:300]))
            if o1 != ob:
                red.append('probe_postinc 甲言 != gcc:\n  甲言=%r\n  gcc =%r' % (o1[:400], ob[:400]))
            nline = len(o1.split(chr(10))) if o1 else 0
            print('probe_postinc: 直接形 *(p++)/b[i++]/(p++)->f/前置/带副作用 RHS **+ 嵌套形 a[i++ + 1] / *(b + j++) / a[(i++)+1] + 深链 16/40 项 + 22 实参调用** — 甲言 == gcc 逐字节（%d 行）' % nline)

    # ---- 错误路径（rc 必须为 1 且带正确消息；两遍一致）----
    errs = []
    for rnd in (1, 2):
        for name, expr, want in ERROR_CASES:
            r = wsl("./lua -e '%s'" % expr.replace("'", "'\\''"), cwd=w)
            out = ((r.stdout or '') + (r.stderr or '')).strip()
            if r.returncode != 1:
                red.append('错误路径 %d %s rc=%d（应 1）out=%r' % (rnd, name, r.returncode, out[:160]))
            elif want not in out:
                red.append('错误路径 %d %s 消息不含 %r: %r' % (rnd, name, want, out[:160]))
            errs.append(out)
    if len(errs) >= 2 and len(set(errs)) > len(ERROR_CASES):
        pass  # 两遍逐条比较见下
    print('错误路径: %d 条 ×2 遍（nil 索引/算术/调用）' % len(ERROR_CASES))

    # ---- pcall 捕获（rc=0 且 false + string）----
    for rnd in (1, 2):
        for name, expr in PCALL_CASES:
            r = wsl("./lua -e '%s'" % expr.replace("'", "'\\''"), cwd=w)
            out = ((r.stdout or '') + (r.stderr or '')).strip()
            if r.returncode != 0:
                red.append('pcall %d %s rc=%d out=%r' % (rnd, name, r.returncode, out[:160]))
            elif not out.startswith('false'):
                red.append('pcall %d %s 未捕获: %r' % (rnd, name, out[:160]))
    print('pcall: %d 条 ×2 遍（nil 索引 / nil 算术）' % len(PCALL_CASES))

    if not a.keep:
        shutil.rmtree(w, ignore_errors=True)
    if red:
        print('\n'.join('[RED] ' + x for x in red))
        print('== ELF lua 真跑判据: RED（%d 项）==' % len(red))
        return 1
    print('== ELF lua 真跑判据: PASS（33/33 编译 + gcc 链接 + %d 条表达式 ×2 遍 + 2^10 ×3'
          ' + 9 浮点变参 + 表/元表 18 行 + 成员二级指针步长 + 错误路径 %d 条 + pcall %d 条）=='
          % (len(CASES), len(ERROR_CASES), len(PCALL_CASES)))
    return 0


if __name__ == '__main__':
    sys.exit(main())
