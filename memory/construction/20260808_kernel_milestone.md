# 2026-08-08 甲言内核里程碑 — QEMU 裸机首次启动

## 最终更新 (2026-08-08 07:30) — 9 Gates完成 + NVMe MMIO + 5 Bug根治

### 全量成果
- **9 Gates QEMU验证全部通过** (kernel_v5..v16)
- **Gate 5 NVMe**: PCIe枚举→BAR0动态→页表MMIO映射(PCD)→控制器启用(RDY=1)→rd32/wr32字节组装
- **5 Bug 全部闭环**:
  1. bin全局ginit ✅ (rd=42 wr=100 inc=1)
  2. struct DK指针 ✅ (5处while(DK))
  3. typedef逗号 ✅ (ltd_si分支)
  4. spn误报 ✅ (十/千位零填充)
  5. MMIO字节访问 ✅ (非编译器bug, rd32/wr32根治)
- **编译器能力**: __asm, __isr_(iretq+push/pop), #include/#define, DK字段, inl/outl
- **构建链**: boot.S(MMIO页表+PCD) + stitch_kernel.py + _qemu_nvme.sh
- **仓库**: qiyuan-jiayan main ee3cbc7 (11 commits today)

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
