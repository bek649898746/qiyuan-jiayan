# 启元编译器引导流程（seed 公约）

## 核心原则
- **源码是真相，种子二进制是引导入口。** 与 rustc/Go 的自举方式一致。
- 日常构建 **零 gcc 依赖**：种子 → 编译源码 → 得自己（固定点）。

## 产物
- 源码：`qcc_x86.c`（英语版，含中文词法层）+ `qcc.jy`（甲言版，翻译生成）
- 运行时：`qcc_rt.c`（自宿主运行时，编译时 prepend）
- 种子：`qcc_AC606F5A_seed.exe`（自举固定点，2026-08-03 记录九）

## 日常重建（无需 gcc）
```powershell
# 英语链（从工作区根目录运行！qcc_rt.c 用相对路径读取）
seed\qcc_4DEFCB43_seed.exe qcc_x86.c -o new.exe    # 自 4DEFCB43
new.exe qcc_x86.c -o verify.exe                     # 验证 == 4DEFCB43
```
注意：必须从工作区根目录运行（"srclib/qcc_rt.c" 是相对路径）。
`..\` 或绝对路径输入文件在自宿主编译器下可读，但 qcc_rt.c prepend 依赖 CWD。

## 2026-08-03 自举记录（九）：route_learn 全链打通（不完善不自举）
route_learn（递归扫日志→算权重→写 JSON）此前编译可通过但**运行即崩**，且三段等价 H1≠H2
（H2 代码短 0x108 字节）。逐层挖出 8 个根因，全部修复后 route_learn 正常运行、
4/4 等价全过、自举收敛到新固定点 **4DEFCB43**（记录九正文；先收敛到 AC606F5A，随后移除 [SZM] 调试打印再收敛到 4DEFCB43）：
1. **stypes 字段槽 16→32**（struct 表溢出）：EngineStat 有 17 个字段，第 17 个字段名
   （"starvation_boosted"）写进 `fnames[16]` **越界覆盖 foffs[0]** → `name` 字段偏移变成
   "star"（0x72617473）→ `stats[i].name` 寻址 1.9GB 外 → strcmp 崩溃。这是 route_learn
   崩溃+偏移错乱的**总根因**。
2. **sizeof(TypedefName)**（typedef struct）：`sizeof(EngineStat)` 未处理 typedef 名 →
   返回 4 → `memset(e,0,4)` 只清零 4 字节。
3. **未知类型局部声明**（size_t/time_t 来自被跳过的 #include）：`size_t n = strlen(name);`
   整句被丢弃 → n 未初始化 → memcpy 计数=垃圾。
4. **sret 目标误判指针变量**：`EngineStat* e = find_stat(...)` 被当作 >8B 结构体 sret 目标
   （只查 st_idx+sz，没查 var_pesz）→ 跳过了 rax→e 的存储 → e 永远是垃圾指针。
5. **alu_ri imm8 符号扩展**：`add eax, 128` 用 imm8 编码（83 C0 80）→ 变成 -128 →
   `e->total_calls = 42`（偏移 128）写到结构体**前面**。>127 偏移改 imm32（81 /0 id）。
   同时 asm_zh **缺 `运即` handler**（-S 路径丢指令）→ 补上 imm8/imm32 双形式。
6. **snprintf 变内置**：qcc_rt 的 snprintf 只转发 1 个 %s，route_learn 的 `"%s\%s"`
   （2 个 %s）读栈垃圾 → sprintf %s 拷贝循环崩。snprintf(buf,n,fmt,args...) 注册为
   内置（emit_sprintf 参数化 argstart=3 + rdx=fmt 重排）。
7. **字符串宏**：`#define LOG_DIR "memory\call_log"` 此前只支持数字宏 → LOG_DIR 编译成
   NULL → `scan_logs(NULL)` 崩。新增 `str_macros` 表：值存解码串，**使用时**才复制进
   str_tbl（保证 ID 顺序=引用顺序）。
8. **字符串/浮点预放置**（ID 序）：str_place/dbl_place 原本懒放置（引用序），与 .字串/.浮点
   的 ID 序排放不一致（空字符串 `""`、静态初始化串会乱序）→ H1 数据池与 H2 漂移。
   gen_code 开头**预放置全部字符串和浮点**（ID 序），布局确定化。
另外：**typedef struct 数组** `P arr[3]` 被第一个 blk() 分支（td_is 拦截）注册成 int 数组
（无 st_idx）→ 数组元素字段读写全垃圾（回归，随记录九一并修）。
回归：128 测 120 过（8 个历史失败：addr_probe/arg5_call_raw/bare_import/null_deref/step1/2/6）；
三段等价 4/4；自举 seed==v1==v2==4DEFCB43。

## 2026-08-03 自举记录（八）：自宿主 -S 模式打通（sgap）
**自宿主编译器的 -S 模式此前生成空 .asm.asm**（known-gap）。两个根因：
1. **`#define ASM asm_emit` 被自宿主词法器注册成 0**：词法器只支持 `#define NAME NUMBER`，
   `asm_emit` 非数字 → ASM=0，所有 ASM() 调用点在自宿主版全部失效。
2. **asm_emit 用 varargs（va_list/vfprintf）**：qcc 子集不支持 → 自宿主版无法编译。
修复：
- **asm_emit 改固定 3 参 `char *a,char *b,char *c` + fprintf**（qcc 内置 fprintf 写 stdout，
  gcc 版 libc 写文件）。335 处 `ASM(` 全部改 `asm_emit(`，%d/%c 参数包 `(char*)(long long)x`
  （gcc/qcc 都合法，int 值存低 32 位），%s 直传。`.浮点 %g` 改 `asm_emit_dbl(".浮点 %f", v)`。
- **词法器 `\0` 转义修复**：`".text\0\0\0"` 的 `\0` 原被当字面 '0'（段名 `.text000`）。
  补 `else if (s[i]=='0') str_tbl[j++]=0`。
- **段名内嵌 null 截断**：str_place 用 `while(*s)` 遇 null 停 → `.text\0\0\0` 截断。
  write_pe 段名改 `fwrite(".text",1,5,f); pad(f,3)` 分开写。
- **自宿主 -S 调用方式**：qcc 内置 fprintf 写 stdout → `cmd /c "qcc -S file.c > file.asm"`
  （PowerShell `>` 重定向是 UTF-16，会乱码，必须用 cmd）。
验证：自宿主 -S 编译 nest_a~g/fp4~5/bigstruct/p2test 17/17 H1==H2；大程序 asm_zh.c
（468KB）和 **qcc_x86.c 自身（2.4MB）H1==H2**。新固定点 **FCAD3D57**（gcc 产物直接
就是固定点，无一步漂移）。自举链终极闭环：seed→编译器→-S 编译自己→asm_zh→字节一致。

## 2026-08-03 自举记录（七）：fnptr 字段全场景 + fnptr 标记修复
- **fp4/fp5**：double 返回的 fnptr 数组元素调用、struct 字段 fnptr（声明/赋值/调用/
  按值传参/回调）全部打通。修复 5 类：① struct 体 4 处解析循环不处理 fnptr 字段
  （`int (*cb)(int,int)` 的 `(` 卡死循环）；② case-10 store 宽度（fsz 1/4/8）；
  ③ case-15 load 侧 fnptr 64 位读取；④ case-4 callee 识别（callee 挂最后一个 child）；
  ⑤ bigsz 数组元素按值传参。
- **-S 路径补全**：fn_patches2（`移动 r0, FN:add` 函数地址 patch）、`调 r10` handler、
  unsigned setcc（`置低/置高/置低等/置高等`）、`浮取栈/浮存栈` xmm1、`取64 [r10+disp]`、
  `存指8/16/32/64`、`零扩展字/字节`。label 检测修复（`:` 前有空格不当 label）。
- **⚠️ 自举破坏根因**：case-15 的 `fsz==8 && fty<0 → mov64` **误伤 8 字节数组字段**
  （char buf[8] 的 frow 也是 1），数组字段被当 fnptr 加载值而非地址 → 自举产物全坏。
  修复：fnptr 字段注册时 `st_field_ty(si, fn, -2)` 标记，load 侧只对 `fty==-2` 做 64 位加载。
- **⚠️ 编码坑**：python `\x` 转义 + `.encode()` 双重编码（mojibake）会污染 qcc 源。
  改中文 ASM 文本必须用 python 十六进制验证字节。
回归：fp4/fp5a~g + nest_a~g + bigstruct1~3 + p2test + test_fnptr 19/19 H1==H2 +
4/4 等价。自举收敛 **BC5D7F1D**（gcc→91257161→BC5D7F1D 一步漂移）。

## 2026-08-03 自举记录（六）：bigsz 拷贝 + sret 返回路径补文本
>8B 结构体按值传参（bigsz 拷贝）和 >8B 结构体返回（sret）两条路径存在裸发射：
- **bigsz 拷贝循环**（case-8）：`mov rax,[r10+k*8]` 无文本；`lea rax,[rsp]` 无文本。
  补 `取64 r0, [r10+off]`（asm_zh `取64` 新增 [r10+disp] 分支）和 `取址 r0, [rsp+0]`
  （lea 改 disp8 形式 48 8D 44 24 00 与 asm_zh `取址` 一致）。
- **sret 返回拷贝**（case-6）：`mov [rcx+off],rax/eax/ax/al` 与 `movzx eax,word/byte[rbp+..]`
  全裸发射。asm_zh 新增 `存指8/16/32/64`（[r1+disp8] 目标 store）+ `零扩展字`/`零扩展字节`
  （[rbp+disp32] movzx），qcc 补文本。
- **⚠️ 转义坑**：C 字符串 `\x` 转义是贪婪的——`"\xe6\x8c\x8764"` 中 `\x8764` 超范围。
  python 写字节（单反斜杠）与写 C 字面（双反斜杠）结果不同；修 qcc_x86.c 时把 `\x` 转义
  改回中文字符直写。改中文 ASM 文本后必须用 python 十六进制验证两文件字节一致
  （read_file 显示中文不可靠，曾把 `取址` 显示成 `取栈址`）。
回归：bigstruct/bigstruct2/bigstruct3（12/16/13/14 字节 struct 传参与返回）H1==H2 全等 +
nest_a~g + p2test 全过 + 4/4 等价 + 自举收敛。新不动点 **643B2257**。

## 2026-08-03 自举记录（五）：struct 数组实参退化指针
nest_g/nest_e 失败：`fill(struct Item *it)` 传 `items`（struct 数组）时，case-8 调用
实参的 bigsz 判断只查 `st_idx>=0 && sz>8`，**没查 `arr_sz`** → struct 数组被当 >8B
大结构体按值拷贝到栈、传副本指针 → callee 写的是死副本，main 原数组不变（读回失败）。
修复：bigsz 条件加 `var_arrsz(an) == 0`（与 var_small_struct 的 arr_sz 防护同类）。
新不动点 **A122CAD6**（gcc→C7F8499F→A122CAD6）。（bigsz 拷贝循环裸发射已由记录六修复）

## 2026-08-03 自举记录（四）：嵌套成员链（数组根）修复
p2test（struct 数组读写测试）直接编译崩溃（0xC0000005，rip=0/rbp=0）。
根因一：**mem_addr 不处理 case-14（数组访问）根节点** → `items[i].p.x`、
`rects[i].tl.x` 这类"数组下标 + 嵌套成员（≥2 层字段）"链在 case 10（赋值）和
case 15（读取）两条路径都静默失败 → 赋值 store 整段丢弃 + 表达式栈泄漏
（每迭代 -8B）→ 尾 `加栈` 失衡 → rbp/返回地址被垃圾覆盖 → 崩溃。
修复：mem_addr 新增 `if (nt[n] == 14)` 分支——`var_stidx` 取数组元素 struct 类型，
`cg_no_deref=1; cg(n)` 得 `&arr[i]`，`*fsz_out=stypes[si].sz`。须在 mem_addr 前
加 `static void cg(int n);` 前置声明（cg 定义在 2956 行，晚于 mem_addr）。
根因二：**case 10 的 `arr[i].field = expr` 无条件 4 字节 `存32rax`** → char 字段
（`items[i].tag`）写成 4 字节污染后续字段。修复：按 `st_field_size` 分 1/4/8
字节（`存字节rax bl` / `存32rax` / `存64 [r0], r3`）。
回归：nest_a/b/c/d + p2test（exit=669=Σ 正确）全过；三段等价 4/4；自举收敛
新固定点 **B2FFD9E7**（gcc→BFB9539A→B2FFD9E7 两步，符合"一步漂移"规律）。

## 2026-08-03 自举恢复记录（三）arrayadd
数组索引缩放 imul（`乘 r11, r9` 4 处）和 64 位元素存储（`存64 [r0], r3` 4 处 +
asm_zh `存64` 扩展）补文本。新不动点 D1B41CDB。

## 2026-08-03 自举恢复记录（二）
大程序（qcc_x86.c 自身，487KB / 6197 labels / 530 字符串）三段等价也打通了，
`scripts\_verify_equiv.ps1` 现含 qcc_self 测试（4/4 通过）。新增根因：
1. **asm_zh labels[4096] → 16384**：大程序 6197 个 label，溢出静默丢弃 2586 条跳转
   （H2 少 14KB）。同时 patches[8192]→16384，表满改显式报错（fail loud）。
2. **数据池计入 pass1**：asm_zh pass1（emit_data=0）不 append 数据池 → code_size 少
   一池 → real_base 比 qcc data_rva_base 少一页（IAT 位移错 0x2000）。改两遍都 append。
3. **pscale 文本/字节不一致**：`乘 r0, r2` 文本 vs 字节是 r10（REX.B=1）→ 文本改 `乘 r0, r10`。
4. **asm_zh 补 `异或` handler**（XOR=0x31，此前静默跳过）。

## 2026-08-03 自举恢复记录（一）
截断事故后恢复源码曾无法自举（种子 token 溢出 70000 → 新源码 131072）。
两个根因修复后自举收敛（新固定点 492BFDD8）：
1. **str_offs/dbl_offs 重置循环 512→1024**：字符串 ID ≥512 在 pass2 保留 pass1 过期偏移，
   池子缺 321 字节 → PE .text/.data 出现 0x1000 空洞（加载器 ERROR_BAD_EXE_FORMAT 193）
   → 自编译产物全部静默损坏。顺带修复 write_pe 空洞（text_size 对齐到 data_rva_base）。
2. **NULL 未定义**：自宿主词法器无预处理器，`outf = NULL` 编译为 stale-register store，
   main 的初始化全错（outf 写成 src 字符串地址、输入文件处理错乱）。
   解析器新增 NULL → 常量 0。
3. qcc_rt.c 补 strcat（-S 模式拼文件名需要，自宿主无 libc）。

## 已知限制
- **自宿主 -S 模式已打通**（2026-08-03 记录八）：asm_emit 固定参数 + fprintf。
  自宿主 -S 输出到 stdout，需 `cmd /c "qcc -S file.c > file.asm"` 重定向
  （PowerShell `>` 是 UTF-16 会乱码）。gcc 版引擎写文件，_verify_equiv.ps1 不受影响。
- 日常重建、编译用户程序、--test 均不依赖 -S。

## 种子丢失时的重建（才需要 gcc）
```powershell
gcc -O2 -Wall -Werror -Wno-misleading-indentation qcc_x86.c -o stage1.exe
stage1.exe qcc_x86.c -o new.exe     # 一步漂移（gcc 版与自宿主版布局差一次）
new.exe qcc_x86.c -o v2.exe         # == 固定点（收敛），此后不再需要 gcc
```

## 历史固定点（参考）
- s6diag14 = 6F9993F3（旧英语链，62MB 栈，无法编译更大源码）
- jy_compiler = 7A2032C7（旧甲言链，TS=65536）
- float-only = 421F5AD2（中间态）
- A3456B9E = 前截断不动点（种子二进制保留 qcc_A3456B9E_seed.exe，其快照源码已被新源码取代）
- 492BFDD8 = 2026-08-03 恢复（一）不动点
- 9289B88C = 大程序三段等价不动点（label 扩容/数据池/异或/pscale 文本）
- D1B41CDB = arrayadd 不动点
- B2FFD9E7 = 嵌套成员链数组根修复不动点
- A122CAD6 = struct 数组实参退化指针修复不动点
- 643B2257 = bigsz 拷贝/sret 返回补文本不动点
- BC5D7F1D = fnptr 字段全场景不动点
- **当前 = FCAD3D57**（自宿主 -S 模式打通 + 词法器 \0 转义 + 段名 split-write）
- AC606F5A = route_learn 全链打通不动点（记录九初版，含 [SZM] 调试打印）
- 4DEFCB43 = route_learn 全链打通不动点（记录九终版，移除调试打印）

## 回滚快照清单（全部在 seed/ 目录）
| 文件 | 对应状态 | 用途 |
|:--|:--|:--|
| `qcc_x86.c` | 当前源码 | 与线上 srclib/qcc_x86.c 一致 |
| `qcc_rt.c` | 当前运行时 | 与线上 srclib/qcc_rt.c 一致 |
| `asm_zh.c` | 当前汇编器源码 | 与线上 srclib/asm_zh.c 一致 |
| `qcc_FCAD3D57_seed.exe` | 当前编译器 | 日常自举种子 |
| `qcc_BC5D7F1D_seed.exe` | fnptr 字段修复不动点 | 回滚 |
| `qcc_643B2257_seed.exe` | bigsz/sret 修复不动点 | 回滚 |
| `qcc_A122CAD6_seed.exe` | 实参退化修复不动点 | 回滚 |
| `qcc_B2FFD9E7_seed.exe` | 嵌套成员修复不动点 | 回滚 |
| `qcc_D1B41CDB_seed.exe` | arrayadd 不动点 | 回滚 |
| `qcc_9289B88C_seed.exe` | 大程序等价不动点 | 回滚 |
| `qcc_492BFDD8_seed.exe` | 恢复（一）不动点 | 回滚 |
| `qcc_A3456B9E_seed.exe` | 前截断不动点 | 回滚二进制（快照源码已更新，无法自重建） |
| `qcc_A421F5AD2_float.exe` | float-only 编译器 | 回滚 float 态二进制 |
| `_BOOTSTRAP.md` | 本说明 | — |

## 回滚路径
- **当前 FCAD3D57** → 直接替换回 seed/qcc_x86.c + qcc_rt.c + asm_zh.c
- **float-only 421F5AD2** → 二进制: seed/qcc_A421F5AD2_float.exe（C 源码见工作区 _qcc_float_only_backup.c）
- **原始基线 6F9993F3/7A2032C7** → 工作区 _qcc_r16_backup.c（双备份）+ srclib_jiayan/qcc_pre_float.jy
