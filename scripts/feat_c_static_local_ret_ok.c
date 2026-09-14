/* feat_c_static_local_ret_ok.c — C 兼容模式: 返回 `static` 局部变量的地址 **不是** 悬垂 (P0-16 根治 2026-09-14)
   病灶: A3105 直形检查 (`反双一`) 判据 = 「局部 (变序 >= 解析基) 且 非指针」, **漏判 static**。
         `static T x;` 声明在函数内, 但 **存储期 = 程序期** → `return &x;` 合法 (C 6.2.4p3)。
         同 语义 的 另一处检查 (引用 NLL 悬垂) 早已 用 `!变量表[x].是否静态标` 豁免 ⟹ 两处 判据 不一致。
   实测: git `attr.c` 的 `default_attr_source`: `static struct object_id attr_source;` + `return &attr_source;` → 误报 A3105。
   本锚: static 局部 buffer 返回并跨调用复用 (gcc 逐字节对照)。 */
// @EXPECTED out:v=42
// @EXPECTED out:calls=2
// @EXPECTED out:same=1
// @EXPECTED exit:0
#include <stdio.h>

struct oid { int a; };
static int calls;

static struct oid *get(void) {
	static struct oid cached;      /* static 局部: 存储期 = 程序期, 返回 &cached 合法 */
	if (cached.a == 0)
		cached.a = 41;
	calls++;
	return &cached;
}

int main(void) {
	struct oid *p = get();
	p->a += 1;
	printf("v=%d\n", get()->a);
	printf("calls=%d\n", calls);
	printf("same=%d\n", get() == p);
	return 0;
}
