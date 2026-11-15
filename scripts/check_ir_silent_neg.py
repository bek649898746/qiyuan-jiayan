# -*- coding: utf-8 -*-
"""check_ir_silent_neg.py —— **「静默错产物」零容忍**判据（2026-11-10 新增）。

判据一句话：
    对每个载体，IR 模式只允许两种结局 ——
      ① `rc != 0` 且报 `IR-BACKEND-UNIMPL`（fail-closed，响亮）；
      ② `rc == 0` **且 .o 与 legacy 逐字节相同**。
    **禁止** `rc == 0` 而字节不同 —— 那是「静默错产物」，比拒收更糟。

为什么需要（本轮实测）：lua 34 里 `lcorolib / linit / lutf8lib` 一直 `rc=0`，
但 IR 产物分别差 +124 / −32 / −1252 字节 ⟹ 以前的「rc=0 5/34」把三件错的算了进去。
没有任何判据守这条边界，所以它们可以长期「绿」着错。

KNOWN_BAD 是**诚实登记**：列出「已知违反本判据」的载体（值 = 忽略）。
纪律：① 新增违例 ⟹ FAIL（不许悄悄扩表）；② 修好一个 ⟹ 必须从表里删掉（表单调下降，
不删也 FAIL，逼着记账）—— 与 `check_ir_j1.py` 的 KNOWN_GAP 同款机制。

用法: python scripts/check_ir_silent_neg.py [--cc <exe>]
退出码: 0 通过 ; 1 失败 ; 2 环境未就绪
"""
import argparse
import io
import os
import subprocess
import sys

sys.stdout.reconfigure(encoding='utf-8', errors='replace')
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
os.chdir(ROOT)
try:
    from qcc_tmp import tmp_dir
except Exception:
    tmp_dir = lambda n: os.path.join('build', n)

# 载体：(标签, 源文本, 说明)
CASES = [
    ('staticarr', 'static int arr[4];\nint f(void) { int *p = arr; return *p; }\n',
     '静态数组名退化（`int *p = arr;` 声明初始化）—— 刀九十六 已修，必须保持 SAME'),
    ('staticstore', 'static int arr[4];\nint *p;\nint f(void) { p = arr; return *p; }\n',
     '静态数组名退化（`p = arr;` **赋值**式写）—— 实测 IR 只发 store 不发源（缺 `lea arr`）'),
    ('ptrchain2', 'struct N { int a; int pad; };\nstruct M { struct N *n; };\n'
                  'int f(struct M *p) { return p->n->a; }\n',
     '指针链中间层（`p->n->a`）—— 刀九十五 已修，必须保持 SAME'),
    ('memberchain3', 'struct A { int v; int pad; };\nstruct B { struct A a; };\n'
                     'struct C { int z; struct B b; };\n'
                     'int f(struct C *p) { return p->b.a.v + p->z; }\n',
     '值字段成员链（`p->b.a.v`）'),
    ('callprintf', 'int printf(const char*, ...);\nint main(void) { printf("hi\\n"); return 0; }\n',
     '内建调用（刀九十一 跨度捕获）'),
    ('staticidx', 'static int arr[4];\nint f(void) { return arr[1]; }\n',
     '静态数组下标（态 12）'),
    ('cmpptr', 'int f(int *p) { return p != 0; }\n',
     '指针比较（64 位，legacy 素九）—— 刀九十八 已修，必须 SAME'),
    ('cmp64', 'int f(long a, long b) { return a < b; }\n',
     '64 位整数比较'),
    ('cmpunsigned', 'int f(unsigned int a) { return a < 128u; }\n',
     '无符号比较（setb vs setl，刀八十三）'),
    ('llsub', 'long long f(long long a, long long b) { return a - b; }\n',
     '真 64 位算术（半属 12）—— 刀一百 已修，必须 SAME'),
    ('lland', 'long long f(long long a, long long b) { return a & b; }\n',
     '64 位位运算（直布 70）—— 刀一百零一 已修'),
    ('llshl', 'long long f(long long a, int n) { return a << n; }\n',
     '64 位移位（容散 68 + 中移64 69）—— 刀一百零一 已修'),
    ('lldiv', 'long long f(long long a, long long b) { return a / b; }\n',
     '64 位除法（缺 __qcc_div_check 守卫，下一刀）'),
    ('llfromi', 'long long f(int a) { return (long long)a; }\n',
     'int→long long 扩宽（缺 movsxd，下一刀）'),
    ('llsub0', 'long long f(long long a) { return 0u - (unsigned long long)a; }\n',
     '64 位减法 + 无符号常量 0u'),
    ('llmod', 'long long f(long long a, long long b) { return a % b; }\n',
     '64 位取模（与除法同源，缺守卫）'),
    ('ifreturn', 'int f(int a) { if (a) return 1; else return 2; }\n',
     'if/return 省掉跨 else 的跳转（刀九十九 条终）'),
    ('ifptr', 'int f(int *p) { if (p) return 1; return 2; }\n',
     '64 位条件 test（指针非空判定）—— 刀一百零四 已修，必须 SAME'),
    ('posrelat64', 'long long f(long long pos, unsigned long long len) {\n'
                   '  if (pos >= 0) return pos;\n'
                   '  else if (0u - (unsigned long long)pos > len) return 0;\n'
                   '  else return (long long)len + pos + 1;\n}\n',
     '64 位三支链（lua u_posrelat 真形状）—— 刀九十九 + 一百'),
    ('posrelat', 'long f(long pos, unsigned long len) {\n'
                 '  if (pos >= 0) return pos;\n'
                 '  else if (0u - (unsigned long)pos > len) return 0;\n'
                 '  else return (long)len + pos + 1;\n}\n',
     'if / else-if / else 三支链（lua u_posrelat 形状）'),
    ('localarr', 'int f(void) { int a[4]; a[0] = 1; return a[1]; }\n',
     '局部数组下标（态 1）'),
    ('derefstore', 'int f(int *p) { *p = 7; return *p; }\n',
     '解引用写（态 8）'),
    ('memberstore', 'struct S { int a; int b; };\nint f(struct S *p) { p->a = 5; return p->b; }\n',
     '指针成员写 + 读（态 9/10）'),
    # ==== 刀一百一十（64 位 / 长标 语义）三条守门载体 ====
    ('argpush64', 'long long g(long long x, long long y) { return x + y; }\n'
                  'long long f(long long a, long long b) { return g(a, b); }\n',
     '压实参 64 位分流（多实参）—— 值引用口径下 节长标 不命中 ⟹ 原走 1 字节 push（−22 B）'),
    ('argpromote', 'long long g(long long x) { return x + 1; }\n'
                   'long long f(int a) { return g(a); }\n',
     '单实参 形参宽化（`查函数长` 分流 + 扩32）—— 原 −14 B'),
    ('castwide', 'long long f(int a) { return (long long)a; }\n'
                 'long long h(char c) { return (long long)c; }\n'
                 'long long k(short s) { return (long long)s; }\n'
                 'long long u(int a) { return (unsigned long long)a; }\n',
     '宽化转型的帧装载扩宽（抽载 帧路径 movsxd + 节点_转型 的透明 L1 载体）—— 原 −3 B / UNIMPL'),
    # ==== 刀一百一十一 / 一百一十二 / 一百一十三 四条守门载体 ====
    ('assign64', 'long long f(long long a, long long b) {\n'
                 '  long long x; x = a + b; return x; }\n',
     '赋值语句的静默源：「静默判据」必须等于「该支会被 IR 回放」—— 原 64 位目标把右值静默后不回放'),
    ('assignchar', 'int f(int a) { char c; c = a; short s; s = a; return c + s; }\n',
     '同上（字符/短整 目标 —— 这两支也不回放）'),
    ('charderef', 'int f(char *p) { return *p; }\n'
                  'int h(const char *s, int i) { return (s[i] & 0xC0) == 0x80; }\n',
     '解引用载宽（char* 读到 4 字节 —— 原 `mov (%rax),%eax`，应 `movzbl`）'),
    ('ptraritharg', 'int g2(const char *s, int i);\n'
                    'int f(const char *s, int i) { return g2(s + i); }\n'
                    'int k(char *s, int i) { return *(s + i); }\n',
     '压实参长标必须取「legacy 求值那一刻」（实参自身代码生成会改 节长标）'),
    ('dowhile', 'int f(int n) { do { n--; } while (n > 0); return n; }\n'
                'int g(char *s, int posi) { do { posi--; } while (posi > 0 && s[posi]); return posi; }\n',
     '后判循环的回边形状：legacy `测试; 非零跳 头`，IR 原 `零跳 出; jmp 头; 出:`（每个 do-while +5 B）'),
    ('addrof_member', 'struct S { int a; int b; };\n'
                      'int *f(struct S *p) { return &p->a; }\n'
                      'int g(struct S *p) { int *q = &p->a; *q = 7; return p->b; }\n',
     '`&p->a` 取址：`免解[]` 侧表让 IR 不加载（原 UNIMPL；无门控版会把字段值当地址）'),
    ('addrof_index', 'int f(int i) { int a[4]; int *p = &a[i]; return *p; }\n'
                     'int g(char *s, int i) { return *(s + i); }\n',
     '`&数组[i]` 取址（态 1 解引=0 重放基）'),
    ('idxstore', 'void f(int i, int v) { int a[4]; a[i] = v; }\n'
                 'struct S { int a[4]; };\n'
                 'void g(struct S *s, int i, int v) { s->a[i] = v; }\n',
     '下标目标写（`存属` 态 11 + `免解[]` 让基只算地址）'),
    ('brk_cont', 'int f(int n) { int i, s = 0; for (i = 0; i < n; i++) { if (i == 3) break; s += i; } return s; }\n'
                 'int g(int n) { int i, s = 0; for (i = 0; i < n; i++) { if (i == 3) continue; s += i; } return s; }\n'
                 'int h(int n) { int s = 0; while (n > 0) { n--; if (n & 1) continue; if (n < 2) break; s += n; } return s; }\n',
     'break / continue 的目标标号（刀一百二十八：legacy 在代码生成期用全局 `帧戊`/`重`，'
     'L1 原本没存 ⟹ IR 一条跳转都不发，`lcorolib −19` / `ltablib −52` 的共同根因）'),
    ('tagunion', 'union Value { struct GCObject *gc; void *p; long (*f)(void); long i; double n; unsigned char ub; };\n'
                 'typedef struct TValue { union Value value_; unsigned char tt_; } TValue;\n'
                 '#define val_(o) ((o)->value_)\n'
                 '#define setfltvalue(obj,x) { TValue *io=(obj); val_(io).n=(x); }\n'
                 '#define setivalue(obj,x) { TValue *io=(obj); val_(io).i=(x); }\n'
                 'void f(double v) { TValue aux; setfltvalue(&aux, v); }\n'
                 'void g(long v) { TValue aux; setivalue(&aux, v); }\n'
                 'long h(TValue *st) { return st->value_.i; }\n'
                 'struct GCObject *k(TValue *st) { return st->value_.gc; }\n',
     'typedef struct + 具名 union + 宏内 `TValue *io=(obj);` + 匿名/具名成员读写'
     '（lua `setfltvalue`/`setivalue` 的真实形状，五种最小变体实测全 SAME；'
     '钉住 IR 对这套形态的能力，防止未来退化）'),
    ('ptr_array_field', 'struct S { int n; char buf[16]; double d; };\n'
                       'char *f(struct S *p) { return p->buf; }\n'
                       'int k(struct S *p) { return p->n; }\n',
     '单级箭头 + **数组字段**（`p->buf` / `p->contents`）—— 刀一百三十八：态 4 的登记门原用'
     '`!末凶一` 排除数组字段，单级登记只认点号、多级要求 深六>=2 ⟹ 三条路都不收 ⟹ `载:成员` UNIMPL。'
     '数组字段的值就是字段地址（legacy `剩未` 只布状态不发字节），IR 走 `中取帧64 + 尾浅` 后 `免解` 短路。'),
    ('setobj_copy', 'typedef union V { long i; double d; void *p; } V;\n'
                    'typedef struct T { V value_; unsigned char tt_; } T;\n'
                    'void setobj(T *io1, T *io2) { io1->value_ = io2->value_; }\n',
     '**8 字节 union 整块拷贝**（lua `setobj` 宏）—— 刀一百五十一：`抽存` 的 态 9/10/11/18 都只覆盖标量写，'
     'legacy 走「箭头成员整块拷贝」分支（目标基址→尾浅→push→源地址→pop rbx→`深利(域四)`）；'
     '新增 态 19，源地址用 legacy 原语 `内存地址(源)`（同一调用、同一字节）。实测 SAME 353 B。'),
    ('deep_member_addr', 'struct B { int v; int pad; };\n'
                        'struct A { struct B b; int x; };\n'
                        'int g(void) { struct A s; s.b.v = 5; s.x = 3; return *(&s.b.v) + s.x; }\n',
     '**值基多级成员链取址**（`&a.b.v`）—— 刀一百六十二：`&` 的操作数从不 `码生成`'
     '（legacy 只用 `内存地址`）⟹ 基节点算子恒 0，走不了「重放基节点」。'
     '修法：在 `例 节点_取址` 的深链分支自算链，把「根槽 + 逐级偏移」记进 `下标属` 态 21，'
     '回放时 `取帧址 + 逆序 尾浅` 复刻同一地址（不调 `内存地址` ⟹ 无全局污染）。SAME。'),
    ('single_member_read', 'struct A { int x; int y; };\n'
                           'int g(void) { struct A s; s.x = 3; s.y = 4; return s.x + s.y; }\n',
     '**单级点号成员值读**（`s.x`）—— 刀一百六十三修的 pre-existing bug：'
     '单级登记块对任何 `s.x` 都写 `免解[数]=1`（注释说是给数组字段用，条件里却没数组判定）'
     '⟹ 值读被编成 `lea`（只发地址、不加载）。实测 `mov eax,[rbp+x]` vs `lea rax,[rbp+x]`。'),
    ('deep_member_read', 'struct B { int v; int pad; };\n'
                         'struct A { struct B b; int x; };\n'
                         'int g(void) { struct A s; s.b.v = 5; s.x = 3; return s.b.v + s.x; }\n',
     '**值基多级成员链的值读**（`a.b.v`）—— 刀一百六十四：与取址同一条 `取帧址 + 逆序 尾浅`，'
     '之后按宽载（态 21 补发射器）。值基无 `缓劣`，与态 6（指针基）只差首条指令。'),
    ('member_chain_write', 'struct B { int v; int pad; };\n'
                          'struct A { struct B b; int x; };\n'
                          'int g(void) { struct A s; s.b.v = 7; s.b.pad = 9; return s.b.v + s.b.pad; }\n',
     '**成员链基写**（`a.b.c = v`）—— 刀一百六十七：`存属` 态 20，静默期自算链、'
     'IR 侧 `取帧址 + 逆序 尾浅` 复刻目标地址（不调 `内存地址`⟹ 无回放期全局污染）。'
     '刀一百五十五曾用「回放期调 `内存地址`」，实测 ltm −288 已回退；本刀是同一形态的正解。'),
    ('ptr_chain_write', 'struct B { int v; int pad; };\n'
                        'struct A { struct B *b; int x; };\n'
                        'int g(struct A *a) { a->b->v = 7; a->b->pad = 9; return a->b->v + a->x; }\n',
     '**指针基成员链写**（`a->b->v = 7`）—— 刀一百六十八：`存属` 态 20 补指针根一支'
     '（发射时多一条 `缓劣` 取指针值），基偏按「指针变量用槽偏、值变量用 `变量基`」分别算。'),
    ('call_many_args', 'int f7(int a,int b,int c,int d,int e,int f,int g) { return a+b+c+d+e+f+g; }\n'
                       'int g(void) { return f7(1,2,3,4,5,6,7); }\n'
                       'int f6(int a,int b,int c,int d,int e,int f) { return a+b+c+d+e+f; }\n'
                       'int h(void) { return f6(1,2,3,4,5,6); }\n',
     '**6/7 个实参的调用** —— 刀一百七十八：`记调属` 的门原为 `参数个数 <= 5`，'
     '6/7 个实参的调用从不记录 ⟹ `调属[]` 为空 ⟹ IR 落「只支持零/单实参」兜底。'
     '实测三处（loadlib 个=7 / lparser 个=7 / lua 个=6）全部卡在这里。'
     '修法：门 5→8 + IR 的 relocate 循环扩到第 5..7 个（照抄 legacy L32448-32451）。'),
    ('deref_store_value', 'int g(void) { int x = 0; char *p = (char *)&x; return ((*p = 3) + 1); }\n'
                          'int h(void) { int y = 0; short *q = (short *)&y; return ((*q = 7) + 2); }\n',
     '**`*p = v` 作表达式的值** —— 刀一百八十三：legacy 实测「1 字节要读回、2 字节不读回」'
     '（`mov [rbx],al` 后 rax 高 24 位不再是存后的值；2 字节时 `mov ebx,eax` 直接复用源值）。'
     '判据用 `存弃[]`（静默期的 `码生成丢弃结果`）：语句形态不读回。'),
    ('vararg64', 'int luaL_error(void *L, const char *fmt, ...);\n'
                 'int f(int i) { return luaL_error(0, "x", i, (long long)i); }\n',
     '变参实参 `(long long)i`：值引用读缺 `movsxd rax,eax`（IR 472 vs legacy 475）—— '
     '刀一百二十五 已修（`IR压实参` 里按 AST 长性 + 值表宽 < 8 补扩）'),
    ('addrof_ptr_member', 'struct S { int a; int b; long c; };\n'
                          'struct S g;\n'
                          'struct S *pg;\n'
                          'int f(void) { return (&g)->b; }\n'
                          'long h(void) { return (&g)->c; }\n'
                          'int k(void) { return (&(*pg))->b; }\n',
     '`(&X)->f` 取址基成员（lua `hvalue(o)->metatable` 形状）—— 态 14 的取址支；'
     '原落 `载:成员` UNIMPL（基型 11 = 取址 不被链回溯认）'),
    # ⚠️ 函数指针变量（`int (*q)(int*) = f; ... q(0)`）**不在本判据**：它在 IR 模式下让
    # **编译器自身崩溃**（rc=0xC0000005，老 control `8CDD3C48` 同样复现 ⟹ 既有 P0）。
    # 崩溃类缺陷另用复现件登记：`scripts/probe/bugs/ir_fptr_var_crash.c`（不进任何判据，
    # 免得判据运行即触发崩溃风暴）。修好后应把它加回本表并保持 SAME。
    ('arrarg', 'extern void g(long long);\n'
               'extern void h(void *);\n'
               'void f(long long *q) { g(q[0]); }\n'
               'void k(void **v)    { h(v[0]); }\n'
               'void m(long long a) { g(a); }\n',
     '**下标实参的压栈宽度**（2026-11-11 新增护栏）：`q[0]`（`long long *`，8 B 元素）'
     '两侧都压 64 位（`sub rsp,8;mov`）；`v[0]`（`void **`）两侧都压 32 位（`push rax`）；'
     '`m(long long)` 压 64 位。实测 SAME 648 B。'
     '由来：排查 `lcorolib +11` 时创建的最小载体，证明「普通下标」两侧一致，'
     '把差异精确收窄到「基不是普通变量」的形态。'),
    ('aggarg', 'extern void g(void *, const char *, const char *);\n'
               'static const char *const names[] = { "a", "b", "c" };\n'
               'void f(int i) { g(0, "x", names[i]); }\n',
     '**指针数组元素作实参**（2026-11-11）：索引是**变量**时两侧一致（都 `push rax`，SAME 969 B）。'
     '与 `arrarg` 同族，进一步收窄 `lcorolib +11` —— 该处索引是**函数调用**（`auxstatus(L,co)`），'
     '差异只在「索引是调用」这一形态。'),
    ('fbin', 'double fmul(double a, double b) { return a * b; }\n'
             'double fadd(double a, double b) { return a + b; }\n'
             'double fdiv(double a, double b) { return a / b; }\n'
             'double fsub(double a, double b) { return a - b; }\n',
     '**浮点二元算术**（2026-11-11 刀二百零二）：`码生成浮` 的 xmm 通道在 IR 侧补了载体 ——'
     '`栈置`(压浮0) + `长转`(addsd/subsd/mulsd/divsd)，整数操作数经新增 `中整转浮`(cvtsi2sd)。'
     '同时把「浮点返回」也纳入 IR 接管（原漏了 return 跳转，每函数 −5 B）。实测 SAME 750 B。'),
    ('fret', 'int imul2(int a, int b) { return a * b; }\n'
             'double dmul2(double a, double b) { return a * b; }\n',
     '**浮点返回 vs 整数返回**（2026-11-11 刀二百零二）：两种返回并存，守住「浮点返回支也必须'
     '走 IR 接管并补 return 跳转」这一判据（修前浮点支少一条 `jmp 尾声`）。实测 SAME 485 B。'),
    ('shifts', 'unsigned shr3(unsigned x) { return x >> 3; }\n'
               'int sar3(int x) { return x >> 3; }\n'
               'unsigned long long shr64(unsigned long long x) { return x >> 3; }\n'
               'long long sar64(long long x) { return x >> 3; }\n'
               'unsigned shl3(unsigned x) { return x << 3; }\n',
     '**逻辑右移 vs 算术右移**（2026-11-11 刀二百零四）：`unsigned >>` 必须 `shr`（逻辑），'
     '`signed >>` 必须 `sar`。修前 IR 一律 `sar` 且把 `>>` 也放进「免推栈」路径'
     '（legacy 的免推栈显式排除 `帧寅`）⟹ 无符号右移编错。实测 SAME 324 B。'),
    ('deep6p', 'struct P5 { int v; };\n'
               'struct N5 { struct P5 *p5; };\n'
               'struct N4 { struct N5 n5; };\n'
               'struct N3 { struct N4 n4; };\n'
               'struct N2 { struct N3 n3; };\n'
               'struct N1 { struct N2 n2; };\n'
               'struct N0 { struct N1 n1; };\n'
               'int deep6p(struct N0 *p) { return p->n1.n2.n3.n4.n5.p5->v; }\n'
               'void deep6pw(struct N0 *p, int x) { p->n1.n2.n3.n4.n5.p5->v = x; }\n',
     '**6 级成员链 + 中间指针**（2026-11-11 刀二百零五）：链侧表原只 4 槽而回溯上限是 8 ⟹'
     '第 5 级起 `% 4` 绕回覆盖前级（该指针级的 `缓劣` 丢失）⟹ IR 少 3 字节。扩到 8 槽后 SAME 460 B。'),
    ('addrofchain', 'struct S { int a; int b; long long c; };\n'
                    'int *f(struct S *p) { return &p->b; }\n'
                    'int *f2(struct S *p) { return &p->a; }\n'
                    'long long *f3(struct S *p) { return &p->c; }\n'
                    'struct T { struct S inner; };\n'
                    'int *k(struct T *t) { return &t->inner.b; }\n',
     '**取址的成员链**（2026-11-11 刀二百零六）：`&p->f` / `&t->inner.b`。写路径走 `内存地址()`，'
     '成员节点从不被码生成 ⟹ 无态 ⟹ `取址:非局部`（lapi 861 / lfunc 215 / lgc 686 三文件同签名）。'
     '本刀在取址处补登 态 6/21 + 免解。实测 SAME 565 B（静态 `&gs.b` 与按值结构体形参 `&s.b` 仍 fail-closed）。'),
    ('arrfield', 'struct S { int a; int arr[4]; };\n'
                 'int *f(struct S *p) { return p->arr; }\n'
                 'int g(struct S *p) { return p->arr[1]; }\n'
                 'int h(struct S *p) { p->arr[2] = 7; return p->arr[2]; }\n',
     '**数组字段作值/作基**（2026-11-11 刀二百零七）：`p->arr` 的值就是字段地址。'
     '态 4 的登记原门 `末凶一 && 免解==0` 把「求址上下文」的数组字段全排除 ⟹ 旧编译器 UNIMPL，'
     '新编译器 SAME 607 B。'),
    ('ternstore', 'struct V { int v; };\n'
                  'struct V gx, gy;\n'
                  'void f(int c) { (c ? &gx : &gy)->v = 7; }\n',
     '**三元基成员写**（2026-11-11 刀二百零九）：`(c ? a : b)->v = 7` 的目标基是三元节点。'
     'legacy 的写路径用 `内存地址(三元)`（物化 `__tern_<节点>` 帧临时）—— 三元自身有 L1 记录，'
     '但 `存属` 的既有态（9/10/11/18/19/20）都只认变量根 ⟹ 旧编译器 UNIMPL。'
     '本刀在赋值入口登记 态 22（基+域偏，基置 `免解`），`抽存` 态 22 发基地址再 `尾浅`。'
     '实测 SAME 538 B。'),
    ('fmem', 'struct S { double d; float f; int i; double e[2]; };\n'
             'double g1(struct S *p) { return p->d; }\n'
             'float  g2(struct S *p) { return p->f; }\n'
             'double g4(struct S *p) { return p->e[1]; }\n'
             'double g5(struct S *p) { return p->d * 2.0; }\n',
     '**浮点成员读**（2026-11-11 刀二百四十五/二百四十六）：`码生成浮` 的成员分支直接发'
     '`浮取 xmm0`，从不经过 `例 节点_成员` ⟹ 该节点无 L1 记录 ⟹ IR 必落「节点:无L1算子」'
     '（`fmem.c` g1 节点=6；lcode `tonumeral` 节点=336 同签）。'
     '本刀补 抽载 + 态 4（箭头基）/ 态 3（值基）+ 浮性，IR 侧态 4 新增浮点分支。'
     '实测 SAME 675 B（静态 `gs.d` 与素系形参 `s.d` 仍 fail-closed）。'),
    ('deep11', 'struct L0 { int **pp; int v; };\n'
               'struct L1 { struct L0 a; };\n'
               'struct L2 { struct L1 a; };\n'
               'struct L3 { struct L2 a; };\n'
               'struct L4 { struct L3 a; };\n'
               'struct L5 { struct L4 a; };\n'
               'struct L6 { struct L5 a; };\n'
               'struct L7 { struct L6 a; };\n'
               'struct L8 { struct L7 a; };\n'
               'struct L9 { struct L8 a; };\n'
               'struct L10 { struct L9 a; };\n'
               'int  **f(struct L10 *p) { return &p->a.a.a.a.a.a.a.a.a.a.pp; }\n'
               'int   g(struct L10 *p) { return p->a.a.a.a.a.a.a.a.a.a.v; }\n'
               'void  h(struct L10 *p) { p->a.a.a.a.a.a.a.a.a.a.v = 7; }\n',
     '**深成员链（11 级）**（2026-11-11 刀二百一十一）：链侧表原为 8 槽（刀二百零五从 4 扩到 8），'
     '但 lua 的 `clLvalue(s2v(x))` 宏展开链深达 **11**（`code → p → & → l → cl → gc → value_ → & → val → p → func → ci`），'
     '超出后回溯在 `深六 == 8` 处退出、`根六 = -1` ⟹ 不登记 ⟹ 取址/载 落 UNIMPL'
     '（实测 ldebug `currentpc`：`[CH3] 深=8 根=-1`）。'
     '本刀把 `链偏/链诀` 扩到 **16 槽**并把四处回溯上限统一到 16。'
     '实测 SAME 513 B；lua33 里 ldebug 的卡点从 `载:成员 304` 前进到 `无L1算子 settraps 450`。'),
    ('widths', 'int   f1(char *p) { return *p; }\n'
               'int   f2(unsigned char *p) { return *p; }\n'
               'int   f3(short *p) { return *p; }\n'
               'int   f4(unsigned short *p) { return *p; }\n'
               'int   f5(int *p) { return *p; }\n'
               'long long f6(long long *p) { return *p; }\n'
               'double f7(double *p) { return *p; }\n'
               'float  f8(float *p) { return *p; }\n',
     '**解引用读的四宽度**（2026-11-11 刀二百六十七）：`*(T*)` 的 L1 类型码原只修了 i8/i16（刀112），'
     '浮点仍落 3(i32) ⟹ IR 发 4 字节整数读（`double f7(double *p){return *p;}`：legacy `movsd xmm0,[rax]` vs IR `mov eax,[rax]`）。'
     '本刀补「元素宽 >= 4 且指向浮点 ⟹ 类型码 10」+ `载:解引用` 的浮点分支。实测 SAME 937 B。'),
    ('derefinc', 'int f(char *p) { int b = *p++; int c = *++p; return b + c; }\n'
                 'int g(char *p) { return *p++; }\n'
                 'typedef struct { int n; const char *p; } ZIO;\n'
                 'static int getc1 (ZIO *Z) { return (Z->n-- > 0 ? (unsigned char)(*(Z->p++)) : -1); }\n'
                 'int h(ZIO *Z) { int s = 0; int i; for (i = 0; i < 4; i++) s += getc1(Z); return s; }\n',
     '**`*(p++)` / `*(++p)` 的宽度**（2026-11-11 刀二百六十八）：宽度判据直接 `节点指元素(增减节点)` 得 0 '
     '⟹ 类型码 3(i32) ⟹ 4 字节读（这正是 lundump `loadByte` 的 `movzx` vs `mov` 差异源）。'
     'legacy（L35374）在求宽度前把 增减/前置增减 剥到指针操作数，本刀逐字照抄。实测 SAME（`_inc.c` 与 `loadByte` 复刻件都 SAME）。'),
    ('postinc', 'int f(char *p, char c) { *p++ = c; return 0; }\n'
                'int g(char *p, int v) { *p++ = v; return 0; }\n'
                'int h(long long *p, long long v) { *p++ = v; return 0; }\n'
                'char *m(char *p, char c) { *p++ = c; return p; }\n',
     '**`*p++ = v` / `*++p = v`**（2026-11-11 刀二百一十三）：写目标是「增减包解引用」。'
     'legacy `内存地址(增减)` 的顺序是 `码生成(增减)` → `反附(__pinc_<节点>)` → `取帧址(临时)`，'
     '`存属` 没有这个态 ⟹ 落「存储形态未支持」（ldblib `unmakemask` 的 `*p++ = flagsof[i].c`）。'
     '本刀新增态 31：源 → [扩32] → push → 增减（重发字节）→ 反附(临时) → 取帧址(临时) → pop rbx → 按宽存。'
     '实测 SAME 862 B。**副产物**：它同时消除了「不补此态就开 i92 会让编译器自身 0xC0000005」的路径（§181）。'),
]

# 诚实登记：已知违反「rc=0 ⟹ 逐字节同」的载体。修好后必须删行（不删 ⟹ FAIL）。
KNOWN_BAD = {
    # 纪律：新增违例必须登记；修好必须删行（不删也 FAIL）。
    # 已修并删行：staticstore（刀九十七）、指针比较（刀九十八）、posrelat（刀九十九 + 一百）。
    # 刀一百一十 已修并删行：lldiv / llmod（压实参 64 位分流）、llfromi（抽载 帧路径 movsxd）。
    # ⟹ 本表当前**空**。留空是正常的：它只承载「已知还没修好的静默错产物」。
}


def build(cc, src, out, ir, work):
    p = os.path.join(work, src if src.endswith('.c') else src)
    env = dict(os.environ)
    env['QCC_RT_PATH'] = os.path.join(ROOT, 'srclib', 'qcc_rt.c')
    env.pop('QCC_CODEGEN', None)
    env.pop('QCC_IR_STATS', None)
    if ir:
        env['QCC_CODEGEN'] = 'ir'
    for e in ('', '.obj', '.o'):
        q = out + e
        if os.path.exists(q):
            os.remove(q)
    r = subprocess.run([cc, p, '-c', '-o', out], capture_output=True, env=env, timeout=600)
    msg = ((r.stdout or b'') + (r.stderr or b'')).decode('utf-8', 'replace')
    return r.returncode, msg, out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cc', default='qcc_zh_v1.exe')
    a = ap.parse_args()
    if not os.path.exists(a.cc):
        print('[ERR] 缺件: %s' % a.cc)
        return 2
    W = tmp_dir('ir_silent')

    bad = []          # 违例（rc=0 且字节不同）但未登记
    stale = []        # 已登记但实际已修好 / 已不再违例
    rows = []
    seen_bad = set()
    for tag, src, desc in CASES:
        sp = os.path.join(W, tag + '.c')
        io.open(sp, 'w', encoding='utf-8', newline='\n').write(src)
        lrc, lmsg, lp = build(a.cc, sp, os.path.join(W, tag + '.leg'), False, W)
        irc, imsg, ip = build(a.cc, sp, os.path.join(W, tag + '.ir'), True, W)
        if lrc != 0:
            rows.append('%-14s LEGACY-FAIL rc=%d' % (tag, lrc))
            bad.append('%s: legacy 编译失败 rc=%d' % (tag, lrc))
            continue
        if irc != 0:
            ok_unimpl = 'IR-BACKEND-UNIMPL' in imsg
            rows.append('%-14s UNIMPL%s' % (tag, '' if ok_unimpl else ' (未报 UNIMPL!)'))
            if not ok_unimpl:
                bad.append('%s: rc=%d 但没报 IR-BACKEND-UNIMPL（静默失败）' % (tag, irc))
            continue
        la = open(lp, 'rb').read() if os.path.exists(lp) else b''
        ia = open(ip, 'rb').read() if os.path.exists(ip) else b''
        if la == ia:
            rows.append('%-14s SAME (%d B)' % (tag, len(la)))
            if tag in KNOWN_BAD:
                stale.append(tag)
        else:
            seen_bad.add(tag)
            mark = '（已登记）' if tag in KNOWN_BAD else '★未登记★'
            rows.append('%-14s **DIFF** legacy=%d ir=%d %s' % (tag, len(la), len(ia), mark))
            if tag not in KNOWN_BAD:
                bad.append('%s: rc=0 但字节不同 %d vs %d —— 静默错产物（%s）' % (tag, len(la), len(ia), desc))
    for tag in KNOWN_BAD:
        if tag not in seen_bad:
            stale.append(tag)

    print('== 静默错产物判据（rc=0 必须逐字节同）==')
    print('\n'.join(rows))
    print()
    if bad:
        print('[FAIL] 出现未登记的静默错产物 / 静默失败：')
        for b in bad:
            print('  - ' + b)
    if stale:
        print('[FAIL] KNOWN_BAD 过期（已不再违例）⟹ 请从表里删掉，保持单调下降：')
        for s in stale:
            print('  - ' + s)
    if bad or stale:
        return 1
    print('[通过 PASS] check_ir_silent_neg：%d 个载体无一「rc=0 但字节不同」；%d 条已知违例已登记'
          % (len(CASES), len(KNOWN_BAD)))
    return 0


if __name__ == '__main__':
    sys.exit(main())
