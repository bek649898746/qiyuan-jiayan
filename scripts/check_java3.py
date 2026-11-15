# -*- coding: utf-8 -*-
"""check_java3.py — ORD-20261108-152 三缺陷综合判据（P1 已修 / P2·P3 未修）。

  P1 `构 类 {…};`            ⟹ rc != 0 且报「结构体 标签名」（已修，应绿）
  P2 全局 `常 整 上限 = 10;` ⟹ rc == 0 且输出 上限 = 10（未修，应红）
  P3 跨函数 `抛出`           ⟹ 只输出「结果 = -2」，不许出现「抛出之后还执行了」（未修，应红）

整体 rc != 0 表示还有未修项（与台账 repro_expect=red 一致）。
用法: python scripts/check_java3.py --cc qcc_zh_v1.exe
"""
import argparse
import io
import os
import subprocess
import sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qcc_tmp import tmp_dir  # noqa: E402

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
HERE = os.path.dirname(os.path.abspath(__file__))

SRC = {
    "P1": """构 类 { 整 x; };
整 主(空) { 构 类 甲; 甲.x = 1; 输出("x = %d\\n", 甲.x); 返 0; }
""",
    "P2": """常 整 上限 = 10;
整 主(空) { 输出("上限 = %d\\n", 上限); 返 0; }
""",
    "P3": """整 危险(整 b) {
    若 (b == 0) { 抛出 1; }
    输出("抛出之后还执行了\\n");
    返 99;
}
整 主(空) {
    整 结果 = -1;
    试 { 危险(0); 输出("调用正常返回了\\n"); } 捕获 { 结果 = -2; }
    输出("结果 = %d\\n", 结果);
    返 0;
}
""",
}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--cc", default="qcc_zh_v1.exe")
    a = ap.parse_args()
    cc = a.cc if os.path.isabs(a.cc) else os.path.join(os.path.dirname(HERE), a.cc)
    if not os.path.exists(cc):
        print("[ERR] 缺件: %s" % cc)
        return 2
    d = tmp_dir("java3")
    bad = []
    for tag, src in SRC.items():
        sp = os.path.join(d, tag + ".jy")
        exe = os.path.join(d, tag + ".exe")
        io.open(sp, "w", encoding="utf-8").write(src)
        r = subprocess.run([cc, sp, "-o", exe], capture_output=True, timeout=120)
        err = (r.stdout + r.stderr).decode("utf-8", "replace")
        if tag == "P1":
            ok = (r.returncode != 0) and ("结构体 标签名" in err)
            print("P1 结构体标签撞词      rc=%d  %s" % (r.returncode, "PASS" if ok else "FAIL"))
            if not ok:
                bad.append("P1: rc=%d（应 rc!=0 且报「结构体 标签名」）" % r.returncode)
            continue
        if r.returncode != 0:
            print("%s 编译失败 rc=%d  %s" % (tag, r.returncode, err.strip()[:100]))
            bad.append("%s: 编译失败 rc=%d" % (tag, r.returncode))
            continue
        rr = subprocess.run([exe], capture_output=True, timeout=10)
        out = (rr.stdout + rr.stderr).decode("utf-8", "replace").strip()
        if tag == "P2":
            ok = (rr.returncode == 0) and ("上限 = 10" in out)
            print("P2 全局 const 整数     rc=%d out=%r  %s" % (rr.returncode, out[:60], "PASS" if ok else "FAIL"))
            if not ok:
                bad.append("P2: rc=%d out=%r（应 rc=0 且含 上限 = 10）" % (rr.returncode, out))
        else:
            ok = (rr.returncode == 0) and ("结果 = -2" in out) and ("抛出之后还执行了" not in out)
            print("P3 跨函数抛出           rc=%d out=%r  %s" % (rr.returncode, out[:80], "PASS" if ok else "FAIL"))
            if not ok:
                bad.append("P3: rc=%d out=%r（应只输出 结果 = -2）" % (rr.returncode, out))
    if bad:
        print("\n[FAIL] check_java3（还有未修项）:\n  - " + "\n  - ".join(bad))
        return 1
    print("\n[通过 PASS] check_java3：三缺陷全部闭环")
    return 0


if __name__ == "__main__":
    sys.exit(main())
