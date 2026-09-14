/* feat_zh_incdir_search_ok.c — 命令行 -I <目录> 头文件搜索 (P0-4 根治 2026-09-14)
   病灶: 原 `-I <路径>` 语义 = "预包含该文件" (读文件 + 前插, = GCC 的 -include), **没有搜索目录概念**;
         而真实多目录项目 (git/zlib/lua/curl) 的构建**全部依赖 `-I<dir>`** ⟹ 这些项目无法编译。
   实测(改前): `-I scratch_gl\inc` 对 `#include "myhdr.h"` 报 "找不到文件"; 对 `#include <myhdr.h>` **静默忽略**(更危险)。
   修法: 判据 = "能否读到该路径" —— 能读 ⟹ 是文件 ⟹ 原语义(前插); 读不到 ⟹ 当目录 ⟹ 加入头文件搜索表
         (搜索链: 当前源文件目录 → 父链回溯 → **-I 目录** → cwd)。两种语义零冲突共存。
   本锚: 引号形式 + 尖括号形式 双覆盖, 头文件在 scripts/inctest/ (靠 @ARG 传 -I)。 */
// @ARG -I scripts/inctest
// @EXPECTED out:quote=42
// @EXPECTED out:angle=42
// @EXPECTED out:both=84
// @EXPECTED exit:0
#include "inctest.h"
#include <inctest.h>
#include <stdio.h>
int main(void) {
    printf("quote=%d\n", INCTEST_VAL);
    printf("angle=%d\n", INCTEST_VAL);
    printf("both=%d\n", INCTEST_VAL + INCTEST_VAL);
    return 0;
}
