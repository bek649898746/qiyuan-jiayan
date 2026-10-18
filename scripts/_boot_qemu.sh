#!/bin/bash
set -e
KERNEL=/mnt/c/Users/Administrator/Desktop/qiyuan-jiayan/scratch_test/kernel_serial_bootable.bin

rm -rf /tmp/iso3
mkdir -p /tmp/iso3/boot/grub
cp $KERNEL /tmp/iso3/boot/kernel.bin

cat > /tmp/iso3/boot/grub/grub.cfg << 'EOF'
serial --speed=115200 --unit=0
terminal_input serial
terminal_output serial
set timeout=1
set default=0
menuentry "JY" {
    multiboot /boot/kernel.bin
    boot
}
EOF

grub-mkrescue -o /tmp/k3.iso /tmp/iso3 2>/dev/null
echo "=== Booting (GDB: -s -S, press Ctrl-A X to exit) ==="
qemu-system-x86_64 -cdrom /tmp/k3.iso -serial stdio -display none -no-reboot -m 128M -d guest_errors,int
