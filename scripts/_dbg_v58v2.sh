#!/bin/bash
# v58v2: v2(镜像产物) 编译的 v58, EP 固定用宿主的 0x2A5F
set -e
K=/mnt/c/Users/Administrator/Desktop/qiyuan-jiayan
KERNEL=kernel_v58
EP=0x2A5F
as --32 $K/tests/kernel/boot.S -o /tmp/boot_k.o
ld -m elf_i386 -Ttext 0x100000 --defsym kernel_main=0 --oformat binary /tmp/boot_k.o -o /tmp/boot_k.bin
python3 - <<PYEOF
import struct
boot = open('/tmp/boot_k.bin','rb').read()
k = open('$K/scratch_test/_v58v2.bin','rb').read()
c = bytearray(boot + k)
for i in range(len(c)-14):
    if c[i]==0x48 and c[i+1]==0xBC:
        if struct.unpack_from('<Q', c, i+2)[0] == 0x200000:
            jmp_off = i+10
            if c[jmp_off]==0xE9:
                entry = 0x100000 + len(boot) + $EP
                rel = entry - (0x100000 + jmp_off + 5)
                struct.pack_into('<i', c, jmp_off+1, rel)
                open('$K/scratch_test/_v58v2_nboot.bin','wb').write(c)
                print('patched')
                break
PYEOF
dd if=/dev/zero of=/tmp/nvme_disk.img bs=1M count=16 2>/dev/null
timeout 20 qemu-system-x86_64 -M q35 \
  -kernel $K/scratch_test/_v58v2_nboot.bin \
  -device nvme,drive=nvme0,serial=JIAYAN \
  -drive file=/tmp/nvme_disk.img,format=raw,if=none,id=nvme0 \
  -nographic -no-reboot -m 128M 2>&1 | grep -aE "v58|serial|printf|done|42|2a" | head -8
