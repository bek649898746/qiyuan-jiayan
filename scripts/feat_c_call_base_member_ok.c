// @EXPECTED out:A=3 B=30 C=7 D=70
/* A71-续2 (2026-09-15): 调用基 成员访问 —— **正向** 合法路径不误伤 + 取值正确。
   病灶: 序七() 缺 节点_调用 分支 ⟹ `f().字段` (构值返回调用作成员基) 类型解析落 -1
         → 成员静态检查静默跳过 (f().typo 生成 codegen 垃圾);
   根治: 序七() 增 调用 分支 (调用构返 = 按值返回构体类型序; 退 调用构指 = 返回指针的所指构体)。
   本锚钉: 构值返回调用 (f().字段, H1i 物化 __mret 帧临时) 与 指针返回调用 (f()->字段)
          两路径取值与 gcc 逐字节一致。
   注: 赋值基 `(s = t).字段` 是 **既存缺陷** (赋值表达式构值语义 — 见 neg 锚 与台账);
       其成员检查已由同一分支修复, 但 codegen 的构值赋值临时物化 未实现 ⟹ 本锚不钉该路径。 */
#include <stdio.h>

struct S { int a; long b; };

struct S mk(int v) { struct S r; r.a = v; r.b = v * 10; return r; }
struct S *mkp(void) { static struct S s = { 7, 70 }; return &s; }

int main(void) {
    printf("A=%d B=%ld ", mk(3).a, mk(3).b);
    printf("C=%d D=%ld\n", mkp()->a, mkp()->b);
    return 0;
}
/* @EXPECTED out:A=3 B=30 C=7 D=70 */
