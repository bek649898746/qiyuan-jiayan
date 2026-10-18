#!/bin/bash
set -e
ISO_DIR=/tmp/iso2
KERNEL=/mnt/c/Users/Administrator/Desktop/qiyuan-jiayan/scratch_test/kernel_serial_bootable.bin

rm -rf $ISO_DIR
mkdir -p $ISO_DIR/boot/grub
cp $KERNEL $ISO_DIR/boot/kernel.bin

cat > $ISO_DIR/boot/grub/grub.cfg << 'GRUBEOF'
set timeout=0
set default=0
menuentry "Jiayan Kernel v3" {
    multiboot /boot/kernel.bin
    boot
}
GRUBEOF

echo "=== Building ISO ==="
grub-mkrescue -o /tmp/k2.iso $ISO_DIR
echo "=== ISO size: $(wc -c < /tmp/k2.iso) ==="

echo "=== Booting QEMU ==="
qemu-system-x86_64 -cdrom /tmp/k2.iso -nographic -no-reboot -m 128M
