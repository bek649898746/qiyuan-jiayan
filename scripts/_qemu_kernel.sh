#!/bin/bash
# QEMU -kernel 直载 (跳过 GRUB): 消除 GRUB 对 NVMe 的干扰
# 参数化: KERNEL=$1 (必填), 项目根 = $QIYUAN_ROOT 环境变量或 $2, 默认 WSL 挂载路径
set -e
K=${QIYUAN_ROOT:-${2:-/mnt/c/Users/Administrator/Desktop/qiyuan-jiayan}}
KERNEL=$1
if [ -z "$KERNEL" ]; then echo "用法: $0 <KERNEL名> [项目根] 或 QIYUAN_ROOT=... $0 <KERNEL名>"; exit 1; fi
WINQCC=$K/qcc_x86.exe
WINSRC=$(wslpath -w $K/tests/kernel/$KERNEL.c)
WINOUT=$(wslpath -w $K/scratch_test/$KERNEL.bin)
EP_RAW=$("$WINQCC" -bin "$WINSRC" -o "$WINOUT" 2>&1 || true)
EP=$(echo "$EP_RAW" | grep -oE 'entry offset: 0x[0-9A-Fa-f]+' | tail -1 | awk '{print $NF}')
if [ -z "$EP" ]; then echo "EP extract FAIL"; echo "$EP_RAW"; exit 1; fi
echo "[OK] $KERNEL.bin EP=$EP"

as --32 $K/tests/kernel/boot.S -o /tmp/boot_k.o
ld -m elf_i386 -Ttext 0x100000 --defsym kernel_main=0 --oformat binary /tmp/boot_k.o -o /tmp/boot_k.bin
python3 -c "
import struct, sys
boot = open('/tmp/boot_k.bin','rb').read()
k = open('$K/scratch_test/$KERNEL.bin','rb').read()
c = bytearray(boot + k)
for i in range(len(c)-14):
    if c[i]==0x48 and c[i+1]==0xBC:
        if struct.unpack_from('<Q', c, i+2)[0] == 0x200000:
            jmp_off = i+10
            if c[jmp_off]==0xE9:
                entry = 0x100000 + len(boot) + $EP
                rel = entry - (0x100000 + jmp_off + 5)
                struct.pack_into('<i', c, jmp_off+1, rel)
                open('$K/scratch_test/_${KERNEL}_nboot.bin','wb').write(c)
                print(f'OK {len(c)}B entry=0x{entry:X}')
                sys.exit(0)
print('FAIL'); sys.exit(1)
"
dd if=/dev/zero of=/tmp/nvme_disk.img bs=1M count=16 2>/dev/null
echo "=== QEMU -kernel $KERNEL (no GRUB) ==="
timeout 20 qemu-system-x86_64 -M q35 \
  -kernel $K/scratch_test/_${KERNEL}_nboot.bin \
  -device nvme,drive=nvme0,serial=JIAYAN \
  -drive file=/tmp/nvme_disk.img,format=raw,if=none,id=nvme0 \
  -nographic -no-reboot -m 128M 2>&1 | grep -aE "v36|BAR|R0|ZERO|QSET|R1|T0|T2|GO|ID=|ST=|CCQ=|CSQ=|SQd|WR=|RD=|W=|R=|LOOPBACK|NOT" | head -25
