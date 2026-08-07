#!/bin/bash
set -e
K=/mnt/c/Users/Administrator/Desktop/qiyuan-jiayan

# Build boot
as --32 $K/tests/kernel/boot.S -o /tmp/boot_k.o
ld -m elf_i386 -Ttext 0x100000 --defsym kernel_main=0 --oformat binary /tmp/boot_k.o -o /tmp/boot_k.bin

# Stitch
EP=0x1811
python3 -c "
import struct,sys
boot=open('/tmp/boot_k.bin','rb').read()
k=open('$K/scratch_test/kernel_v15.bin','rb').read()
c=bytearray(boot+k)
for i in range(len(c)-14):
    if c[i]==0x48 and c[i+1]==0xBC:
        if struct.unpack_from('<Q',c,i+2)[0]==0x200000:
            jmp_off=i+10
            if c[jmp_off]==0xE9:
                entry=0x100000+len(boot)+$EP
                rel=entry-(0x100000+jmp_off+5)
                struct.pack_into('<i',c,jmp_off+1,rel)
                open('$K/scratch_test/_nvme_boot.bin','wb').write(c)
                print(f'OK:{len(c)}B entry=0x{entry:X}')
                sys.exit(0)
print('FAIL')
"

rm -rf /tmp/isonv; mkdir -p /tmp/isonv/boot/grub
cp $K/scratch_test/_nvme_boot.bin /tmp/isonv/boot/kernel.bin
cat > /tmp/isonv/boot/grub/grub.cfg << 'EOF'
serial --speed=115200 --unit=0
terminal_output serial
set timeout=1
menuentry "JY" { multiboot /boot/kernel.bin; boot; }
EOF
grub-mkrescue -o /tmp/knv.iso /tmp/isonv 2>/dev/null
dd if=/dev/zero of=/tmp/nvme_disk.img bs=1M count=16 2>/dev/null
echo "=== QEMU + NVMe ==="
timeout 12 qemu-system-x86_64 -M q35 -cdrom /tmp/knv.iso \
  -device nvme,drive=nvme0,serial=JIAYAN \
  -drive file=/tmp/nvme_disk.img,format=raw,if=none,id=nvme0 \
  -nographic -no-reboot -m 128M 2>&1
