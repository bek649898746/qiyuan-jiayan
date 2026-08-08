#!/bin/bash
# QEMU NVMe 内核验证一键链 (无懈可击版 2026-08-08)
# 用法: wsl bash scripts/_qemu_nvme.sh [kernel_name]
# 功能: ① 用 qcc_x86.exe 编内核并自动提取 EP (不手抄)
#       ② boot.S 汇编 + stitch + jmp 修补 + GRUB ISO + -device nvme
#       ③ QEMU 串口输出持久化到 scratch_test/_qemu_verify_<kernel>.txt
#       ④ 与 tests/kernel/expected/<kernel>.txt 比对 → PASS/FAIL 落盘
set -e
K=/mnt/c/Users/Administrator/Desktop/qiyuan-jiayan
KERNEL=${1:-kernel_v17}
TAG=$KERNEL
cd $K

# --- ① 编译内核 (Windows exe via WSL interop) + 自动提取 EP ---
WINQCC=$K/qcc_x86.exe
WINSRC=$(wslpath -w $K/tests/kernel/$KERNEL.c)
WINOUT=$(wslpath -w $K/scratch_test/$KERNEL.bin)
EP_RAW=$("$WINQCC" -bin "$WINSRC" -o "$WINOUT" 2>&1 || true)
EP=$(echo "$EP_RAW" | grep -oE 'entry offset: 0x[0-9A-Fa-f]+' | tail -1 | awk '{print $NF}')
if [ -z "$EP" ]; then echo "FAIL: cannot extract EP"; echo "$EP_RAW"; exit 1; fi
echo "[OK] $KERNEL.bin compiled, EP=$EP"

# --- ② boot + stitch ---
as --32 $K/tests/kernel/boot.S -o /tmp/boot_k.o
ld -m elf_i386 -Ttext 0x100000 --defsym kernel_main=0 --oformat binary /tmp/boot_k.o -o /tmp/boot_k.bin
python3 -c "
import struct,sys
boot=open('/tmp/boot_k.bin','rb').read()
k=open('$K/scratch_test/$KERNEL.bin','rb').read()
c=bytearray(boot+k)
for i in range(len(c)-14):
    if c[i]==0x48 and c[i+1]==0xBC:
        if struct.unpack_from('<Q',c,i+2)[0]==0x200000:
            jmp_off=i+10
            if c[jmp_off]==0xE9:
                entry=0x100000+len(boot)+$EP
                rel=entry-(0x100000+jmp_off+5)
                struct.pack_into('<i',c,jmp_off+1,rel)
                open('$K/scratch_test/_${KERNEL}_bootable.bin','wb').write(c)
                print(f'OK:{len(c)}B entry=0x{entry:X}')
                sys.exit(0)
print('FAIL')
" > /tmp/stitch_out.txt
cat /tmp/stitch_out.txt

# --- ③ ISO + NVMe 盘 ---
rm -rf /tmp/isonv; mkdir -p /tmp/isonv/boot/grub
cp $K/scratch_test/_${KERNEL}_bootable.bin /tmp/isonv/boot/kernel.bin
cat > /tmp/isonv/boot/grub/grub.cfg << 'EOF'
serial --speed=115200 --unit=0
terminal_output serial
set timeout=1
menuentry "JY" { multiboot /boot/kernel.bin; boot; }
EOF
grub-mkrescue -o /tmp/knv.iso /tmp/isonv 2>/dev/null
dd if=/dev/zero of=/tmp/nvme_disk.img bs=1M count=16 2>/dev/null

# --- ④ QEMU + 持久化日志 ---
echo "=== QEMU $TAG ==="
timeout 15 qemu-system-x86_64 -M q35 -cdrom /tmp/knv.iso \
  -device nvme,drive=nvme0,serial=JIAYAN \
  -drive file=/tmp/nvme_disk.img,format=raw,if=none,id=nvme0 \
  -nographic -no-reboot -m 128M 2>&1 | tee $K/scratch_test/_qemu_verify_$TAG.txt

# --- ⑤ 期望比对 ---
EXPECTED=$K/tests/kernel/expected/$KERNEL.txt
if [ -f "$EXPECTED" ]; then
  MISS=""
  while IFS= read -r pat; do
    [ -z "$pat" ] && continue
    if ! grep -qF -- "$pat" $K/scratch_test/_qemu_verify_$TAG.txt; then
      MISS="$MISS [$pat]"
    fi
  done < "$EXPECTED"
  if [ -z "$MISS" ]; then echo "[PASS] all expected strings found"; else echo "[FAIL] missing:$MISS"; fi
else
  echo "[INFO] no expected file: $EXPECTED"
fi
