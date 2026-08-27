#include <stdio.h>

/* RAII 模拟: C 资源获取 + 清理 (手动/集中清理模式) */
static int open_res(int *handle) {
    *handle = 42;
    return 0;
}
static void close_res(int handle) {
    (void)handle;
    /* 模拟释放 */
}

typedef struct {
    int fd;
    char *buf;
} FileRes;

static int file_open(FileRes *f) {
    f->fd = 7;
    f->buf = "data";
    return 0;
}
static void file_close(FileRes *f) {
    f->fd = -1;
    f->buf = NULL;
}

int main(void) {
    int h = -1;
    int r = 0;
    if (open_res(&h) != 0) return 1;
    r = h * 2;
    close_res(h);          /* 清理 */

    FileRes f;
    if (file_open(&f) != 0) return 2;
    char *data = f.buf;
    file_close(&f);        /* 清理 */

    printf("r=%d fd=%d data=%s\n", r, f.fd, data);
    if (r != 84) return 3;
    if (f.fd != -1) return 4;
    if (data[0] != 'd') return 5;
    printf("PASS\n");
    return 0;
}
