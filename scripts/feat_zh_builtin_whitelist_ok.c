// @EXPECTED out:PASS
#include <stdint.h>
#include <errno.h>
#include <direct.h>
#include <stdio.h>

/* 内建白名单三缺口 —— ORD-20261104-111
 *
 * 病灶（修前，实测）：
 *   ① `<stdint.h>` 的 C99 `*_C` 宏家族 **6/6 全缺** ⟹ `UINT64_C(...)` 报 A1001；
 *   ② `<errno.h>` 只登记了 ENOENT/ENOMEM/EINVAL 三个，**16 个次常用码缺**（EAGAIN/EINTR/…）；
 *   ③ Win64 `struct _stat` **能编过但字段读垃圾**（实测 st_size=4576032 应 42）—— 比报错更坏。
 *
 * 修法（都在「系统头 include 点」注入，复用既有 C 解析器）：
 *   ① `<stdint.h>`：注入 INT8_C..UINTMAX_C 十个宏（`##` 拼接 + U/LL/ULL 后缀）；
 *   ② errno：命宏加 16 个（MSVC 值）；
 *   ③ `<direct.h>`：注入 `struct _stat` / `struct _stat64` 真实字段布局。
 *
 * 判据：值必须**真的对**（不只看能否编过）。灵敏度：修前 ①A1001 ②A1001 ③静默错值。
 */

static int bad = 0;

int main(void) {
    /* ① *_C 家族：值 + 宽度 */
    unsigned long long u64 = UINT64_C(0xad93d23594c935a9);
    long long i64 = INT64_C(0);
    unsigned int u32 = UINT32_C(7);
    int i32 = INT32_C(3);
    unsigned char u8 = UINT8_C(200);
    long long imax = INTMAX_C(9);

    if ((int)(u64 >> 60) != 10) { printf("FAIL UINT64_C %d\n", (int)(u64 >> 60)); bad++; }
    if (i64 != 0) { printf("FAIL INT64_C %lld\n", i64); bad++; }
    if (u32 != 7 || i32 != 3 || u8 != 200 || imax != 9) { printf("FAIL C_MISC\n"); bad++; }

    /* ② errno：16 个次常用码（MSVC 值） */
    if (EAGAIN != 11) { printf("FAIL EAGAIN %d\n", EAGAIN); bad++; }
    if (EINTR != 4 || EBADF != 9 || EEXIST != 17 || ENOSPC != 28) { printf("FAIL errno A\n"); bad++; }
    if (EPIPE != 32 || EDOM != 33 || EFBIG != 27 || EMFILE != 24) { printf("FAIL errno B\n"); bad++; }
    if (EACCES != 13 || EBUSY != 16 || EISDIR != 21 || EIO != 5) { printf("FAIL errno C\n"); bad++; }
    if (ENOTTY != 25 || E2BIG != 7 || ERANGE != 34) { printf("FAIL errno D\n"); bad++; }

    /* ③ struct _stat：字段真的落对偏移 */
    struct _stat st;
    st.st_dev = 1;
    st.st_ino = 2;
    st.st_mode = 0100644;
    st.st_nlink = 1;
    st.st_size = 42;
    st.st_mtime = 1700000000;
    if (st.st_size != 42) { printf("FAIL st_size %lld\n", st.st_size); bad++; }
    if ((int)st.st_mode != 33188) { printf("FAIL st_mode %d\n", (int)st.st_mode); bad++; }
    if (st.st_mtime != 1700000000) { printf("FAIL st_mtime %lld\n", st.st_mtime); bad++; }
    if ((int)st.st_dev != 1 || (int)st.st_ino != 2 || (int)st.st_nlink != 1) { printf("FAIL stat misc\n"); bad++; }

    if (bad) { printf("FAIL total=%d\n", bad); return 1; }
    printf("PASS\n");
    return 0;
}
