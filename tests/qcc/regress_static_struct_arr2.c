// @EXPECTED exit:0
// @EXPECTED out:1-2
// 复现: 全局 static struct 数组 + brace init (command_list 模式)
#include <stdio.h>

struct cmdname_help {
    const char *name;
    const char *help;
    unsigned int category;
};
static struct cmdname_help command_list[] = {
    { "add", "Add file contents to the index", 1 },
    { "am", "Apply a series of patches", 2 },
};

int main(void) {
    printf("%u-%u\n", command_list[0].category, command_list[1].category);
    return 0;
}
