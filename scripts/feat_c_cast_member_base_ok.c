/* A2003 + A2004 (2026-09-16): 转型 成员基 —— 检查 与 取值 双修 (语料闸 — git merge 根治锚)。
   病灶 (两个, 同一表达式 `((结构体 标签*)X)->f`):
     ① A2003 检查侧: 后缀链() 的 **嵌套链** 检查 (L6900) 沿 子0 一路下钻收集成员节点, 再从 根类型
        逐级推字段 —— 完全不看 链中间 是否有 **转型结果** ⟹ ((结构体 标签*)变量.字段)->x
        用「字段自身类型」(object) 检查 ⟹ 误报 A9001 (git builtin/merge.c L1254)。
     ② A2004 取值侧: 内存地址() 的成员分支 用 递归给出 的「基自身声明类型」查字段 ⟹
        字段序(object, "tag") < 0 ⟹ 静默计(2) 返 -1 ⟹ 地址表达式 被丢弃 ⟹ 读落未初始化 / 写丢失。
   修法: ① 链遍历 遇 节转序[寻] != 0 即 断 (类型由转型决定);
         ② 内存地址 成员分支 递归后 若 基节点 有 节转序, 用 序七(基) 覆盖 减构序。
   本锚钉: ((结构体 标签*)全局结构体成员)->字段 的 写 + 读, 与 gcc 逐字节一致。
   ⚠️ 已知未修 (已记挂账, 故本锚不含该形态): `((结构体 标签*)函数()->字段)->x` 与
      `((结构体 标签*)指针变量)->x` 走另一条 codegen 路径, 取值仍错。 */
#include <stdio.h>

struct object { int a; };
struct tag { struct object obj; int tag; };
struct remote { struct object *obj; };

static struct tag T;
static struct remote R = { &T.obj };

int main(void) {
    ((struct tag *)R.obj)->tag = 42;
    printf("a=%d b=%d\n", T.obj.a, ((struct tag *)R.obj)->tag);
    return 0;
}
/* @EXPECTED out:a=0 b=42 */
