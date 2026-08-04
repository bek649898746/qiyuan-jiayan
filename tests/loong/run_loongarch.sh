#!/usr/bin/env bash
# ============================================================
# 启元 · 龙芯（LoongArch）交叉编译回归测试 — WSL + QEMU 一键复现
#
# 验证层级（严格边界，不可混同概念）：
#   源码级交叉编译验证 —— 指令生成/ABI 对齐/ELF 构建全部由龙芯 GCC
#   完成，甲言仅提供符合自身语法规范的源码，不涉及自研编译后端。
#   不等同于"甲言编译器原生支持龙芯"。
#   当前为 QEMU 用户模式模拟，非龙芯真机。
#
# 用法:
#   在 WSL (Ubuntu 24.04+) 内执行:  bash run_loongarch.sh
#   或 Windows 侧:                 wsl -d Ubuntu -- bash tests/loong/run_loongarch.sh
#
# 工具依赖:
#   qemu-user (qemu-loongarch64)          sudo apt install qemu-user
#   gcc-loongarch64-linux-gnu             sudo apt install gcc-loongarch64-linux-gnu
# ============================================================
set -u
cd "$(dirname "$0")"
echo "==== 启元 · LoongArch 交叉编译回归测试 ===="

# ---- 0. 工具检查/安装提示 ----
if ! command -v loongarch64-linux-gnu-gcc >/dev/null 2>&1; then
    echo "[SKIP] loongarch64-linux-gnu-gcc 未安装。"
    echo "       sudo apt install gcc-loongarch64-linux-gnu qemu-user"
    exit 2
fi
if ! command -v qemu-loongarch64 >/dev/null 2>&1; then
    echo "[SKIP] qemu-loongarch64 未安装。"
    echo "       sudo apt install qemu-user"
    exit 2
fi

PASS=0
FAIL=0

run_case() {
    local src="$1" bin="$2" expected_sha="$3"
    echo ""
    echo "---- 用例: $src ----"
    loongarch64-linux-gnu-gcc -static -O2 "$src" -o "$bin" || { echo "[FAIL] 交叉编译失败: $src"; FAIL=$((FAIL+1)); return; }
    # ELF magic 校验: 0x7F 'E' 'L' 'F'
    local magic
    magic=$(head -c 4 "$bin" | od -An -tx1 | tr -d ' \n')
    if [ "$magic" != "7f454c46" ]; then
        echo "[FAIL] 产物不是 ELF (magic=$magic): $bin"; FAIL=$((FAIL+1)); return
    fi
    # 运行并校验输出哈希
    local out
    out=$(qemu-loongarch64 "$bin")
    local got
    got=$(printf '%s' "$out" | sha256sum | awk '{print $1}')
    echo "--- 输出 ---"
    printf '%s\n' "$out"
    echo "--- SHA256: $got"
    if [ "$got" = "$expected_sha" ]; then
        echo "[PASS] $src"
        PASS=$((PASS+1))
    else
        echo "[FAIL] $src: 哈希不匹配"
        echo "      期望: $expected_sha"
        FAIL=$((FAIL+1))
    fi
}

# ---- 用例 1: 甲言风格基础（种子 828） ----
run_case test_loong.c test_loong.out \
    "ad360f438902b1a372287d860d9a486214f935f8d1fea31ffc05354065e6ab05"

# ---- 用例 2: 深水测试（递归/结构体/数组/指针/哈希） ----
run_case qi_deep.c qi_deep.out \
    "2d752102bcbd610b47a5b768d46a48ac1b18571c8ac4ce3b456901ebd5219d1a"

echo ""
echo "==== 结果: PASS=$PASS FAIL=$FAIL ===="
exit $FAIL
