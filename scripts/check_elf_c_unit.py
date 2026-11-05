#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""check_elf_c_unit —— `-c --target=elf` 的 `.c` 路径判据（ORD-20261104-113）

背景（2026-11-04 审计递单）：`--target=elf` 原先只接管「单文件直接出可执行」，
`-c`（目标文件）那条路产物仍是 COFF 且带 `__imp_*` ⟹ lua/zlib/git 这类
「分编再链」的程序必然卡在链接期，而 hello 级全绿会掩盖这一点。
ELF-E3 已让 `-c --target=elf` 产出 ET_REL；ORD-20261104-124 又让 ELF 目标
不再前插 Windows mini。本判据把这两件事**钉成可复跑的闸**。

三组（全部实测真产物，不硬编码编译器哈希）：

  A. `-c --target=elf lib.c` ⟹ ELF64 **ET_REL**，且二进制里
     `LoadLibraryA` / `GetProcAddress` / `__imp_` / `__qcc_snprintf` **一个都不许有**。
     （glibc 提供 snprintf/vsnprintf；带 mini = 把 Windows 运行时拖进 .o）
  B. 负对照：同源 `-c`（默认 PE）必须**不是** ELF（COFF machine 0x8664），
     证明 A 的检查能区分目标、不是恒绿。
  C. 回归：同源走 PE 全链 `-o exe` 必须 rc=0，且产物运行退出码符合预期。

用法：
  python scripts/check_elf_c_unit.py [--cc qcc_zh_v1.exe]
退出码：0 = 三组全绿；1 = 有红。
"""
import argparse
import os
import struct
import subprocess
import sys
from qcc_tmp import tmp_dir

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

LIB_SRC = """int add(int a, int b) { return a + b; }
int sub(int a, int b) { return a - b; }
"""

MAIN_SRC = """int main(void) { return 42; }
"""

BANNED = (b"LoadLibraryA", b"GetProcAddress", b"__imp_", b"__qcc_snprintf")


def run(cc, args, cwd=ROOT):
    p = subprocess.run([cc] + args, cwd=cwd, capture_output=True, timeout=600)
    return p.returncode, (p.stdout + p.stderr).decode("utf-8", "replace")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--cc", default="qcc_zh_v1.exe")
    a = ap.parse_args()
    cc = a.cc if os.path.isabs(a.cc) else os.path.join(ROOT, a.cc)
    if not os.path.exists(cc):
        print("[FAIL] 找不到编译器: %s" % cc)
        return 1
    print("== ELF `-c` 路径判据（ORD-20261104-113）==")
    print("  编译器: %s" % os.path.basename(cc))

    tmp = tmp_dir("elfc")
    lib = os.path.join(tmp, "lib.c")
    mn = os.path.join(tmp, "main42.c")
    open(lib, "w", encoding="utf-8", newline="\n").write(LIB_SRC)
    open(mn, "w", encoding="utf-8", newline="\n").write(MAIN_SRC)

    # ---- A ----
    elf_o = os.path.join(tmp, "lib_elf.o")
    rc, log = run(cc, ["-c", "--target=elf", lib, "-o", elf_o])
    if rc != 0 or not os.path.exists(elf_o):
        print("[FAIL] A: `-c --target=elf` 编译失败 rc=%d\n%s" % (rc, log[-600:]))
        return 1
    d = open(elf_o, "rb").read()
    if d[:4] != b"\x7fELF":
        print("[FAIL] A: 产物不是 ELF（magic=%s）" % d[:4].hex())
        return 1
    if d[4] != 2:
        print("[FAIL] A: 不是 ELF64（EI_CLASS=%d）" % d[4])
        return 1
    e_type = struct.unpack_from("<H", d, 16)[0]
    if e_type != 1:
        print("[FAIL] A: e_type=%d != ET_REL(1)" % e_type)
        return 1
    hits = [s.decode() for s in BANNED if s in d]
    if hits:
        print("[FAIL] A: ELF .o 里仍有 Windows 运行时符号: %s" % ", ".join(hits))
        return 1
    print("[PASS] A: ELF64 ET_REL (%d B)，零 Windows 符号" % len(d))

    # ---- B 负对照 ----
    pe_o = os.path.join(tmp, "lib_pe.o")
    rc, log = run(cc, ["-c", lib, "-o", pe_o])
    if rc != 0 or not os.path.exists(pe_o):
        print("[FAIL] B: `-c`（默认 PE）编译失败 rc=%d\n%s" % (rc, log[-600:]))
        return 1
    p = open(pe_o, "rb").read()
    if p[:4] == b"\x7fELF":
        print("[FAIL] B: 默认 PE 目标却产出了 ELF ⟹ 目标开关失效")
        return 1
    machine = struct.unpack_from("<H", p, 0)[0]
    if machine != 0x8664:
        print("[FAIL] B: 不是 x86-64 COFF（machine=0x%04X）" % machine)
        return 1
    print("[PASS] B: 默认 PE 目标仍是 x86-64 COFF（负对照成立）")

    # ---- C PE 全链回归 ----
    exe = os.path.join(tmp, "main42.exe")
    rc, log = run(cc, [mn, "-o", exe])
    if rc != 0 or not os.path.exists(exe):
        print("[FAIL] C: PE 全链编译失败 rc=%d\n%s" % (rc, log[-600:]))
        return 1
    try:
        r = subprocess.run([exe], capture_output=True, timeout=60)
        rrc = r.returncode
    except Exception as e:  # noqa: BLE001
        print("[FAIL] C: 产物无法运行: %r" % (e,))
        return 1
    if (rrc & 0xFFFFFFFF) != 42:
        print("[FAIL] C: 运行退出码 %d != 42" % rrc)
        return 1
    print("[PASS] C: PE 全链 rc=0，产物运行退出 42")

    print("[PASS] ELF `-c` 路径判据全绿")
    return 0


if __name__ == "__main__":
    sys.exit(main())
