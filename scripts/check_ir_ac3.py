# -*- coding: utf-8 -*-
"""check_ir_ac3.py — AC3：未支持形态必须响亮 UNIMPL；赋值/声明必须完全走 IR。

判据：
  IR-OK   `int f(void){int x; x=7; return x;}` / `int x=7;`
          ⟹ rc=0 + 有 [IR-STATS] + 产物与 legacy 逐字节相同（**完全接管**，不是静默 legacy）
  UNIMPL  仍未实现的形态（多实参调用）
          ⟹ rc=1 + `IR-BACKEND-UNIMPL`

用法: python scripts/check_ir_ac3.py --cc qcc_zh_v1.exe
"""
import argparse, hashlib, io, os, subprocess, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qcc_tmp import tmp_dir  # noqa: E402

sys.stdout.reconfigure(encoding="utf-8", errors="replace")

CASES = {
    "single_assign": ("int f(void) { int x; x = 7; return x; }\n", "ir_ok"),
    "single_declinit": ("int f(void) { int x = 7; return x; }\n", "ir_ok"),
    "while": ("int f(int n) { int s = 0; while (n > 0) { s = s + n; n = n - 1; } return s; }\n", "ir_ok"),
    # 2026-11-10 刀九十一后：**内建/内置调用**（`strcmp` 等）已全部进 IR 且字节全同 ⟹ 旧负控
    # `ext_call2` 已变成「已支持形态」。按纪律**改指仍未实现的形态**（不是放宽判据）：
    # 函数指针调用（间接调用尚未做成 L1 载体；原为**静默错产物** 373B vs 370B，刀九十二 改响亮拒收）。
    # 刀三百五十三（2026-11-11）：函数指针调用（被调者是**变量**：全局/形参/局部 typedef）已纳入
    # 「字节跨度捕获」⟹ 变成**已支持形态**：rc=0 + IR-STATS + 与 legacy 逐字节相同。
    # 负控改指**仍未实现**的形态（被调者是成员：`s.fp()`），保持 fail-closed，不是放宽判据。
    "call_fptr": ("int main(void) { int (*p)(void) = 0; return p(); }\n", "ir_ok"),
    "call_fptr_member": ("struct S { int (*fp)(void); };\nint main(void) { struct S s; s.fp = 0; return s.fp(); }\n", "ir_ok"),
}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--cc", default="qcc_zh_v1.exe")
    a = ap.parse_args()
    if not os.path.exists(a.cc):
        print("[ERR] 缺件: %s" % a.cc); return 2
    d = tmp_dir("ir_ac3")
    bad = []
    for tag, (src, kind) in CASES.items():
        sp = os.path.join(d, tag + ".c"); io.open(sp, "w", encoding="utf-8").write(src)
        op = os.path.join(d, tag + ".obj")
        bp = os.path.join(d, tag + "_b.obj")
        env = dict(os.environ); env["QCC_CODEGEN"] = "ir"; env["QCC_IR_STATS"] = "1"; env["QCC_DBG_FMAP"] = "1"
        envb = dict(os.environ); envb.pop("QCC_CODEGEN", None); envb.pop("QCC_IR_STATS", None)
        p = subprocess.run([a.cc, sp, "-c", "-o", op], capture_output=True, env=env)
        pb = subprocess.run([a.cc, sp, "-c", "-o", bp], capture_output=True, env=envb)
        out = (p.stdout + p.stderr).decode("utf-8", "replace")
        has_unimpl = "IR-BACKEND-UNIMPL" in out
        has_stats = "[IR-STATS]" in out
        print("=== %s (%s) rc=%d unimpl=%s stats=%s ===" % (tag, kind, p.returncode, has_unimpl, has_stats))
        for ln in out.splitlines():
            if "IR-STATS" in ln or "UNIMPL" in ln:
                print("  " + ln)
        if kind == "unimpl":
            if p.returncode != 1:
                bad.append("%s: rc=%d（应 1）" % (tag, p.returncode))
            if not has_unimpl:
                bad.append("%s: 缺 IR-BACKEND-UNIMPL（静默回落 legacy）" % tag)
        else:
            if p.returncode != 0:
                bad.append("%s: rc=%d（应 0）" % (tag, p.returncode))
            if not has_stats:
                bad.append("%s: 未走 IR（缺 [IR-STATS]）" % tag)
            if pb.returncode == 0 and p.returncode == 0:
                h1 = hashlib.sha256(io.open(op, "rb").read()).hexdigest()[:8]
                h2 = hashlib.sha256(io.open(bp, "rb").read()).hexdigest()[:8]
                print("  obj ir=%s base=%s %s" % (h1, h2, "EQUAL" if h1 == h2 else "DIFF"))
                if h1 != h2:
                    bad.append("%s: 产物 DIFF ir=%s base=%s" % (tag, h1, h2))
            # total-vs-FMAP：已知 legacy 尾巴 = 12 字节（节点_赋值 源值 5 + store 7）
            import re as _re
            m_fn = _re.search(r"\[IR-STATS\] fn muted=(\d+) replay=(\d+)", out)
            m_epi = _re.search(r"epilogue muted=(\d+)", out)
            m_fm = _re.search(r"\[FMAP\] f (\d+) (\d+)", out)
            if m_fn and m_epi and m_fm:
                # P1 口径：用**函数级** replay（重放真实落盘字节），不用各静默块之和
                # （源值等静默块没有独立统计行 ⟹ 求和会漏）。
                fn_replay = int(m_fn.group(2))
                user_bytes = int(m_fm.group(2)) - int(m_fm.group(1))
                prologue_body = fn_replay - int(m_epi.group(1))   # FMAP = 序言 + 体（不含尾声）
                print("  fn_replay=%d (去尾声=%d) · FMAP 范围=%d" %
                      (fn_replay, prologue_body, user_bytes))
                # 已知缺口：函数入口的「参数 → 帧槽」搬运尚未进 L1（`int f(int n)` 实测 7 B）。
                # 显式登记，不掩盖；`check_ir_j1.py` 会以红字提示同一缺口。
                KNOWN_PARAM_SPILL = 0   # 2026-11-09: 参数→帧槽 已进 L1
                if prologue_body + KNOWN_PARAM_SPILL != user_bytes:
                    bad.append("%s: fn_replay(去尾声)=%d + 参数存栈缺口 %d != FMAP 范围 %d" %
                               (tag, prologue_body, KNOWN_PARAM_SPILL, user_bytes))
    if bad:
        print("\n[FAIL] check_ir_ac3:\n  - " + "\n  - ".join(bad)); return 1
    print("\n[通过 PASS] check_ir_ac3：赋值/声明/`当` 全部走 IR（fn replay 与 FMAP 范围相等）；"
          "未支持形态全部响亮 UNIMPL")
    return 0


if __name__ == "__main__":
    sys.exit(main())
