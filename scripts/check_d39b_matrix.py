# -*- coding: utf-8 -*-
"""D39-b 判据矩阵：-graph 输出路径 与 源文件/产物 的 同一性 判定（绝对 vs 相对 是核心残留）"""
import os, sys, subprocess, shutil, tempfile, hashlib, json
sys.stdout.reconfigure(encoding='utf-8', errors='replace')

CC = sys.argv[1] if len(sys.argv) > 1 else r"build\dev_fast.exe"
SRC = "int 主(空) {\n    返 7;\n}\n"

def sha(p):
    return hashlib.sha256(open(p, "rb").read()).hexdigest()[:16]

def run(cwd, args):
    r = subprocess.run([os.path.abspath(os.path.join(os.path.dirname(__file__), "..", CC))] + args,
                       cwd=cwd, capture_output=True)
    return r.returncode, (r.stdout + r.stderr).decode("utf-8", "replace")

def case(name, graph_expr, extra=None, expect_rc=None, expect_intact=None):
    d = tempfile.mkdtemp(prefix="d39b_")
    os.makedirs(os.path.join(d, "a"), exist_ok=True)
    src = os.path.join(d, "t.jy")
    open(src, "w", encoding="utf-8").write(SRC)
    before = sha(src)
    size0 = os.path.getsize(src)
    g = graph_expr(d)
    args = ["t.jy"] + (extra(d) if extra else []) + ["-graph", g]
    rc, out = run(d, args)
    intact = sha(src) == before
    ok = True
    msg = []
    if expect_rc is not None and rc != expect_rc:
        ok = False; msg.append("rc=%d 期望 %d" % (rc, expect_rc))
    if expect_intact is not None and intact != expect_intact:
        ok = False; msg.append("源完好=%s 期望 %s (大小 %d, 原 %d)" % (intact, expect_intact, os.path.getsize(src), size0))
    print("[%s] %-14s rc=%d 源完好=%-5s %s" % ("PASS" if ok else "FAIL", name, rc, intact,
          "" if ok else "| " + "; ".join(msg)))
    if not ok:
        print("    args: %s\n    out: %s" % (args, out.strip()[:300]))
    shutil.rmtree(d, ignore_errors=True)
    return ok

allok = True
allok &= case("相对同形",     lambda d: "t.jy",                      expect_rc=1, expect_intact=True)
allok &= case("./前缀",       lambda d: ".\\t.jy",                    expect_rc=1, expect_intact=True)
allok &= case("..折叠",       lambda d: "a\\..\\t.jy",                expect_rc=1, expect_intact=True)
allok &= case("绝对路径",     lambda d: os.path.join(d, "t.jy"),      expect_rc=1, expect_intact=True)
allok &= case("绝对+大写",    lambda d: os.path.join(d, "T.JY"),      expect_rc=1, expect_intact=True)
allok &= case("绝对+斜杠",    lambda d: os.path.join(d, "t.jy").replace("\\", "/"), expect_rc=1, expect_intact=True)
allok &= case("绝对+..",      lambda d: os.path.join(d, "a", "..", "t.jy"), expect_rc=1, expect_intact=True)
allok &= case("产物同形",     lambda d: "t.exe", extra=lambda d: ["-o", "t.exe"], expect_rc=1)
allok &= case("产物绝对",     lambda d: os.path.join(d, "t.exe"), extra=lambda d: ["-o", "t.exe"], expect_rc=1)
allok &= case("正常JSON",     lambda d: "t.json",                     expect_rc=0, expect_intact=True)
print("=== 矩阵 %s ===" % ("全绿" if allok else "有红"))
sys.exit(0 if allok else 1)
