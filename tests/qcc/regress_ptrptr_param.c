/* 回归: struct T ** 双指针参数解引用必须 8 字节加载 (fix 2026-08-18)
   根因: 参数解析 if(p_stidx>=0) esz=struct 大小 → arr_esz=72 → case 12
   el=72 不匹配 8/4/2 → 落到 movzbl 字节加载 → 指针截断成低字节 (0x90)
   → rename_tempfile 收到 tempfile=0x90 → close_tempfile_gently SEGV */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

struct vlh { struct vlh *next, *prev; };

struct tempfile {
    volatile struct vlh list;
    volatile int fd;
    char *fp;
    int owner;
    char *filename;
    char *directory;
};

int probe(struct tempfile **tempfile_p, const char *path) {
    struct tempfile *tempfile = *tempfile_p;
    return tempfile ? tempfile->fd : -1;
}

int main(void) {
    struct tempfile tf;
    memset(&tf, 0, sizeof(tf));
    tf.fd = 7;
    struct tempfile *tp = &tf;
    int r = probe(&tp, "x");
    printf("r=%d\n", r);
    return r != 7;
}
