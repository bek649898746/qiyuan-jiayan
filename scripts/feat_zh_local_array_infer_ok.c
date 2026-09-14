/* feat_zh_local_array_infer_ok.c — 局部「空维度数组 + 花括号初始化」长度推断 (P0-11 根治 2026-09-14)
   病灶 (同一语义 三条路径, 两处 bug):
     ① 命名标签路径: `常量表达式求值(&维数)` 对空 [] **返回成功**(维数 0) ⟹ 空维度标记 空玖 恒 0
        ⟹ "未定长推断元素数"整段被跳过 ⟹ 长度 恒 1 ⟹ 码生成写 a[1] → A2001。
     ② 命名标签推断循环 落点 `词位 += 1` 落在**外层 `{`** 上 → 被「跳过内层初始化器」整块吞掉 → 数 恒 1。
     ③ 通用推断段: 元素以 `}` 闭合后 尾逗号标记 前辰 未清零 → 退出循环时误减 1 (typedef 场景 少 1)。
   实测: git apply.c / archive.c 的 `struct option X[] = {...}` (局部, 空维度) 全卡此。
   本锚: struct / typedef / 尾逗号 三形态, 用 sizeof 断言 (与 gcc 逐字节对照)。 */
// @EXPECTED out:n=3,3,5,3
// @EXPECTED exit:0
#include <stdio.h>

struct option { const char *l; int v; int f; };
typedef struct { const char *l; int v; int f; } opt_t;

int f_struct(void) {
	struct option a[] = {
		{ "n0", 0, 0 },
		{ "n1", 1, 0 },
		{ "n2", 2, 0 }
	};
	return (int)(sizeof(a) / sizeof(a[0]));
}

int f_typedef(void) {
	opt_t b[] = {
		{ "n0", 0, 0 },
		{ "n1", 1, 0 },
		{ "n2", 2, 0 }
	};
	return (int)(sizeof(b) / sizeof(b[0]));
}

int f_five(void) {
	opt_t c[] = {
		{ "n0", 0, 0 },
		{ "n1", 1, 0 },
		{ "n2", 2, 0 },
		{ "n3", 3, 0 },
		{ "n4", 4, 0 }
	};
	return (int)(sizeof(c) / sizeof(c[0]));
}

int f_trailing(void) {
	struct option d[] = {
		{ "n0", 0, 0 },
		{ "n1", 1, 0 },
		{ "n2", 2, 0 },
	};
	return (int)(sizeof(d) / sizeof(d[0]));
}

int main(void) {
	printf("n=%d,%d,%d,%d\n", f_struct(), f_typedef(), f_five(), f_trailing());
	return 0;
}
