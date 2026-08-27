#include <stdio.h>

/* 字符串插值: sprintf 编译期拼接 — "Hello {name}, score={n}"
   C 模拟: 格式化拼接 (qcc 内联 printf/sprintf 引擎) */

static void format_score(char *out, const char *name, int score) {
    sprintf(out, "Player %s got score %d", name, score);
}

int main(void) {
    char buf[128];

    /* 1. 基本插值: %s + %d — "Hello world" (d 在 [10]) */
    sprintf(buf, "Hello %s", "world");
    if (buf[0] != 'H' || buf[1] != 'e' || buf[6] != 'w') return 1;
    if (buf[10] != 'd') return 2;

    /* 2. 多段: "x=10 y=20" (y 后的 2 在 [7]) */
    sprintf(buf, "x=%d y=%d", 10, 20);
    if (buf[0] != 'x' || buf[2] != '1' || buf[7] != '2') return 3;

    /* 3. 函数封装插值 — "Player XiaMi got score 828" (8 在 [23]) */
    format_score(buf, "XiaMi", 828);
    if (buf[7] != 'X' || buf[23] != '8') return 4;

    /* 4. 十六进制插值 — "hex=ff" */
    sprintf(buf, "hex=%x", 255);
    if (buf[4] != 'f' || buf[5] != 'f') return 5;

    /* 5. 字符插值 — "ch=A" */
    sprintf(buf, "ch=%c", 'A');
    if (buf[3] != 'A') return 6;

    /* 6. 变长插值: "[qcc#42]" */
    {
        int id = 42;
        const char *tag = "qcc";
        sprintf(buf, "[%s#%d]", tag, id);
        if (buf[0] != '[' || buf[1] != 'q' || buf[5] != '4' || buf[7] != ']') return 7;
    }

    /* 7. 插值 + 字符串长度: "Player Test got score 99" = 24 字符 */
    format_score(buf, "Test", 99);
    int len = 0;
    while (buf[len] != 0) len++;
    if (len != 24) return 8;

    /* 8. 内联值插值 */
    {
        int n = 7;
        sprintf(buf, "n=%d", n);
        if (buf[2] != '7') return 9;
    }

    printf("s1=%s s2=%s s3=%s\n", "Hello world", "x=10 y=20", "Player XiaMi got score 828");
    printf("PASS\n");
    return 0;
}
