// @EXPECTED out:RPTF 42
/* 锚: 「返回指针的调用」作成员访问的基 (`f(...)->字段`) —— C111 (第 104 站, 2026-09-18)
   发现路径: 跨版本产物对比 (`r16a_objdiff.py`) —— C110 → C112 之间
     `lua/lparser.c`(4 个函数) 与 `lua/ltable.c` 的 .o **变了**;
     反汇编 (`r16d_fndiff.py`, 以 `init_var` 为例) 显示 C112 **新增**了整段:
        call getlocalvardesc ; 存 rax 到帧槽 ; 取槽址 ; 解引用 ; + 字段偏移 ; 读
     —— 即 C110 **完全没发射** `getlocalvardesc(fs, vidx)->vd.ridx`
     (源码 `lparser.c` L270 `e->u.var.ridx = getlocalvardesc(fs, vidx)->vd.ridx;` 的右侧整段丢失)。
   根因 (同 C111 的 m3): `内存地址()` 的 `节点_调用` 分支只处理「按值返回构体」,
     返回指针的调用 `返 -1` ⟹ 外层成员访问整段不发射 ⟹ **静默错值**。
   修法: 新增 helper `构指临` (8 字节隐藏帧临时) + 成员访问处分配 (`__cret_<节点序>`)
     + `内存地址` 调用分支的「返回指针」通路 (求值 → `反附` 存槽 → `取帧址`)。
   锚灵敏度 (实测): C110 版 输出 `RPTF 4208928` rc=1 (**红**); C112 版 `RPTF 42` rc=0;
     gcc 对照 `RPTF 42` rc=0。

   ⚠️⚠️ **本锚必须保持最小 —— 实测到的「上下文敏感性」(重要发现)**:
     只要在同文件里**再加任何一处** `x->vd.字段` 形式的**变量基**访问
     (例如 `Vardesc *q=…; q->vd.a;` 或 `static int take(Vardesc*v){return v->vd.a;}`),
     **旧编译器也会变绿**! 原因是旧编译器在 `内存地址` 拿不到基类型时, 会走
     「遍历所有类型找第一个含该字段的结构体」的兜底扫描 —— 而那类访问会在**解析期**
     触发一次字段查表, 使该扫描**恰好命中**正确的类型。
     ⟹ 旧编译器的正确与否取决于「同文件上下文」/**脆弱**; 新版走确定性的
     「调用返回指针 → 物化 → 加字段偏移」通路, 与上下文无关。
     (这也解释了为何 lparser.c 会红: 它的类型表有几百个类型, 兜底扫描命中的是别的类型。)
     覆写本锚时**请勿**添加对照语句 —— 加了就测不出缺陷了。 */
#include <stdio.h>

typedef struct { union { int a; } vd; } Vardesc;
typedef struct { int firstlocal; Vardesc *arr[8]; } Dyn;
typedef struct { Dyn *dyd; } LuaState;

static Vardesc slots[8];
static Dyn dyn;

static Vardesc *getlocalvardesc(LuaState *ls, int v) { return ls->dyd->arr[v]; }

int main(void) {
    int r = 0;
    LuaState ls;
    slots[3].vd.a = 42;
    dyn.arr[3] = &slots[3];
    ls.dyd = &dyn;
    int x = getlocalvardesc(&ls, 3)->vd.a;   /* 缺陷位: 返回指针的调用 -> 字段 */
    if (x != 42) r |= 1;
    printf("RPTF %d\n", x);
    return r;
}
