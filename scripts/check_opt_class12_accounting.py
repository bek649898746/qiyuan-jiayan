# -*- coding: utf-8 -*-
"""check_opt_class12_accounting.py —— ORD-156 判据（记账守恒 + 两个 pass 的实测）.

四条：
  A. **类12 拆分守恒**：新 `算术立即融合` + `算术立即寄存器形态` == 旧 `算术立即融合`（旧源码取 `git show <rev>:srclib/opt_zh.jy`）；
  B. 拆分**不改行为**：新旧优化器对同一 `.asm` 的产物逐字节相同；
  C. **④ 不可达**：报出命中数（跳转/返回），`--no-4` 与全开 的产物字节/耗时对照（用于判定是否零收益）；
  D. **⑰ 立即直存**：报出命中数，`--no-17` 与全开 的产物字节差（用于判定是否零字节收益）。

用法: python scripts/check_opt_class12_accounting.py [--cc qcc_zh_v1.exe] [--old-rev HEAD~1]
退出码: 0 = PASS / 1 = RED
"""
import argparse
import hashlib
import io
import os
import re
import shutil
import struct
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qcc_tmp import tmp_dir  # noqa: E402

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ASM_TOOL = os.path.join(REPO, "asm_zh.exe")   # 出厂中文汇编器：.asm → 产物（.text 口径的唯一可信尺子）


def sh(cmd, cwd=REPO, timeout=900):
    return subprocess.run(cmd, cwd=cwd, capture_output=True, text=True,
                          encoding="utf-8", errors="replace", timeout=timeout)


def build_opt(cc, src, out):
    r = sh([cc, src, "-o", out])
    if r.returncode != 0 or not os.path.exists(out):
        print("[ERR] opt_zh 编译失败 rc=%d\n%s" % (r.returncode, (r.stdout + r.stderr)[-300:]))
        return False
    return True


def run_opt(exe, asm, out, extra=()):
    t0 = time.perf_counter()
    r = sh([exe, asm, "-o", out, *extra])
    el = time.perf_counter() - t0
    return r, el


def num(text, pat, default=None, g=1):
    m = re.search(pat, text)
    return int(m.group(g)) if m else default


def text_size(pe_path):
    """取 PE 的 `.text` 段大小（vsize, rawsize）。**产物字节**与 `.asm` 文本字节是两把尺子。"""
    b = io.open(pe_path, "rb").read()
    pe = struct.unpack_from("<I", b, 0x3C)[0]
    nsec = struct.unpack_from("<H", b, pe + 6)[0]
    optsz = struct.unpack_from("<H", b, pe + 20)[0]
    sec = pe + 24 + optsz
    for i in range(nsec):
        off = sec + i * 40
        name = b[off:off + 8].rstrip(b"\x00")
        if name == b".text":
            vsize, _vaddr, rsize = struct.unpack_from("<III", b, off + 8)
            return vsize, rsize
    return None


def assemble(asm_path, exe_path):
    """用出厂 asm_zh 把 .asm 真汇编成 PE，返回 (rc, 输出)。"""
    r = sh([ASM_TOOL, asm_path, "-o", exe_path])
    return r.returncode, (r.stdout + r.stderr)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--cc", default="qcc_zh_v1.exe")
    ap.add_argument("--old-rev", default="72e5284e",
                    help="拆分前的 opt_zh 提交（钉死，别用 HEAD~1 —— 新提交会让它漂到拆分后）")
    a = ap.parse_args()
    cc = a.cc if os.path.isabs(a.cc) else os.path.join(REPO, a.cc)
    if not os.path.exists(cc):
        print("[ERR] 缺件: %s" % cc)
        return 2
    work = tmp_dir("opt_c12")
    old_src = os.path.join(work, "opt_old.jy")
    new_src = os.path.join(REPO, "srclib", "opt_zh.jy")
    r = subprocess.run(["git", "show", "%s:srclib/opt_zh.jy" % a.old_rev], cwd=REPO, capture_output=True)
    if r.returncode != 0:
        print("[ERR] 取旧 opt_zh 失败: %s" % a.old_rev)
        return 2
    io.open(old_src, "wb").write(r.stdout)
    old_exe = os.path.join(work, "opt_old.exe")
    new_exe = os.path.join(work, "opt_new.exe")
    if not (build_opt(cc, old_src, old_exe) and build_opt(cc, new_src, new_exe)):
        return 1

    # ---- 样本 .asm：优先复用 asm_e2e 的 t_prt；否则用**同一份 t_prt 源码**现场生成 ----
    # 说明：`-S` 产物 = 用户代码 + 内联运行库（6.5 MB 量级，⑥⑰ 的命中主要来自运行库），
    #       所以 fallback 必须与 check_asm_e2e 的 t_prt 用例同源，不能换成「小样本」糊过去。
    asm = os.path.join(REPO, "build", "asm_e2e", "t_prt.exe.asm")
    if not os.path.exists(asm):
        src = os.path.join(work, "t_prt.c")
        io.open(src, "w", encoding="utf-8").write(
            '#include <stdio.h>\nint main(void){ printf("CNE2E 42\\n"); return 0; }\n')
        base = os.path.join(work, "t_prt.exe")
        rr = sh([cc, "-S", src, "-o", base])
        asm = base + ".asm"
        if rr.returncode != 0 or not os.path.exists(asm):
            print("[ERR] 样本 .asm 生成失败")
            return 2
        print("[提示] build/asm_e2e 缺样本，已按 check_asm_e2e 的 t_prt 用例现场生成")
    try:
        asm_show = os.path.relpath(asm, REPO)
    except ValueError:            # 样本可能在 D: 临时盘（跨盘无法 relpath）
        asm_show = asm
    print("样本 .asm = %s（%d 字节）" % (asm_show, os.path.getsize(asm)))

    o_old = os.path.join(work, "old.asm")
    o_new = os.path.join(work, "new.asm")
    r_old, t_old = run_opt(old_exe, asm, o_old)
    r_new, t_new = run_opt(new_exe, asm, o_new)
    for tag, r in (("old", r_old), ("new", r_new)):
        if r.returncode != 0:
            print("[ERR] %s 优化器 失败 rc=%d\n%s" % (tag, r.returncode, (r.stdout + r.stderr)[-300:]))
            return 1
    txt_old = (r_old.stdout or "") + (r_old.stderr or "")
    txt_new = (r_new.stdout or "") + (r_new.stderr or "")

    old_c12 = num(txt_old, r"算术立即融合 (\d+)")
    new_c12 = num(txt_new, r"算术立即融合 (\d+)")
    new_c12b = num(txt_new, r"算术立即寄存器形态 (\d+)")
    print("A 记账: 旧 算术立即融合=%s · 新 算术立即融合=%s + 寄存器形态=%s" % (old_c12, new_c12, new_c12b))

    same = (hashlib.sha256(io.open(o_old, "rb").read()).hexdigest()
            == hashlib.sha256(io.open(o_new, "rb").read()).hexdigest())
    print("B 产物: 新旧逐字节%s" % ("相同 ✓" if same else "**不同 ✗**"))

    # ---- C: ④ 不可达 ----
    o_no4 = os.path.join(work, "no4.asm")
    r_no4, t_no4 = run_opt(new_exe, asm, o_no4, ("--no-4",))
    txt_no4 = (r_no4.stdout or "") + (r_no4.stderr or "")
    hit_j = num(txt_no4, r"不可达 跳转 (\d+) 返回 (\d+)", g=1)
    hit_r = num(txt_no4, r"不可达 跳转 (\d+) 返回 (\d+)", g=2)
    sz_full, sz_no4 = os.path.getsize(o_new), os.path.getsize(o_no4)

    # ---- D: ⑰ 立即直存 ----
    o_no17 = os.path.join(work, "no17.asm")
    r_no17, t_no17 = run_opt(new_exe, asm, o_no17, ("--no-17",))
    hit_17 = num(txt_new, r"立即直存 (\d+)")   # 命中数取**全开**那一跑（--no-17 关掉后自然是 0）
    sz_no17 = os.path.getsize(o_no17)

    # ---- 两把尺子：`.asm` 文本字节 vs **真汇编后的 `.text` 字节** ----
    # 审计实测（两真实样本）：④/⑰ 的 `.text` 都是 0 —— 只缩 `.asm` 文本不算产物收益。
    if not os.path.exists(ASM_TOOL):
        print("[ERR] 缺 asm_zh.exe：.text 口径不可测（不许拿 .asm 文本冒称产物收益）")
        return 2
    prod = {}
    for tag, src_asm in (("full", o_new), ("no4", o_no4), ("no17", o_no17)):
        exe = os.path.join(work, tag + ".exe")
        rc, out = assemble(src_asm, exe)
        if rc != 0 or not os.path.exists(exe):
            print("[ERR] 汇编失败 tag=%s rc=%d\n%s" % (tag, rc, out[-300:]))
            return 1
        prod[tag] = text_size(exe)
        if prod[tag] is None:
            print("[ERR] 产物无 .text 段 tag=%s" % tag)
            return 1

    d_asm_no4 = sz_full - sz_no4
    d_txt_no4 = prod["full"][1] - prod["no4"][1]
    print("C ④ 不可达: 命中 跳转=%s 返回=%s · .asm 差 %d B · **.text 差 %d B** · 耗时 %.2fs vs %.2fs"
          % (hit_j, hit_r, d_asm_no4, d_txt_no4, t_new, t_no4))

    d_asm_17 = sz_full - sz_no17
    d_txt_17 = prod["full"][1] - prod["no17"][1]
    print("D ⑰ 立即直存: 命中=%s · .asm 差 %d B · **.text 差 %d B**（产物口径）"
          % (hit_17, d_asm_17, d_txt_17))

    bad = []
    if old_c12 is None or new_c12 is None or new_c12b is None:
        bad.append("OK 行里缺 算术立即融合/算术立即寄存器形态 计数")
    elif new_c12 + new_c12b != old_c12:
        bad.append("A 记账不守恒: 新 %d + %d != 旧 %d" % (new_c12, new_c12b, old_c12))
    if not same:
        bad.append("B 拆分改变了产物字节")
    if hit_j is None or hit_r is None:
        bad.append("C 缺 不可达 计数行")
    if hit_17 is None:
        bad.append("D 缺 立即直存 计数行")
    # 产物口径 fail-closed：这两个 pass 若哪天真的改了 .text，必须回来重新处置（删/降级/改判据），
    # 不许用「.asm 文本变小」继续冒称收益。
    if d_txt_no4 != 0:
        bad.append("C ④ 的 .text 差 != 0（%d B）：请重新处置该 pass，别拿 .asm 文本当收益" % d_txt_no4)
    if d_txt_17 != 0:
        bad.append("D ⑰ 的 .text 差 != 0（%d B）：请重新处置该 pass，别拿 .asm 文本当收益" % d_txt_17)

    print()
    if bad:
        print("[FAIL] check_opt_class12_accounting：")
        for b in bad:
            print("  - " + b)
        return 1
    print("[通过 PASS] check_opt_class12_accounting：类12 拆分守恒（%d+%d==%d）· 产物逐字节同 · ④/⑰ 已实测"
          % (new_c12, new_c12b, old_c12))
    return 0


if __name__ == "__main__":
    sys.exit(main())
