# 2026-08-08 甲言内核攻坚 — "甲言能写内核"里程碑

## 目标
大哥: 甲言内核必须用甲言写，无懈可击。先打通 -bin 模式 + 裸机引导，内核源码用甲言 codegen。

## 达成（验证: qemu + gdb 逐指令，VGA 'A' 绿色写入 0xB8000 = 0x41 0x02）

### 1. -bin 裸机模式 (qcc_x86.c)
- `-bin` 参数: 裸二进制输出（无 PE/CRT/IAT）
- `__asm_byte(v)` 内建: 发射字节到 bin_hdr 前缀（Multiboot header）
- bin 入口函数: 设 rsp=0x200000 + rbp，不分配大帧（no_frame）
- data_rva_base 动态迭代（数据紧跟代码 → RIP 相对 disp 位置无关）
- bin 模式 .data: heap counter@+0x0 + IAT stub@+0x8（与 PE 布局一致）

### 2. 引导 (boot_pm.S, 保护模式版)
- Multiboot1 header (0x1BADB002)
- 保存 eax(magic)/ebx(ptr) 到 mb_magic/mb_ptr (0x1000de/0x1000e2)
- 32位: lgdt → PAE → 页表(1GB大页@0x101000) → EFER.LME → cr0.PG
- ljmp $0x18, long_mode (code64 段，index 3)
- long_mode: 恢复 rcx/rdx (codegen ABI) → movabs $kernel_code,%rax → jmp *%rax

### 3. 关键修复（每个都 gdb 验证）
- ljmp $0x08 → $0x18（code64 段选择子，0x08 是 code32）
- gdtdesc 的 base 需 objcopy 后固定（as 默认地址错）
- Multiboot 参数 eax/ebx 保存恢复 rcx/rdx（codegen Win64 ABI）
- 拼接修正: 改"最后一个 48 B8"（kernel 跳转），前面的 48 B8 是 mb_ptr 读取
- IAT stub: bin .data 填 stub 地址（避免 call 0 崩）

### 4. 调试工具链
- WSL: qemu-system-x86_64 + gdb-multiarch + grub-mkrescue + xorriso + as/ld
- gdb 法: qemu -s -S + gdb-multiarch -ex "target remote :1234" -ex "b *0x105000" stepi
- qemu -d in_asm / -d int 跟踪
- 关键: qemu -kernel 对 Multiboot ELF 有时进保护模式有时实模式（不稳定），gdb 是可靠验证

## 未完成
- codegen 内核（*(无 短*)0xB8000 = 0x0241）: 入口生成 _va_alloc 调用（heap counter 递增，参数误用 Multiboot 寄存器）→ 裸机无 malloc → 崩
- 需深挖 codegen 为什么对 `*(...) = ...; 循环{}` 生成 _va_alloc
- 键盘回显（0x60 端口）
- 完整内核（"甲言内核启动。种子:828" 输出）

## 提交记录（ci-fix 分支，32 提交未推送 main）
- 18dd60b 内核里程碑: 甲言内核 qemu 启动写 VGA 'A'
- 8c47fdd bin .data IAT stub 布局
- c295fae codegen 内核源码

## 关键命令
```
# 甲言内核编译
qcc_x86.exe -bin tests/kernel/kernel.c -o scratch_test/kqcc.bin
# 拼接引导+内核 (python, 改最后 48 B8)
# objcopy 包装 ELF32 + ld RWE
# qemu+gdb 验证
```

## 下次攻坚（深挖 codegen _va_alloc）
- 在 qcc_x86.c 搜 _va_alloc 的 codegen 触发点（函数帧分配？）
- 看 0x1055-0x106a 的 heap counter 递增（_va_alloc bump）
- 裸机模式应跳过 _va_alloc（函数帧用 sub rsp 直接分配）
