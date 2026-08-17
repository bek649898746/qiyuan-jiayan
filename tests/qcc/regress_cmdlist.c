// @EXPECTED exit:0
// @EXPECTED out:24-24-1-8
// 复现 help.c command_list: 字符串指针 + category 立即数混合 struct 数组
#include <stdio.h>

struct cmdname_help {
    const char *name;
    const char *help;
    unsigned int category;
};
static struct cmdname_help command_list[] = {
    { "add", "Add file contents to the index", 1 },
    { "am", "Apply a series of patches", 2 },
    { "apply", "Apply a patch", 4 },
    { "archive", "Create an archive", 8 },
};

int main(void) {
    printf("%d-%d-%u-%u\n", (int)sizeof(struct cmdname_help),
           (int)((char*)&command_list[1] - (char*)&command_list[0]),
           command_list[0].category, command_list[3].category);
    return 0;
}
