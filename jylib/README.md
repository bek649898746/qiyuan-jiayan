# jylib — 甲言基础库（第一个库）

> 用**甲言（中文 C 方言编译器）**编译的通用 C 库，产物可被任意 C ABI 兼容工具链（gcc / clang / msvc）链接调用。
> 语料来自旧壳武库中挑选的通用件，经改造为库形态（公开化 / 统一前缀 / 补头文件 / 剥离 main）。

**版本** 1.1.0 · **锚定编译器** `36B99BEE`（10-29 R905l）· **状态** ✅ 甲言版与 gcc 版**双轨全通**（2 模块 × 双轨 = 4 自测全 exit=0）

---

## 模块

| 模块 | 头文件 | 内容 | 标准 | 状态 |
|:--|:--|:--|:--|:--|
| `jylib_sha256` | `include/jylib_sha256.h` | SHA-256 / HMAC-SHA256 / HKDF | FIPS 180-4 · FIPS 198-1 · RFC 5869 | ✅ 已验 |
| `jylib_sort` | `include/jylib_sort.h` | 排序（快排 / 插排 / 堆排）+ 二分查找 + 已排序判据 | 与 C 标准 `qsort`/`bsearch` 同语义 | ✅ 已验 |
| `jylib_arena` | *(待补)* | 内存竞技场（O(1) 分配 + 快照回滚 + 子竞技场） | — | 规划中 |
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

## ✅ 构建状态（2026-10-29 更新 · 2 模块双轨全通）

**gcc 版：完全可用** ✅
**甲言版：完全可用** ✅ —— 两个模块双轨自测全 exit=0

```
[jylib_sha256]  甲言 -> OK   gcc -> OK
[jylib_sort]    甲言 -> OK   gcc -> OK
[selftest]
  [jylib_sha256/gcc] exit=0
  [jylib_sha256/jy]  exit=0
  [jylib_sort/gcc]   exit=0
  [jylib_sort/jy]    exit=0
复现: powershell -NoProfile -ExecutionPolicy Bypass -File jylib\build.ps1
```

> 注：10-29 复验时 `jylib_sha256` 的 `pass=6 fail=0` 明细输出已改为统一的 `; exit=N` 契约，
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
│   └── jylib_sort.h                   排序模块公开 API
├── src/
│   ├── jylib_sha256.c                 核心（零字符串字面量）
│   ├── jylib_sha256_selftest.c        标准向量自测（含 printf）
│   ├── jylib_sort.c                   核心（零字符串字面量）
│   └── jylib_sort_selftest.c          10 用例自测（含 printf）
├── lib/                               构建产物（*.jy.obj + *.gcc.obj）
└── tests/
    └── selftest_main.c                sha256 自测驱动（sort 的驱动由 build.ps1 自动生成）
```

---

## 语料来源与改造记录

| 原文件 | 改造 |
|:--|:--|
| 旧壳武库 `sha256_mini.c`（223 行，零依赖） | ① 全部 `static` 函数公开化（原文件外部根本调不到）② 统一 `jy_` 前缀 ③ 结构体从 `.c` 内移入头文件 ④ 剥离 `main` ⑤ 去掉未使用的 `<math.h>` ⑥ 新增 `jy_sha256_hex` |
| 旧壳武库 `qsort.c`（44 行，`int*` 专用递归快排） | ① 公开化并改名 `jy_qsort` ② 统一 `jy_` 前缀 ③ **泛型化**：由 `int*` 扩到 `void*` + 元素宽度 + 比较器 ④ 新增插入排序 / 堆排序 / 二分查找 / 已排序判据 ⑤ 剥离 `main` ⑥ **健壮化**：原实现取 `a[(l+r)/2]` 作中轴，已排序输入退化为 O(n²) ⟹ 改三数取中 + 小区间切插排 |

**`sha256` 算法实现逐字保留，未做任何逻辑改动** —— 通过 gcc 版标准向量全过验证改造无损。

**`sort` 的分区与递归主干沿用原实现思路**，但有一处**必须改**：原版用「值拷贝」`int p = a[(l+r)/2]` 保存中轴；泛型版无法值拷贝任意宽度元素，若改用数组内指针当 pivot，分区过程中的 swap 会改写 pivot 指向的值 ⟹ 分区错（本次实测复现：1000 元素用例失败，堆排/插排均正确且元素集合未变，定位即此）。修法 = 三数取中后把中值换到 `lo` 位固定，整个分区期间 `lo` 不参与交换，最后归位。
