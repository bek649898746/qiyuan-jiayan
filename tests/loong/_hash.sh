#!/usr/bin/env bash
# 计算期望输出哈希（与 run_loongarch.sh 相同的方式：命令替换去尾换行）
cd "$(dirname "$0")"
for src in test_loong.c qi_deep.c; do
    bin="${src%.c}.out"
    loongarch64-linux-gnu-gcc -static -O2 "$src" -o "$bin" || exit 1
    out=$(qemu-loongarch64 "$bin")
    h=$(printf '%s' "$out" | sha256sum | awk '{print $1}')
    echo "$src -> $h"
done
