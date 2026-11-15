# -*- coding: utf-8 -*-
"""check_ir_whole_func.py — AC2：整函数字节由 L1 发出（序言 + 体 + 尾声）。

判据（对每个载体）：
  W1  prologue muted == replay > 0
  W2  epilogue muted == replay > 0
  W3  所有 [IR-STATS] 行 muted == replay
  W4  (总 muted − 尾声) + KNOWN_LEGACY == [FMAP] 函数范围（允许 0..15 对齐）
  W5  IR 产物与 legacy 产物逐字节相同

载体：
  main          int main(void){return 42;}            KNOWN_LEGACY=0
  single_assign int f(void){int x; x=7; return x;}    KNOWN_LEGACY=12（源值 5 + store 7）
  single_declinit int f(void){int x=7; return x;}     KNOWN_LEGACY=12

⚠️ AC2 边界：W4 只覆盖**被接管函数**。`.text` 中的 `__qcc_*` 运行时仍 legacy，
   不在本判据范围（见 `_evidence/20261108_link_dce/round2/README.md` §AC2 边界）。

用法: python scripts/check_ir_whole_func.py --cc qcc_zh_v1.exe
"""
import argparse, hashlib, io, os, re, subprocess, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qcc_tmp import tmp_dir  # noqa: E402

sys.stdout.reconfigure(encoding="utf-8", errors="replace")

CASES = [
    ("main", "int main(void) { return 42; }\n", "main", 0),
    ("single_assign", "int f(void) { int x; x = 7; return x; }\n", "f", 0),
    ("single_declinit", "int f(void) { int x = 7; return x; }\n", "f", 0),
]

RE_PRO = re.compile(r"\[IR-STATS\] prologue muted=(\d+) replay=(\d+)")
RE_EPI = re.compile(r"\[IR-STATS\] epilogue muted=(\d+) replay=(\d+)")
RE_BODY = re.compile(r"\[IR-STATS\] (?!fn )(\w+ )?muted=(\d+) replay=(\d+)")
RE_FN = re.compile(r"\[IR-STATS\] fn muted=(\d+) replay=(\d+)")


def coff_text(path):
    b = io.open(path, "rb").read()
    nsec = int.from_bytes(b[2:4], "little")
    for i in range(nsec):
        off = 20 + i * 40
        name = b[off:off + 8].rstrip(b"\x00")
        if name in (b".text", b".text$mn"):
            size = int.from_bytes(b[off + 16:off + 20], "little")
            ptr = int.from_bytes(b[off + 20:off + 24], "little")
            return b[ptr:ptr + size]
    return b""


def sha(b):
    return hashlib.sha256(b).hexdigest().upper()[:16]


def run_case(cc, tag, src_text, fn, known_legacy, work):
    sp = os.path.join(work, tag + ".c")
    io.open(sp, "w", encoding="utf-8").write(src_text)
    bobj = os.path.join(work, tag + "_base.obj")
    iobj = os.path.join(work, tag + "_ir.obj")
    envb = dict(os.environ); envb.pop("QCC_CODEGEN", None); envb.pop("QCC_IR_STATS", None); envb["QCC_DBG_FMAP"] = "1"
    envi = dict(os.environ); envi["QCC_CODEGEN"] = "ir"; envi["QCC_IR_STATS"] = "1"; envi["QCC_DBG_FMAP"] = "1"
    rb = subprocess.run([cc, sp, "-c", "-o", bobj], capture_output=True, env=envb)
    ri = subprocess.run([cc, sp, "-c", "-o", iobj], capture_output=True, env=envi)
    if rb.returncode or ri.returncode:
        return ["%s: 编译失败 base=%d ir=%d" % (tag, rb.returncode, ri.returncode)], None
    err = (ri.stdout + ri.stderr).decode("utf-8", "replace")
    pro = RE_PRO.search(err)
    epi = RE_EPI.search(err)
    all_stats = [(m.group(2), m.group(3)) for m in RE_BODY.finditer(err)]
    bad = []
    if not pro:
        bad.append("%s: 缺 prologue IR-STATS" % tag)
    elif not (int(pro.group(1)) > 0 and pro.group(1) == pro.group(2)):
        bad.append("%s: prologue muted=%s replay=%s" % (tag, pro.group(1), pro.group(2)))
    if not epi:
        bad.append("%s: 缺 epilogue IR-STATS" % tag)
    elif not (int(epi.group(1)) > 0 and epi.group(1) == epi.group(2)):
        bad.append("%s: epilogue muted=%s replay=%s" % (tag, epi.group(1), epi.group(2)))
    for m, r in all_stats:
        if m != r:
            bad.append("%s: stats muted=%s replay=%s" % (tag, m, r))
    fnm = RE_FN.search(err)
    if not fnm:
        bad.append("%s: 缺 [IR-STATS] fn 行" % tag)
        fn_m, fn_r = -1, -1
    else:
        fn_m, fn_r = int(fnm.group(1)), int(fnm.group(2))
        if fn_m != fn_r:
            bad.append("%s: fn muted=%d != replay=%d" % (tag, fn_m, fn_r))
    total = sum(int(m) for m, _ in all_stats)
    epi_n = int(epi.group(1)) if epi else 0
    total_no_epi = (fn_r - epi_n) if fn_r >= 0 else (total - epi_n)
    tb = coff_text(bobj); ti = coff_text(iobj)
    hb, hi = sha(tb), sha(ti)
    if hb != hi:
        bad.append("%s: 产物 DIFF base=%s ir=%s" % (tag, hb, hi))
    fm = re.search(r"\[FMAP\] %s (\d+) (\d+)" % re.escape(fn), err)
    if not fm:
        bad.append("%s: 缺 [FMAP] %s" % (tag, fn))
        fmap_bytes = -1
    else:
        fmap_bytes = int(fm.group(2)) - int(fm.group(1))
        if not (total_no_epi + known_legacy <= fmap_bytes <= total_no_epi + known_legacy + 15):
            bad.append("%s: (total−尾声)=%d + 已知legacy=%d vs FMAP 范围=%d（超出 0..15 对齐）" %
                       (tag, total_no_epi, known_legacy, fmap_bytes))
    return bad, (total, total_no_epi, fmap_bytes, hb, hi)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--cc", default="qcc_zh_v1.exe")
    a = ap.parse_args()
    if not os.path.exists(a.cc):
        print("[ERR] 缺件: %s" % a.cc)
        return 2
    work = tmp_dir("ir_whole")
    bad_all = []
    for tag, src, fn, known in CASES:
        bad, info = run_case(a.cc, tag, src, fn, known, work)
        bad_all += bad
        if info:
            print("%-15s total=%d (去尾声 %d) · FMAP=%d · base=%s ir=%s %s" %
                  (tag, info[0], info[1], info[2], info[3], info[4],
                   "EQUAL" if info[3] == info[4] else "DIFF"))
    if bad_all:
        print("\n[FAIL] check_ir_whole_func:")
        for b in bad_all:
            print("  - " + b)
        return 1
    print("\n[通过 PASS] check_ir_whole_func：三个载体 序言+体+尾声 muted==replay；"
          "(total−尾声)+KNOWN_LEGACY == FMAP 范围；产物逐字节相同")
    print("⚠️ 边界：`.text` 中的 __qcc_* 运行时仍 legacy（AC2 全 .text 未达成，见 README §AC2 边界）")
    return 0


if __name__ == "__main__":
    sys.exit(main())
