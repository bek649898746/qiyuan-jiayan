#!/usr/bin/env python3
"""拼接+启动: boot.S(ld定址) + kernel.bin → ISO → QEMU"""
import sys, struct, subprocess, os

kernel_bin = sys.argv[1] if len(sys.argv) > 1 else "scratch_test/kernel_serial.bin"
entry_off = int(sys.argv[2], 0) if len(sys.argv) > 2 else 0
load_base = 0x100000

wsl_base = "/mnt/c/Users/Administrator/Desktop/qiyuan-jiayan"
boot_s = f"{wsl_base}/tests/kernel/boot.S"

# 1. 编译 boot.S → boot.elf → boot.bin (基址 0x100000)
boot_o = "/tmp/boot_k.o"
boot_elf = "/tmp/boot_k.elf"
boot_bin = "/tmp/boot_k.bin"

subprocess.run(["wsl", "bash", "-c",
    f"as --32 {boot_s} -o {boot_o} && "
    f"ld -m elf_i386 -Ttext 0x{load_base:X} --defsym kernel_main=0 --oformat binary {boot_o} -o {boot_bin}"
], check=True)

r = subprocess.run(["wsl", "cat", boot_bin], capture_output=True)
boot_data = r.stdout
print(f"boot.bin: {len(boot_data)} bytes (base=0x{load_base:X})")

with open(kernel_bin, "rb") as f:
    kernel_data = f.read()
print(f"kernel.bin: {len(kernel_data)} bytes")

# 2. 拼接
combined = bytearray(boot_data + kernel_data)
entry_addr = load_base + len(boot_data) + entry_off
print(f"entry_addr: 0x{entry_addr:X} (0x{load_base:X} + 0x{len(boot_data):X} + 0x{entry_off:X})")

# 3. 修 long_mode 跳转 (48 BC + E9)
found = False
for i in range(len(combined) - 14):
    if combined[i]==0x48 and combined[i+1]==0xBC:
        val = struct.unpack_from('<Q', combined, i+2)[0]
        if val == 0x200000:
            jmp_off = i + 10
            if combined[jmp_off] == 0xE9:
                new_rel = entry_addr - (load_base + jmp_off + 5)
                struct.pack_into('<i', combined, jmp_off+1, new_rel)
                print(f"Patched jmp at file off 0x{jmp_off:X} → +0x{new_rel:X} (target 0x{entry_addr:X})")
                found = True
                break

if not found:
    print("ERROR: jump not found")
    sys.exit(1)

# 4. 输出
out_bin = kernel_bin.replace('.bin', '_bootable.bin')
with open(out_bin, "wb") as f:
    f.write(combined)
print(f"\nOutput: {out_bin} ({len(combined)} bytes)")

# 5. 写 GRUB cfg + 构建 ISO (WSL)
subprocess.run(["wsl", "bash", "-c",
    f"mkdir -p /tmp/iso/boot/grub && "
    f"cp {wsl_base}/{out_bin.replace(chr(92),'/')} /tmp/iso/boot/kernel.bin && "
    f"echo 'set timeout=0' > /tmp/iso/boot/grub/grub.cfg && "
    f"echo 'set default=0' >> /tmp/iso/boot/grub/grub.cfg && "
    f"echo \"menuentry 'Jiayan' {{ multiboot /boot/kernel.bin; boot; }}\" >> /tmp/iso/boot/grub/grub.cfg && "
    f"grub-mkrescue -o /tmp/kernel.iso /tmp/iso 2>/dev/null && "
    f"echo 'ISO: /tmp/kernel.iso ('$(wc -c < /tmp/kernel.iso)' bytes)'"
], check=True)

# 6. 启动 QEMU
print("\n=== QEMU (nographic, timeout 15s) ===")
subprocess.run(["wsl", "timeout", "15", "qemu-system-x86_64",
    "-cdrom", "/tmp/kernel.iso", "-nographic", "-no-reboot", "-m", "128M"])
