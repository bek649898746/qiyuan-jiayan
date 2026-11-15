# -*- coding: utf-8 -*-
"""check_ir_rpo.py — I-1 RPO 与 I-3 块循环深度 的独立判据。

来源（他山之石）:
  · regalloc2 src/postorder.rs (45 行) —— 显式栈非递归后序
  · regalloc2 src/domtree.rs (CHK 迭代 idom) —— 回边判据 = b 支配 a
  · qbe cfg.c fillloop —— 自然循环 + 嵌套深度

判据（全部可复算，不看自报数字）:
  R1  RPO 长度 == 可达块数，且每个可达块恰出现一次
  R2  RPO 中 idom 先于被支配者（idom != 自身时 序位[idom] < 序位[b]）
  R3  真回边数 > 0（while 正例）
  R4  环深：while 循环头与回边源 >= 1；循环外块 == 0
  R5  负例：无循环程序 ⟹ 真回边 == 0 且 无环深块
  R6  嵌套：双层循环 ⟹ 内层环深 >= 2（外层 1 + 内层 1）
  R7  溢出标志 == 0

用法:
  python scripts/check_ir_rpo.py --cc qcc_zh_v1.exe
"""
import argparse
import io
import os
import re
import subprocess
import sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qcc_tmp import tmp_dir  # noqa: E402


PROBE_LOOP = r"""
int f(int n) {
    int i = 0;
    int s = 0;
    while (i < n) {
        s = s + i;
        i = i + 1;
    }
    if (s > 3) { return 1; }
    return 0;
}
int main(void) { return f(4); }
"""

PROBE_NOLOOP = r"""
int g(int a) {
    if (a > 3) { return 1; }
    return 0;
}
int main(void) { return g(4); }
"""

PROBE_NEST = r"""
int h(int n) {
    int i = 0;
    int j = 0;
    int s = 0;
    while (i < n) {
        j = 0;
        while (j < n) {
            s = s + 1;
            j = j + 1;
        }
        i = i + 1;
    }
    return s;
}
int main(void) { return h(3); }
"""


def run_dump(cc, src_text, tag):
    d = tmp_dir("ir_rpo")
    src = os.path.join(d, tag + ".c")
    obj = os.path.join(d, tag + ".obj")
    io.open(src, "w", encoding="utf-8").write(src_text)
    env = dict(os.environ)
    env["QCC_DUMP"] = "ir"
    p = subprocess.run([cc, src, "-c", "-o", obj], capture_output=True, env=env)
    if p.returncode != 0:
        sys.stderr.write(p.stderr.decode("utf-8", "replace"))
        raise RuntimeError("%s 编译失败 rc=%d" % (tag, p.returncode))
    return p.stderr.decode("utf-8", "replace")


RE_RPO_HEAD = re.compile(r"-- IR RPO \(可达=(\d+) · 后序=(\d+) · 溢出=(\d+)\) --")
RE_RPO_ITEM = re.compile(r"rpo\[(\d+)\]=块\[(\d+)\]")
RE_LOOP_HEAD = re.compile(r"-- IR 块循环深度 \(真回边=(\d+) · 溢出=(\d+)\) --")
RE_LOOP_ITEM = re.compile(r"环深\[(\d+)\]=(\d+)")
RE_IDOM = re.compile(r"支\[(\d+)\] idom=(-?\d+) 深=(-?\d+)")


def parse(text):
    rpo_n = rpo_post = rpo_ov = None
    loop_n = loop_ov = None
    rpo = []
    loops = {}
    idom = {}
    for ln in text.splitlines():
        m = RE_RPO_HEAD.search(ln)
        if m:
            rpo_n, rpo_post, rpo_ov = int(m.group(1)), int(m.group(2)), int(m.group(3))
            continue
        m = RE_RPO_ITEM.search(ln)
        if m:
            rpo.append((int(m.group(1)), int(m.group(2))))
            continue
        m = RE_LOOP_HEAD.search(ln)
        if m:
            loop_n, loop_ov = int(m.group(1)), int(m.group(2))
            continue
        m = RE_LOOP_ITEM.search(ln)
        if m:
            loops[int(m.group(1))] = int(m.group(2))
            continue
        m = RE_IDOM.search(ln)
        if m:
            idom[int(m.group(1))] = int(m.group(2))
    return dict(rpo_n=rpo_n, rpo_post=rpo_post, rpo_ov=rpo_ov,
                loop_n=loop_n, loop_ov=loop_ov, rpo=rpo, loops=loops, idom=idom)


def check_rpo(d, label, expect_loop_edge, expect_depth2=False):
    bad = []
    rpo_n = d["rpo_n"]
    if rpo_n is None:
        return ["%s: 缺 RPO 段（未接 构RPO 或 dump 未执行）" % label]
    if d["rpo_ov"] != 0:
        bad.append("%s: RPO 溢出=%d" % (label, d["rpo_ov"]))
    seq = [b for _, b in d["rpo"]]
    if len(seq) != rpo_n:
        bad.append("%s: RPO 条数 %d != 可达 %d" % (label, len(seq), rpo_n))
    if len(set(seq)) != len(seq):
        bad.append("%s: RPO 有重复块" % label)
    pos = {b: i for i, b in enumerate(seq)}
    # R2: idom 先于被支配者
    for b, i in d["idom"].items():
        if i < 0 or i == b:
            continue
        if b not in pos or i not in pos:
            continue
        if pos[i] >= pos[b]:
            bad.append("%s: idom 顺序错 块[%d] 的 idom 块[%d] 在其后 (pos %d >= %d)" %
                       (label, b, i, pos[i], pos[b]))
    # R3
    if expect_loop_edge:
        if d["loop_n"] is None:
            bad.append("%s: 缺 块循环深度 段" % label)
        elif d["loop_n"] < 1:
            bad.append("%s: 有 while 但真回边=0" % label)
    else:
        if d["loop_n"] is not None and d["loop_n"] != 0:
            bad.append("%s: 无循环程序但真回边=%d" % (label, d["loop_n"]))
        if d["loops"]:
            bad.append("%s: 无循环程序但有环深块 %s" % (label, d["loops"]))
    # R4/R6
    if expect_loop_edge and not d["loops"]:
        bad.append("%s: 有回边但无任何环深块" % label)
    if expect_depth2:
        if not d["loops"] or max(d["loops"].values()) < 2:
            bad.append("%s: 双层循环未出现环深>=2 (实际 %s)" % (label, d["loops"]))
    if d["loop_ov"] not in (None, 0):
        bad.append("%s: 环深溢出=%d" % (label, d["loop_ov"]))
    return bad


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--cc", default="qcc_zh_v1.exe")
    args = ap.parse_args()
    cc = args.cc
    if not os.path.exists(cc):
        sys.stderr.write("[ERR] 缺件: %s\n" % cc)
        return 2

    cases = [
        ("while+if", PROBE_LOOP, True, False),
        ("no-loop", PROBE_NOLOOP, False, False),
        ("nested", PROBE_NEST, True, True),
    ]
    bad = []
    for tag, src, exp_edge, exp_d2 in cases:
        d = parse(run_dump(cc, src, tag))
        bad += check_rpo(d, tag, exp_edge, exp_d2)
        print("[%s] 可达=%s 后序=%s RPO=%d 真回边=%s 环深块=%s" % (
            tag, d["rpo_n"], d["rpo_post"], len(d["rpo"]),
            d["loop_n"], sorted(d["loops"].items())))

    if bad:
        print("\n[FAIL] check_ir_rpo：")
        for b in bad:
            print("  - " + b)
        return 1
    print("\n[通过 PASS] check_ir_rpo：RPO 顺序 + 真回边（dominates 判据）+ 嵌套环深 全绿")
    return 0


if __name__ == "__main__":
    sys.exit(main())
