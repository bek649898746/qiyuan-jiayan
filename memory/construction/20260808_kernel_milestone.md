# 2026-08-08 甲言内核里程碑 — QEMU 裸机首次启动

## ⚠️ 修正 (2026-08-08 09:30, 虾米审计) — 前述"9 Gates完成/5 Bug根治"为虚标

### 审计发现 (裂缝 1-4)
1. **Bug #5 的"根治"是误标**: `*(整*)MMIO` 直接强转读只有 1 字节(真bug), rd32/wr32 是绕行非根治
2. **根因**: `字节`(双字)在 lexer 无映射 → `字节 *p` 声明被整个丢弃, 内核代码在坏地基上
3. **Gate 6-9 虚标**: kernel_v10-v16 大部分是演示/模拟/占位, nvme.c/tensor_pool.c 等模块文件不存在
4. **验证证据不持久化**: QEMU 串口输出从不落盘

### 本轮修复 (commit 1216cfb, 全部验证)
- `字节`→char 映射 (根因) + pesz[] 数组宽度修复 (case-12/10) + 无短* 2字节 (11处分支)
- `__attribute__((...))` 死循环修复 (bin_test.c 挂死 build.ps1)
- boot.S: 16MB MMIO 窗口 (pd[496..503])
- _qemu_nvme.sh: EP 自动提取 + 日志持久化 + expected 比对
- **验证**: v17 探针 A-H 全对 [PASS]; build.ps1 不动点 A9CC10A2 完好 + 167/167 测试全过

### 诚实 Gate 状态 (修正后)
| Gate | 真实等级 | 说明 |
|:--|:--|:--|
| Gate 0-1 | ✅ 实现级 | VGA/串口/PIC (kernel_v5) |
| Gate 2 | 🔵 接口级 | kernel_mem.h 纯宏 |
| Gate 3-4 | 🟡 功能演示 | kernel_agent.h 类型 + v8/v9 调度逻辑 |
| Gate 5 | 🔴 探针级 | PCI枚举/MMIO探针 (v15/v16/v17), **真驱动未写** |
| Gate 6-9 | 🔵 占位/演示 | v11-v14, 模块文件不存在 |

### 已知遗留
- **jy 镜像 (qcc_work.jy) 冻结在 A9CC10A2**: 深度诊断确认自宿主链对**任何镜像源码修改**（连加一个 lexer 字符串 `字节` 都会）产出 8.4MB 破产物 (v1→v2, .text 函数大量丢失)。根因是预先存在的自宿主静态布局/字符串表脆弱性（`str_tbl[1024][2048]` 上限 + 静态槽寻址不一致 + 顶层节点 256 限制），非本轮引入。改镜像本身就会破坏不动点 —— 无法同步。**决策: 镜像冻结, 宿主 (qcc_x86.exe) 承载全部修复**。内核用宿主编译, 不受影响。后续若要解除冻结, 需先修自宿主布局 bug（另立专项, 风险极高）。
- **Gate 5 NVMe 已打通 Identify (commit 8bea5a8)**: PCIe枚举→BAR0→原生32位MMIO→控制器重置→大队列配置(AQA=0xFF003F, 队列@0x7FDD000/0x7FDE000)→启用RDY=1→Identify→**SN=JIAYAN + MN=QEMU NVMe Ctrl 读回 [PASS]**。遗留: NN 字段偏移显示非标值 + 读写命令(0x01/0x02) + Tensor 池 待做。
- **Tensor 持久池 v2 (commit bdb04e9)**: 版本链 + 追加写 (store→v1, append→v2, fetch取最新, fetch_v取指定版本, delete全版本), 纯内存(池@0x2000000 64槽), **QEMU 验证 TENSOR-PASS [PASS]**。NVMe 持久化层待数据面解阻。
- **数据面状态 (commit 8722a74)**: NVM 读写需走 I/O 队列(非管理队列); 完成检测被 GRUB 异步读盘干扰; bin模式零全局铁律。读写回环待干净队列环境(QEMU/GRUB 时序)。

---

## 背景
大哥的"甲言内核合成实现方案 v3.2"设计了完整路线。Gate 0 裸机存活已有基础（kernel.c→VGA手写），Gate 1 需要串口+中断。

## 成果：甲言内核 v5 在 QEMU 裸机成功启动

```
JIAYAN KERNEL v5
PIC OK | COM1 OK | SEED:828
>
```

## 关键突破

### 1. 编译器 __asm 内建 (srclib/qcc_x86.c)
- 新增 `__asm` 内置函数，发射任意字节到代码流
- 解锁 sti (0xFB) / cli (0xFA) / hlt (0xF4) 等特权指令
- 同时注册到 coff_is_builtin 列表

### 2. boot.S Multiboot 修复
- a.out kludge: flags 改为 0x10003（bit16=1 支持 raw binary）
- `ld -m elf_i386 -Ttext 0x100000 --defsym kernel_main=0` 定基址
- `jmp kernel_main` 跳转修补：找 48 BC (movabs rsp) + E9 模式，计算正确 rel32

### 3. 内核架构 (tests/kernel/kernel_v5.c)
- 零全局变量：发现 bin 模式全局变量 RIP-relative 偏移有 bug（写 NULL→Page Fault）
- 解决方案：所有状态放在栈上（`整 pos=0; 整 com1=0x3F8;`），通过 `&` 传地址
- 验证：局部变量、字符串常量（sdat）、函数调用、参数传递均正常

### 4. COM1 串口驱动
- 8250 UART 初始化序列 (IER/DLAB/LCR/FCR/MCR)
- serial_wait 轮询 LSR bit5 (THRE)
- serial_putc: \n 自动转 \r\n

### 5. PIC 8259A 中断框架
- ICW1-4 + OCW1 全屏蔽
- IRQ0-7→0x20-0x27, IRQ8-15→0x28-0x2F
- 轮询模式安全（中断屏蔽），框架已就绪

## 已知遗留

| 问题 | 优先级 | 说明 |
|:--|:--|:--|
| bin 模式全局变量 NULL 寻址 | P1 | RIP-relative 偏移计算bug，bin 模式下 data_base 不正确 |
| IDT 未设 | P1 | 中断分发需要 `lidt` + ISR 桩，当前仅 PIC 框架就绪 |
| 内核无 malloc/堆 | P2 | 方案设计为编译时定死，当前栈+常量够用 |
| `\r\n` 双 CR | P3 | serial_putc 和字符串字面量都带 \r，输出多一个 CR |

## 构建与验证命令

```powershell
# 编译编译器（含 __asm 内建）
gcc -O2 -Wall -Werror srclib/qcc_x86.c -o qcc_x86_new.exe

# 编译内核
.\qcc_x86_new.exe -bin tests/kernel/kernel_v5.c -o scratch_test/kernel_v5.bin

# 拼接 + ISO + QEMU (自动)
python scripts/stitch_kernel.py scratch_test/kernel_v5.bin 0x2EC9

# 或用 shell 脚本
wsl bash scripts/_build_iso.sh
wsl timeout 15 qemu-system-x86_64 -cdrom /tmp/k2.iso -nographic -no-reboot -m 128M
```

## 文件清单

| 文件 | 说明 |
|:--|:--|
| srclib/qcc_x86.c | +__asm 内建 |
| tests/kernel/boot.S | Multiboot a.out kludge 修复 |
| tests/kernel/kernel_v5.c | 全功能内核（串口+PIC+键盘+hlt） |
| tests/kernel/kernel_noglob.c | 零变量验证版 |
| tests/kernel/kernel_serial.c | 串口+VGA+键盘（全局变量版，待修） |
| tests/kernel/kernel_pic.c | PIC框架+键盘（全局变量版，待修） |
| scripts/stitch_kernel.py | 全自动：编译boot+拼接+修补+ISO+QEMU |
| scripts/_build_iso.sh | WSL GRUB ISO 构建 |
| scripts/_find_jmp.py | boot.bin 跳转分析工具 |

## Gate 进度

| Gate | 状态 |
|:--|:--|
| Gate 0: 裸机存活 | ✅ VGA+键盘回显+banner+codegen 突破 |
| Gate 1: 串口+中断 | 🎉 COM1 ✅ / PIC 框架 ✅ / IDT+中断分发 🚧 |
| Gate 2-9 | 待攻 |

## 教训
1. **bin模式全局变量是陷阱**：RIP-relative 偏移在 bin 模式下 data_base 计算有误 → 栈上 context 更优
2. **Multiboot a.out kludge 需要基址修正**：`ld -Ttext` 定址比 `as --32` + `objcopy` 更可靠
3. **`ld --defsym kernel_main=0`** 让未定义符号不报错
4. **GRUB serial console** 需要在 grub.cfg 配置 `terminal_output serial`
5. **_BOOT_CONTEXT.md 要及时更新**：主战场已从 COFF jy 转向内核开发
