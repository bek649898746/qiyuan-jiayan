# tests/kernel/archive — 旧版本内核测试归档

> 2026-08-09 归档（台账 #9）。当前开发版本见 `../kernel_v41.c`（及更新的版本）。

## 归档内容
- `kernel_v5.c` ~ `kernel_v40.c` — 内核版本迭代历史（每版自包含裸机测试）
- `kernel_kbd*.c` — 键盘驱动早期迭代
- `kernel.c` / `k0.c` — 早期内核骨架实验
- `kernel_else.c` / `kernel_arr.c` / `kernel_full.c` / `kernel_noglob.c` — 单特性实验
- `_bug2.c` / `_bug3.c` — 早期 bug 复现

## 用法
```powershell
# 构建归档中的某版内核（串口输出测试）:
# 把 KERNEL 指到 archive/ 下的文件名即可（脚本参数化）
wsl bash scripts/_qemu_kernel.sh KERNEL=kernel_v20
```

## 保留在上级目录的
- `kernel_v41.c` — 当前最新版本
- `kernel_pic.c` / `kernel_serial.c` / `kernel_font.c` — 组件测试
- `kernel_*.h` — 头文件
- `boot.S` / `kernel.S` — 启动与汇编支持
- `_test_*.c` — 特性探针
- `expected/` — 期望输出
