#!/bin/bash
set -e
K=/mnt/c/Users/Administrator/Desktop/qiyuan-jiayan
rm -rf /tmp/iso_t; mkdir -p /tmp/iso_t/boot/grub
cp $K/scratch_test/_kernel_v36_bootable.bin /tmp/iso_t/boot/kernel.bin
cat > /tmp/iso_t/boot/grub/grub.cfg << 'EOF'
serial --speed=115200 --unit=0
terminal_output serial
set timeout=0
menuentry "JY" { multiboot /boot/kernel.bin; boot; }
EOF
grub-mkrescue -o /tmp/k_t.iso /tmp/iso_t 2>/dev/null
dd if=/dev/zero of=/tmp/nvme_disk.img bs=1M count=16 2>/dev/null
timeout 20 qemu-system-x86_64 -M q35 -cdrom /tmp/k_t.iso \
  -device nvme,drive=nvme0,serial=JIAYAN \
  -drive file=/tmp/nvme_disk.img,format=raw,if=none,id=nvme0 \
  -nographic -no-reboot -m 128M \
  -trace "enable=pci_nvme_ub_db_wr_invalid_sq,enable=pci_nvme_ub_db_wr_invalid_sqtail,enable=pci_nvme_mmio_doorbell_sq,enable=pci_nvme_update_sq_tail,enable=pci_nvme_io_cmd" > /tmp/qemu_io.log 2>&1 || true
echo "=== invalid db / doorbell / io ==="
grep -a -E "pci_nvme_(ub_db|mmio_doorbell|update_sq|io_cmd)" /tmp/qemu_io.log | tail -12
