# jylib — 甲言基础库（第一个库）

> 用**甲言（中文 C 方言编译器）**编译的通用 C 库，产物可被任意 C ABI 兼容工具链（gcc / clang / msvc）链接调用。
> 语料来自旧壳武库中挑选的通用件，经改造为库形态（公开化 / 统一前缀 / 补头文件 / 剥离 main）。

**版本** 1.3.0 · **锚定编译器** `36B99BEE`（10-31 R905l）· **状态** ✅ 甲言版与 gcc 版**双轨全通**（6 模块 × 双轨 = 12 自测全 exit=0，且全部**零外部依赖**）

---

## 模块

| 模块 | 头文件 | 内容 | 标准 | 状态 |
|:--|:--|:--|:--|:--|
| `jylib_sha256` | `include/jylib_sha256.h` | SHA-256 / HMAC-SHA256 / HKDF | FIPS 180-4 · FIPS 198-1 · RFC 5869 | ✅ 已验 |
| `jylib_sort` | `include/jylib_sort.h` | 排序（快排 / 插排 / 堆排）+ 二分查找 + 已排序判据 | 与 C 标准 `qsort`/`bsearch` 同语义 | ✅ 已验 |
| `jylib_crc32` | `include/jylib_crc32.h` | CRC-32（零状态版 + 查表版 + 流式） | CRC-32/ISO-HDLC（以太网/PNG/ZIP） | ✅ 已验 |
| `jylib_base64` | `include/jylib_base64.h` | Base64 编解码 | RFC 4648 §4 | ✅ 已验 |
| `jylib_bitmap` | `include/jylib_bitmap.h` | 位图（动态位数 + 集合运算 + 位计数/遍历） | — | ✅ 已验 |
| `jylib_arena` | `include/jylib_arena.h` | 线性分配器（对齐 + 快照回滚 + 峰值统计） | — | ✅ 已验 |
| `jylib_pool` | *(待补)* | 对象池（无锁空闲链 + 线程本地缓存） | — | 规划中 |

### jylib_sort — 已验证

**泛型排序**：任意元素宽度 + 比较器（与 C 标准 `qsort` 同语义，零学习成本）。

```c
#include "jylib_sort.h"

int cmp_int(const void *a, const void *b) {
    int x = *(const int*)a, y = *(const int*)b;
    return x < y ? -1 : (x > y ? 1 : 0);
}

int 数[] = {42, 7, 88, 15, 3};
jy_qsort(数, 5, sizeof(int), cmp_int);          /* 快排 */
if (jy_is_sorted(数, 5, sizeof(int), cmp_int)) { /* 判据 */ }
long 位 = jy_bsearch(&数[0], 数, 5, sizeof(int), cmp_int);  /* 二分 */

/* 整数快捷路径（免比较器） */
jy_qsort_int(数, 5);
long 位2 = jy_bsearch_int(7, 数, 5);
```

**提供三种算法**，各有适用场景：

| 函数 | 稳定性 | 时间复杂度 | 何时用 |
|:--|:--|:--|:--|
| `jy_qsort` | 不稳定 | 平均 O(n log n) | 默认选择 |
| `jy_insertion_sort` | **稳定** | O(n²)，小数组/近有序时最快 | 已基本有序的输入 |
| `jy_heapsort` | 不稳定 | **最坏也 O(n log n)** | 要求最坏保证时 |

**自测覆盖**（10 个用例 + 5 万元素压力）：

```
[OK] 用例1 乱序 12 元素          [OK] 用例6 交叉对拍 快排/堆排/插排 × 1000 元素
[OK] 用例2 已排序 200 元素       [OK] 用例7 二分查找 256 元素全命中 + 不存在返 -1
[OK] 用例3 逆序 200 元素         [OK] 用例8 is_sorted 正反两态
[OK] 用例4 全相等 300 元素       [OK] 用例9 整数快捷路径 + 整数二分
[OK] 用例5 边界 0/1/2 元素       [OK] 用例10 结构体排序（元素宽度 8 字节）
```

压力：**50,000 元素 × 6 种分布**（随机含负数 / 已排序 / 逆序 / 全相等 / 两值交替 / 三值重复），双轨 0 失败 —— 后四种正是快排的经典退化场景。

> 判据设计说明：不只验「已有序」，还验 **元素集合不变**（排序前后「和」与「异或」必须相同），防止丢元素/重复元素这类隐蔽错误；三算法结果还必须**逐字节一致**。

### jylib_crc32 — 已验证

**两套 API，各有取舍**：

| 接口 | 优点 | 缺点 |
|:--|:--|:--|
| `jy_crc32()` | **零状态**、无初始化、彻底线程安全 | 位运算逐位算，比查表慢约 8 倍 |
| `jy_crc32_make_table()` + `jy_crc32_fast()` | 快 | 需先建表；表由调用者持有 |

```c
#include "jylib_crc32.h"

/* 简单用法：零状态 */
unsigned int crc = jy_crc32("abc", 3);          /* 0x352441C2 */

/* 高性能用法：自带表 */
unsigned int tab[JY_CRC32_TABLE_SIZE];
jy_crc32_make_table(tab);
unsigned int crc2 = jy_crc32_fast(tab, "abc", 3);

/* 流式：分多次喂入 */
jy_crc32_ctx ctx;
jy_crc32_init(&ctx);
jy_crc32_update(&ctx, "ab", 2);
jy_crc32_update(&ctx, "c", 1);
unsigned int crc3 = jy_crc32_final(&ctx);       /* 同样 0x352441C2 */
```

**为什么不提供「懒初始化的全局静态表」**：原实现 `crc32.c` 用 `static uint32_t crc32_table[256]` + `static int crc32_init`，首次调用时的竞争会让两个线程都建表，且外部无法重置。本库把表交给调用者持有 ⟹ 零隐藏状态。

**标准向量**（CRC-32/ISO-HDLC）：

```
""                                                = 0x00000000  [OK]
"a"                                               = 0xE8B7BE43  [OK]
"abc"                                             = 0x352441C2  [OK]
"message digest"                                  = 0x20159D7F  [OK]
"123456789"                                       = 0xCBF43926  [OK]  ← 标准校验值
"The quick brown fox jumps over the lazy dog"      = 0x414FA339  [OK]
三路交叉（零状态 / 查表 / 流式）                    结果一致      [OK]
任意分段喂（1/2/3/5/8 字节五种切分）                结果一致      [OK]
```

### jylib_base64 — 已验证

```c
#include "jylib_base64.h"

char out[128];
long n = jy_base64_encode("foobar", 6, out, sizeof(out));    /* "Zm9vYmFy" */

unsigned char dec[128];
long m = jy_base64_decode(out, (unsigned long)n, dec, sizeof(dec));  /* 还原 6 字节 */
```

**RFC 4648 §4 官方向量全过**（7 条），另有二进制往返（含 `0x00`/`0xFF`）与四种边界。

> ⚠️ 本库编码表用**字符数组初始化**，不用 `"ABC..."` 字符串字面量 —— 二者语义等价，但字符串字面量会生成 `IMAGE_REL_AMD64_ADDR32` 重定位，导致甲言 `.obj` 无法被 gcc/ld 链接（详见 `jylib_sha256.c` 文件头）。

### jylib_bitmap — 已验证

**动态位数**（原语料硬编码 128 位上界），存储由调用者提供：

```c
#include "jylib_bitmap.h"

unsigned int 存储[8];               /* 8 × 32 = 256 位 */
jy_bitmap bm;
jy_bitmap_init(&bm, 存储, 8, 200);  /* 200 位 */

jy_bitmap_set(&bm, 0);
jy_bitmap_set(&bm, 199);
jy_bitmap_toggle(&bm, 100);

unsigned long n = jy_bitmap_count(&bm);       /* 置位总数 */
long first = jy_bitmap_first(&bm);            /* 第一个置位点 */
long nx = jy_bitmap_next(&bm, 1);             /* 1 之后第一个 */

/* 集合运算（三个位图位数必须相同）*/
jy_bitmap_and(&结果, &A, &B);
jy_bitmap_or (&结果, &A, &B);
jy_bitmap_not(&结果, &A);
```

**7 个用例全过**：单位操作四态 / 边界位数（1/31/32/33/100，**尾巴位不计入 count**）/
count+first+next 与暴力对照 / 5 种集合运算与逐位暴力对照 / 原地运算 r==a / 相等比较 / 1024 位压测。

> 原 `bitset.c` 用 `__builtin_ctz` / `__builtin_popcount`（GCC 内建，非 C 标准），
> 本库自己实现，任何编译器都能编。

### jylib_arena — 已验证

**线性分配**，一次性回收 O(1)，带对齐与快照回滚：

```c
#include "jylib_arena.h"

unsigned char 内存[4096];
jy_arena a;
jy_arena_init(&a, 内存, sizeof(内存));

int *p = (int *)jy_arena_alloc(&a, sizeof(int));
double *d = (double *)jy_arena_alloc_aligned(&a, sizeof(double), 16);

jy_arena_mark m = jy_arena_save(&a);      /* 快照 */
/* ... 临时分配 ... */
jy_arena_rollback(&a, m);                 /* 回滚，空间可复用 */

jy_arena_reset(&a);                       /* 一次性全部回收 */
```

> **为什么不让库自己 malloc**：那就不叫零依赖了，还把「分配失败」引入本不该关心它的层。
> 存储由调用者提供（栈数组 / 静态数组 / 自己的池都行）。
>
> 原语料把 64 KB 缓冲**内嵌在结构体里**（`unsigned char data[65536]`）—— 栈上声明直接爆栈；
> 本版改为指针 + 容量。

### jylib_sha256 — 已验证

由**同一份源**分别用甲言与 gcc 编译，标准向量全部通过（`pass=6 fail=0`）：

```
SHA256("")            = E3B0C44298FC1C149AFBF4C8996FB92427AE41E4649B934CA495991B7852B855  [OK]
SHA256("abc")         = BA7816BF8F01CFEA414140DE5DAE2223B00361A396177A9CB410FF61F20015AD  [OK]
SHA256(分段三喂)       = D7A8FBB307D7809469CA9ABCB0082E4F8D5651E46D3CDB762D02D0BF37C9E592  [OK]
SHA256(200B乱步长分段==一次性)                                                            [OK]
HMAC-RFC4231-TC2      = 5BDCC146BF60754E6A042426089575C75A003F089D2739839DEC58B964EC3843  [OK]
HKDF-RFC5869-TC1      = 3CB25F25FAACD57A90434F64D0362F2A2D2D0A90CF1A5A4C5DB02D56ECC4C5BF34007208D5B887185865  [OK]
```

> 判据 R 说明：真值未知处**不写期望值**。块边界测试采用「乱步长分段 == 一次性」的**内部自洽**比对，而非编造十六进制常量。

---

## 用法

```c
#include "jylib_sha256.h"

unsigned char digest[JY_SHA256_DIGEST_SIZE];
char hex[JY_SHA256_HEX_SIZE];

jy_sha256((const unsigned char*)"abc", 3, digest);
jy_sha256_hex(digest, hex);
printf("%s\n", hex);
```

```bash
# 编译你的程序，链接库模块
gcc -I jylib/include  your_app.c  jylib/lib/jylib_sha256.obj  -o your_app
```

---

## 构建

```powershell
# 双轨构建：同时产出「甲言编译版」与「gcc 编译版」，便于交叉验证
powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1
```

产物落在 `lib/`：

| 文件 | 编译者 | 用途 |
|:--|:--|:--|
| `jylib_sha256.jy.obj` | 甲言 `qcc_x86` | 本库的主交付物 |
| `jylib_sha256.gcc.obj` | gcc | 可信基准 / 交叉验证 |

**注意**：构建脚本会把源复制到纯 ASCII 工作路径 —— **甲言拒收中文路径参数**（实测传中文目录名会收到乱码）。

---

## ✅ 构建状态（2026-10-31 更新 · 6 模块双轨全通 · 全部零外部依赖）

**gcc 版：完全可用** ✅
**甲言版：完全可用** ✅ —— 6 个模块 × 双轨 = 12 个自测全 exit=0

```
[jylib_arena]  甲言 -> OK   gcc -> OK
[jylib_base64] 甲言 -> OK   gcc -> OK
[jylib_bitmap] 甲言 -> OK   gcc -> OK
[jylib_crc32]  甲言 -> OK   gcc -> OK
[jylib_sha256] 甲言 -> OK   gcc -> OK
[jylib_sort]   甲言 -> OK   gcc -> OK
[selftest]  12 条全 exit=0（6 模块 × gcc/jy 两轨）
复现: powershell -NoProfile -ExecutionPolicy Bypass -File jylib\build.ps1

零外部依赖判据（库 .obj 不得引用运行时符号）:
  jylib_arena   jylib_base64   jylib_bitmap
  jylib_crc32   jylib_sha256   jylib_sort       ← 全部 ✅ 零未定义符号
```

> 编译器选择：build.ps1 **seed 优先**。seed 是 `scripts\build_zh.ps1` 验证过的不动点产物；
> `qcc_zh_v2.exe` 是临时中间产物，可能来自未收敛构建（实测 10-30：v2=`12AA33D3`，seed=`36B99BEE`）。
>
> 注：`jylib_sha256` 的 `pass=6 fail=0` 明细输出已改为统一的 `; exit=N` 契约，
> 判据不变（exit=0 即全过），只是驱动模板统一了。

### 历史：三个已定位并**全部修复**的编译器缺陷

| # | 缺陷 | 后果 | 状态 |
|:--|:--|:--|:--|
| ① | 字符串字面量生成 `IMAGE_REL_AMD64_ADDR32` | gcc/ld 拒绝链接 | ✅ **已修**（启元 Q0：12 处 → `lea rax,[rip+disp32]`；ADDR32 命中 99.2% → **0**） |
| ② | `memset`/`memcpy` 内联用 32 位寄存器承载 64 位指针 | 运行时 `0xC0000005` | ✅ **已修**（虾米第 262 站 ③） |
| ③ | `__qcc_ginit` 内联 memset 同一毛病 | 运行时 `0xC0000005` | ✅ **已修**（同上，阻塞级） |

### 本库暴露并促成的后续四刀（启元 Q1/Q7/Q8/Q10）

| 刀 | 缺陷 | 形态 | 锚 |
|:--|:--|:--|:--|
| Q1 | 指针自增/自减丢高 32 位 | `p++` / `--p` | `1CE4FCE1` |
| Q7 | 数组名 + 整数 落 32 位加法 | `缓冲区 + 64` | `D6CFC7EB` |
| Q8 | `->` 字段基址 32 位装载 | `ctx->count = 0` | `58F36411` |
| Q10 | `扩32` 对**成员数组地址**无条件符号扩展 | `ctx->buf + ctx->blen` | **`646FC353`** |

⟹ **jylib 是甲言第一个"给别人用"的库**，也是把这六刀串起来的载体。

gdb 取证：

```
rcx  0x7ff6fd496000     ← 正确的 64 位地址
r8   0xfd496000         ← 高 32 位被清零
=>   mov %r9b,(%r8)     ← 用截断地址写内存 ⟹ SIGSEGV
```

**为什么甲言自己链自己能跑**：甲言产物的最终映像基址低（`0x400000` 量级），截断后恰好仍落在合法范围；
**一旦交给 gcc/ld 链接（基址 `0x140000000`、运行时 `0x7ff6...`），立刻现形。**

⟹ **"甲言的库能给别人调用"这件事此前从未被真正验证过** —— 甲言的验证一直在自己闭环里。
建库这一步正好把它逼出来了。

---

## ✅ 硬判据：库 `.obj` 必须零外部依赖

**判据**：`objdump -t jylib_xxx.jy.obj` 的输出中，不得出现任何 `*UND*` 符号。

**为什么**：库的用途是「给别人链接」。若 `.obj` 里留有未定义符号，别人链接时就得额外提供那个符号——那就不是「零依赖库」了。

**实测踩过的坑**（2026-10-31，建 arena 时）：

```c
aligned = (a->pos + align - 1UL) / align * align;   /* 除法 */
```
甲言把整数除法编译成对 **`__qcc_div_check64`**（运行时库 qcc_rt.c 的除零检查）的调用：
```
ld.exe: jylib_arena.jy.obj:(.text+0x3f9): undefined reference to `__qcc_div_check64'
```
⟹ 库 `.obj` 无法被 gcc/ld 链接。

**修法**：`align` 恒是 2 的幂 ⟹ 用位掩码上取整替代除法：
```c
mask = align - 1UL;
aligned = (a->pos + mask) & (~mask);      /* 等价于 ceil(pos/align)*align */
```

`build.ps1` 已内置此判据（每个库逐个 `objdump` 检查 `*UND*`），6 个库当前全部零未定义符号。

---

## ⚠️ 本库发现并已规避的编译器缺陷：D107（`long` 家族指针下标）

**现象**（锚定 `36B99BEE`，2026-10-30，jylib 建库时实测）：

以 `long *` / `unsigned long *` 作**形参**时，`p[i]` 的下标运算按 **8 字节步长**做，
而 Windows LLP64 下 `unsigned long` 只有 **4 字节** ⟹ 写入位置整体错位。

最小复现（`ptrprobe.c`，五种指针类型对照）：

```
gcc : unsigned long * → 1 2 3 4 5 6 7 8   ✅
甲言: unsigned long * → 1 0 2 0 3 0 4 0   ❌ （写到了索引 0,2,4,6）
甲言: long *          → 1 0 2 0 3 0 4 0   ❌
甲言: unsigned int *  → 1 2 3 4 5 6 7 8   ✅
甲言: int *           → 1 2 3 4 5 6 7 8   ✅
甲言: unsigned short* → 1 2 3 4 5 6 7 8   ✅
```

**结论**：仅 `long` / `unsigned long` 指针参数受影响，`int` / `short` 正常。
属「`指元素` 尺寸登记漏了 long 家族」——与 Q8（`->` 字段基址 32 位装载）同族。

**规避**：`jylib_crc32` 全程用 `unsigned int`（32 位 CRC 本就用不着 64 位）。
**递刀**：已记入台账 D107，待编译器侧修复后本规避可撤。

---
## 设计约定

1. **零外部依赖** —— 除 C 标准库外不链接任何东西（`jylib_sha256` 连 `<string.h>` 都已去除）
2. **C ABI 兼容** —— 甲言编译的 `.obj` 可被 gcc 链接（在三缺陷修复后）
3. **符号前缀 `jy_`** —— 避免与 OpenSSL 等第三方实现的 `sha256()` 冲突
4. **头文件 C/C++ 双可用** —— `extern "C"` 包裹
5. **真值可验** —— 每个模块自带标准向量自测，别人一跑就知道真伪

---

## 目录

```
jylib/
├── README.md                          本文件
├── build.ps1                          双轨构建脚本（自动发现 src/jylib_*.c 与 *_selftest.c）
├── include/
│   ├── jylib.h                        总入口（版本 + jy_selftest）
│   ├── jylib_sha256.h                 SHA 模块公开 API
│   ├── jylib_sort.h                   排序模块公开 API
│   ├── jylib_crc32.h                  CRC-32 模块公开 API
│   ├── jylib_base64.h                 Base64 模块公开 API
│   ├── jylib_bitmap.h                 位图模块公开 API
│   └── jylib_arena.h                  线性分配器公开 API
├── src/
│   ├── jylib_sha256.c                 核心（零字符串字面量）
│   ├── jylib_sha256_selftest.c        标准向量自测（含 printf）
│   ├── jylib_sort.c                   核心（零字符串字面量）
│   ├── jylib_sort_selftest.c          10 用例自测（含 printf）
│   ├── jylib_crc32.c                  核心（零字符串字面量）
│   ├── jylib_crc32_selftest.c         标准向量 + 三路交叉自测
│   ├── jylib_base64.c                 核心（零字符串字面量）
│   ├── jylib_base64_selftest.c        RFC 4648 向量 + 往返自测
│   ├── jylib_bitmap.c                 核心（零字符串字面量）
│   ├── jylib_bitmap_selftest.c        7 用例自测（含暴力对照）
│   ├── jylib_arena.c                  核心（零字符串字面量 / 零除法）
│   └── jylib_arena_selftest.c         7 用例自测
├── lib/                               构建产物（*.jy.obj + *.gcc.obj）
└── tests/
    └── selftest_main.c                sha256 自测驱动（其余驱动由 build.ps1 自动生成）
```

---

## 语料来源与改造记录

| 原文件 | 改造 |
|:--|:--|
| 旧壳武库 `sha256_mini.c`（223 行，零依赖） | ① 全部 `static` 函数公开化（原文件外部根本调不到）② 统一 `jy_` 前缀 ③ 结构体从 `.c` 内移入头文件 ④ 剥离 `main` ⑤ 去掉未使用的 `<math.h>` ⑥ 新增 `jy_sha256_hex` |
| 旧壳武库 `qsort.c`（44 行，`int*` 专用递归快排） | ① 公开化并改名 `jy_qsort` ② 统一 `jy_` 前缀 ③ **泛型化**：由 `int*` 扩到 `void*` + 元素宽度 + 比较器 ④ 新增插入排序 / 堆排序 / 二分查找 / 已排序判据 ⑤ 剥离 `main` ⑥ **健壮化**：原实现取 `a[(l+r)/2]` 作中轴，已排序输入退化为 O(n²) ⟹ 改三数取中 + 小区间切插排 |

| 旧壳武库 `crc32.c`（71 行，查表法） | ① 公开化并加 `jy_` 前缀 ② **去全局**：`static crc32_table[256]` + `static crc32_init` 改为「表由调用者提供」③ 新增流式 init/update/final + 零状态慢速版 ④ 剥离 `main` ⑤ 规避甲言 `long` 家族指针缺陷（见 D107），全程用 `unsigned int` |
| 旧壳武库 `base64.c`（76 行） | ① 公开化并加 `jy_` 前缀 ② **编码表改字符数组**（原 `static const char b64[] = "ABC..."` 是字符串字面量 ⟹ ADDR32 重定位 ⟹ 无法 gcc 链接）③ decode 由 `strlen(in)` 改显式 `inlen` ④ 新增 `jy_base64_encoded_size` ⑤ 剥离 `main` |
| 旧壳武库 `bitmap.c`（77 行）+ `bitset.c`（54 行） | ① **两份合并**（语义重叠）② **动态化**：原 `BM_WORDS=4` / `BITSET_WORDS=4` 硬编码 128 位 ⟹ 位数由调用者指定 ③ **去 builtin**：原用 `__builtin_ctz`/`__builtin_popcount` ⟹ 自己实现 popcount/ctz ④ 去全局 ⑤ 剥离 `main` 与 `bm_dump` |
| 旧壳武库 `arena.c`（66 行） | ① 公开化并加 `jy_` 前缀 ② **外部化**：原把 64 KB 缓冲内嵌在结构体里（栈上声明爆栈）⟹ 改调用者提供缓冲 ③ 动态容量 ④ **增对齐**（原按上次结束位置直接切，double/指针可能未对齐）⑤ 增快照回滚 ⑥ **零除法**：`/ align` 改位掩码上取整（除法会生成 `__qcc_div_check64` 调用，库 .obj 无法独立链接）|

**`sha256` 算法实现逐字保留，未做任何逻辑改动** —— 通过 gcc 版标准向量全过验证改造无损。

**`sort` 的分区与递归主干沿用原实现思路**，但有一处**必须改**：原版用「值拷贝」`int p = a[(l+r)/2]` 保存中轴；泛型版无法值拷贝任意宽度元素，若改用数组内指针当 pivot，分区过程中的 swap 会改写 pivot 指向的值 ⟹ 分区错（本次实测复现：1000 元素用例失败，堆排/插排均正确且元素集合未变，定位即此）。修法 = 三数取中后把中值换到 `lo` 位固定，整个分区期间 `lo` 不参与交换，最后归位。
