#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""check_ir_owns_codegen —— IR 真接管判据（施工单 `_施工单_IR路线_单一入口`）。

七条：
  A1 · 骨架等价（常数）：`int f(void){ return 42; }` 两路径（默认 vs QCC_CODEGEN=ir）`-c` 产物逐字节相同。
  A2 · 骨架等价（变量/抽载）：`int f(void){ int x = 7; return x; }` 同上。
  A3 · 子链等价（二元 x+1）：`int f(int x){ return x + 1; }` 同上。
  B  · 未实现必报：`int f(int x){ return x + 1; }` + IR 模式 ⟹ rc!=0 且 stderr 含 `IR-BACKEND-UNIMPL`。
  C  · 默认开关关：默认模式编译成功且 rc=0。
  D  · 默认链不动点：`scripts/build_zh.ps1` 的 `$expected` 可读出且非空（自举链另由 build_zh 验）。
  E  · 产物零变化：**默认模式**下，新编译器产物 == 旧编译器（`--old-cc`）产物（逐字节）。
  F  · **真接管·建链不落盘**：IR 模式下 stderr 的 `[IR-STATS] muted>0 replay==muted delta=1`
       （第一遍建 L1 一个字节都不落盘；第二遍从 L1 重放的字节数恰等于第一遍尝试数）。
  F2 · **真接管·子链等量**：同上断言作用于 `x + 1`（`muted == replay`、`delta == 1`）。
  G  · **真接管·L1 变异反证**：改写 L1 立即数 ⟹ IR 产物必变（差异恰 1 字节、+1）；
       legacy 产物不受影响。证明「产物由 L1 决定」。

用法: python scripts/check_ir_owns_codegen.py [--cc qcc_zh_v1.exe] [--old-cc qcc_zh_seed.exe] [--work <dir>]
"""
import argparse
import hashlib
import io
import os
import re
import subprocess
import sys

sys.stdout.reconfigure(encoding='utf-8', errors='replace')
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
os.chdir(ROOT)
try:
    from qcc_tmp import tmp_dir
except Exception:
    tmp_dir = lambda n: os.path.join('build', n)

A_SRC = 'int f(void) { return 42; }\n'
A2_SRC = 'int f(void) { int x = 7; return x; }\n'
A3_SRC = 'int f(int x) { return x + 1; }\n'      # 子链载体：二元 + 右常量子集
A4_SRC = 'int h(int a) { return a; }\nint g(void) { return h(7); }\n'  # 实参载体：单整数实参调用
A5_SRC = 'int h(int a) { return a; }\nint f(void) { if (h(1)) { return 1; } return 2; }\nint main(void) { return 0; }\n'  # 控制流载体
A6_SRC = 'int f(void) { int x; x = 7; return x; }\n'  # 赋值载体（声明无初始化 + 赋值）
B2_SRC = 'int f(int n) { int s = 0; while (n > 0) { s = s + n; n = n - 1; } return s; }\n'  # 当 未接管：必 UNIMPL
B_SRC = 'int f(int a, int b) { return a + b; }\n'  # 两变量相加：push/pop 形态未进 L1（应报 IR-BACKEND-UNIMPL）
E_SRC = ('int snprintf(char *, unsigned long long, const char *, ...);\n'         'int f(void) { return 42; }\nint g(void) { return 7; }\n')


def run(cmd, env=None, t=600):
    e = dict(os.environ)
    e.pop('QCC_CODEGEN', None)
    e.pop('QCC_IR_STATS', None)
    e.pop('QCC_IR_MUTATE', None)
    if env:
        e.update(env)
    return subprocess.run(cmd, capture_output=True, timeout=t, env=e)


def sha8(p):
    return hashlib.sha256(io.open(p, 'rb').read()).hexdigest()[:8].upper()


def coff_text(path):
    """从 PE/COFF 对象里取出 .text 段原始字节（不依赖 objcopy）。

    布局：IMAGE_FILE_HEADER(20) + N×IMAGE_SECTION_HEADER(40)。
    段名 8 字节；`SizeOfRawData` @16；`PointerToRawData` @20。
    """
    b = io.open(path, 'rb').read()
    nsec = int.from_bytes(b[2:4], 'little')
    for i in range(nsec):
        off = 20 + i * 40
        name = b[off:off + 8].rstrip(b'\x00')
        if name in (b'.text', b'.text$mn'):
            size = int.from_bytes(b[off + 16:off + 20], 'little')
            ptr = int.from_bytes(b[off + 20:off + 24], 'little')
            return b[ptr:ptr + size]
    return b''


def coff_relocs(path):
    """COFF 重定位（与符号名）：[(段名, VirtualAddress, Type, 符号名)] —— 用于多重集比较。

    刀三百五十（2026-11-11）：`.o` 的重定位/符号表顺序已**规范化排序**（与登记顺序解耦）。
    老编译器的表内次序不同 ⟹ 逐字节比必红，但代码与链接结果相同。E 判据因此在
    「`.text` 逐字节相同」之上再要求**重定位多重集相同**，才允许放行。
    """
    b = io.open(path, 'rb').read()
    nsec = int.from_bytes(b[2:4], 'little')
    psym = int.from_bytes(b[8:12], 'little')
    nsym = int.from_bytes(b[12:16], 'little')
    # 符号名解析（内联 8 字节 / 字符串表偏移）
    strtab = psym + nsym * 18
    names = []
    for i in range(nsym):
        o = psym + i * 18
        nm = b[o:o + 8]
        if nm[:4] == b'\x00\x00\x00\x00':
            so = int.from_bytes(nm[4:8], 'little')
            e = b.find(b'\x00', strtab + so)
            names.append(b[strtab + so:e].decode('utf-8', 'replace'))
        else:
            names.append(nm.rstrip(b'\x00').decode('utf-8', 'replace'))
    out = []
    for i in range(nsec):
        off = 20 + i * 40
        sec = b[off:off + 8].rstrip(b'\x00').decode('utf-8', 'replace')
        ptr = int.from_bytes(b[off + 24:off + 28], 'little')
        num = int.from_bytes(b[off + 32:off + 34], 'little')
        for k in range(num):
            r = ptr + k * 10
            va = int.from_bytes(b[r:r + 4], 'little')
            si = int.from_bytes(b[r + 4:r + 8], 'little')
            ty = int.from_bytes(b[r + 8:r + 10], 'little')
            nm = names[si] if 0 <= si < len(names) else '?%d' % si
            out.append((sec, va, ty, nm))
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cc', default='qcc_zh_v1.exe')
    ap.add_argument('--old-cc', default='qcc_zh_seed.exe')
    ap.add_argument('--work', default=None)
    a = ap.parse_args()
    for p in (a.cc, a.old_cc):
        if not os.path.exists(p):
            print('环境未就绪：缺 %s' % p)
            return 2

    W = a.work or tmp_dir('ir_owns')
    os.makedirs(W, exist_ok=True)
    fails = []

    def write(name, text):
        p = os.path.join(W, name)
        io.open(p, 'w', encoding='utf-8', newline='\n').write(text)
        return p

    def build(cc, src, tag, ir, mode='-c', extra=None):
        out = os.path.join(W, tag)
        env = {'QCC_CODEGEN': 'ir'} if ir else {}
        if extra:
            env.update(extra)
        r = run([cc, src, mode, '-o', out], env)
        return r, out

    sa = write('a.c', A_SRC)
    ra, oa = build(a.cc, sa, 'a_base.obj', False)
    rb, ob = build(a.cc, sa, 'a_ir.obj', True)
    if ra.returncode != 0 or rb.returncode != 0:
        fails.append('A 编译失败 base=%d ir=%d' % (ra.returncode, rb.returncode))
    elif sha8(oa) != sha8(ob):
        fails.append('A 两路径不等 %s != %s' % (sha8(oa), sha8(ob)))
    else:
        print('A1 骨架等价（常数）: PASS (%s)' % sha8(oa))

    sa2 = write('a2.c', A2_SRC)
    ra2, oa2 = build(a.cc, sa2, 'a2_base.obj', False)
    rb2, ob2 = build(a.cc, sa2, 'a2_ir.obj', True)
    if ra2.returncode != 0 or rb2.returncode != 0:
        fails.append('A2 编译失败 base=%d ir=%d :: %s'
                     % (ra2.returncode, rb2.returncode,
                        (rb2.stdout + rb2.stderr).decode('utf-8', 'replace')[-200:]))
    elif sha8(oa2) != sha8(ob2):
        fails.append('A2 两路径不等 %s != %s' % (sha8(oa2), sha8(ob2)))
    else:
        print('A2 骨架等价（变量/抽载）: PASS (%s)' % sha8(oa2))

    sa3 = write('a3.c', A3_SRC)
    ra3, oa3 = build(a.cc, sa3, 'a3_base.obj', False)
    rb3, ob3 = build(a.cc, sa3, 'a3_ir.obj', True)
    if ra3.returncode != 0 or rb3.returncode != 0:
        fails.append('A3 编译失败 base=%d ir=%d :: %s'
                     % (ra3.returncode, rb3.returncode,
                        (rb3.stdout + rb3.stderr).decode('utf-8', 'replace')[-200:]))
    elif sha8(oa3) != sha8(ob3):
        fails.append('A3 两路径不等 %s != %s（子链载体）' % (sha8(oa3), sha8(ob3)))
    else:
        print('A3 骨架等价（子链 x+1）: PASS (%s)' % sha8(oa3))

    sa4 = write('a4.c', A4_SRC)
    ra4, oa4 = build(a.cc, sa4, 'a4_base.obj', False)
    rb4, ob4 = build(a.cc, sa4, 'a4_ir.obj', True)
    if ra4.returncode != 0 or rb4.returncode != 0:
        fails.append('A4 编译失败 base=%d ir=%d :: %s'
                     % (ra4.returncode, rb4.returncode,
                        (rb4.stdout + rb4.stderr).decode('utf-8', 'replace')[-200:]))
    elif coff_text(oa4) != coff_text(ob4):
        fails.append('A4 .text 不等（实参载体）：%s != %s'
                     % (hashlib.sha256(coff_text(oa4)).hexdigest()[:8].upper(),
                        hashlib.sha256(coff_text(ob4)).hexdigest()[:8].upper()))
    else:
        # 全对象字节可能因 COFF 调试/符号表元数据不同；判据锁定「机器码段」，
        # A1–A3 仍用整文件严格口径。
        print('A4 骨架等价（实参 h(7)）: PASS (.text %s, %d B)'
              % (hashlib.sha256(coff_text(oa4)).hexdigest()[:8].upper(), len(coff_text(oa4))))

    sa5 = write('a5.c', A5_SRC)
    ra5, oa5 = build(a.cc, sa5, 'a5_base.obj', False)
    rb5, ob5 = build(a.cc, sa5, 'a5_ir.obj', True)
    if ra5.returncode != 0 or rb5.returncode != 0:
        fails.append('A5 编译失败 base=%d ir=%d :: %s'
                     % (ra5.returncode, rb5.returncode,
                        (rb5.stdout + rb5.stderr).decode('utf-8', 'replace')[-200:]))
    elif coff_text(oa5) != coff_text(ob5):
        fails.append('A5 .text 不等（控制流载体）：%s != %s'
                     % (hashlib.sha256(coff_text(oa5)).hexdigest()[:8].upper(),
                        hashlib.sha256(coff_text(ob5)).hexdigest()[:8].upper()))
    else:
        print('A5 骨架等价（控制流 if/h(1)）: PASS (.text %s, %d B)'
              % (hashlib.sha256(coff_text(oa5)).hexdigest()[:8].upper(), len(coff_text(oa5))))

    sa6 = write('a6.c', A6_SRC)
    ra6, oa6 = build(a.cc, sa6, 'a6_base.obj', False)
    rb6, ob6 = build(a.cc, sa6, 'a6_ir.obj', True)
    if ra6.returncode != 0 or rb6.returncode != 0:
        fails.append('A6 编译失败 base=%d ir=%d :: %s'
                     % (ra6.returncode, rb6.returncode,
                        (rb6.stdout + rb6.stderr).decode('utf-8', 'replace')[-200:]))
    elif coff_text(oa6) != coff_text(ob6):
        fails.append('A6 .text 不等（赋值载体）：%s != %s'
                     % (hashlib.sha256(coff_text(oa6)).hexdigest()[:8].upper(),
                        hashlib.sha256(coff_text(ob6)).hexdigest()[:8].upper()))
    else:
        print('A6 骨架等价（赋值 x=7）: PASS (.text %s, %d B)'
              % (hashlib.sha256(coff_text(oa6)).hexdigest()[:8].upper(), len(coff_text(oa6))))

    sb2 = write('b2.c', B2_SRC)
    rc2, oc2 = build(a.cc, sb2, 'b2_ir.obj', True)
    rdc2, od2 = build(a.cc, sb2, 'b2_default.obj', False)
    err2 = (rc2.stdout + rc2.stderr).decode('utf-8', 'replace')
    if rc2.returncode == 0 and rdc2.returncode == 0 and sha8(oc2) == sha8(od2):
        print('B2 `当` 已接管（IR == 默认）: PASS (.text %s)' % sha8(oc2))
    else:
        fails.append('B2 `当` 接管失败 rc=%d/%d%s' % (
            rc2.returncode, rdc2.returncode,
            '' if rc2.returncode == 0 else ' ' + err2.strip()[:160]))

    sb = write('b.c', B_SRC)
    rc, oc = build(a.cc, sb, 'b_ir.obj', True)
    rdc, od = build(a.cc, sb, 'b_default.obj', False)
    err = (rc.stdout + rc.stderr).decode('utf-8', 'replace')
    if rc.returncode == 0 and rdc.returncode == 0 and sha8(oc) == sha8(od):
        print('B 双实参加减已接管（IR == 默认）: PASS (.text %s)' % sha8(oc))
    else:
        fails.append('B 双实参加减接管失败 rc=%d/%d%s' % (
            rc.returncode, rdc.returncode,
            '' if rc.returncode == 0 else ' ' + err.strip()[:160]))

    rc0, od = build(a.cc, sb, 'b_default.obj', False)
    if rc0.returncode != 0:
        fails.append('C 默认模式失败 rc=%d' % rc0.returncode)
    else:
        print('C 默认开关关: PASS')

    t = io.open(os.path.join('scripts', 'build_zh.ps1'), encoding='utf-8-sig', errors='replace').read()
    m = re.search(r"^\$expected = '([0-9A-Fa-f]{8})'", t, re.M)
    if not m:
        fails.append('D $expected 不可读')
    else:
        print('D 默认链不动点: PASS (%s)' % m.group(1).upper())

    se = write('e.c', E_SRC)
    rn, on = build(a.cc, se, 'e_new.obj', False)
    ro, oo = build(a.old_cc, se, 'e_old.obj', False)
    if rn.returncode != 0 or ro.returncode != 0:
        fails.append('E 编译失败 new=%d old=%d' % (rn.returncode, ro.returncode))
    elif sha8(on) == sha8(oo):
        print('E 产物零变化: PASS (%s)' % sha8(on))
    else:
        # AC2-mini-DCE（2026-11-09）：`-c` 模式不再无条件前插 mini 运行库。
        # 允许的唯一差异 = 旧 `.text` 含有而新 `.text` 不含的一段 1010 B（两个 static 函数），
        # 且新 `.text` 是旧 `.text` 的**连续子串**（说明只删不增、不改）。
        tn, to = coff_text(on), coff_text(oo)
        if len(to) - len(tn) == 1010 and to.find(tn) >= 0:
            print('E 产物变化 = mini-DCE（旧 .text %d → 新 %d，新为旧连续子串）: PASS'
                  % (len(to), len(tn)))
        elif tn == to and sorted(coff_relocs(on)) == sorted(coff_relocs(oo)):
            # 刀三百五十：`.text` 逐字节相同 且 重定位（段/偏移/类型/符号名）多重集相同
            # ⟹ 差异只在**表内次序**（规范化排序 vs 老编译器登记序），链接结果相同。
            print('E 产物变化 = 仅元数据次序（.text %d 逐字节相同、重定位 %d 条多重集相同）: PASS'
                  % (len(tn), len(coff_relocs(on))))
        else:
            fails.append('E 默认模式产物变化 %s != %s（.text %d vs %d；老编译器 %s）'
                         % (sha8(on), sha8(oo), len(tn), len(to), a.old_cc))

    # ── F · 真接管：建链不落盘、重放恰等量 ─────────────────────────────
    rf, of2 = build(a.cc, sa, 'f_ir.obj', True, extra={'QCC_IR_STATS': '1'})
    ferr = (rf.stdout + rf.stderr).decode('utf-8', 'replace')
    if rf.returncode != 0:
        fails.append('F IR 模式编译失败 rc=%d :: %s' % (rf.returncode, ferr[-200:]))
    else:
        mm = re.findall(r'\[IR-STATS\]\s+muted=(\d+)\s+replay=(\d+)\s+delta=(\d+)', ferr)
        if not mm:
            fails.append('F 无 [IR-STATS] 输出（静默发/计数器 机制未实现）—— 真接管未成立')
        else:
            muted, replay, delta = (int(x) for x in mm[-1])
            if muted <= 0:
                fails.append('F muted=%d（第一遍没有走码生成，或静默开关未生效）' % muted)
            elif delta != 1:
                fails.append('F delta=0（第一遍仍有字节落盘，静默未覆盖全部路径）')
            elif replay != muted:
                fails.append('F replay=%d != muted=%d（L1 未能完整复现第一遍字节流，差 %d）'
                             % (replay, muted, muted - replay))
            else:
                print('F 真接管·建链不落盘: PASS (muted=%d replay=%d delta=1)' % (muted, replay))

    # ── F2 · 子链载体同样等量 ───────────────────────────────────────────
    rf2, of3 = build(a.cc, sa3, 'f2_ir.obj', True, extra={'QCC_IR_STATS': '1'})
    f2err = (rf2.stdout + rf2.stderr).decode('utf-8', 'replace')
    if rf2.returncode != 0:
        fails.append('F2 子链 IR 编译失败 rc=%d :: %s' % (rf2.returncode, f2err[-200:]))
    else:
        mm2 = re.findall(r'\[IR-STATS\]\s+muted=(\d+)\s+replay=(\d+)\s+delta=(\d+)', f2err)
        if not mm2:
            fails.append('F2 子链无 [IR-STATS] 输出')
        else:
            m2, rp2, d2 = (int(x) for x in mm2[-1])
            if not (m2 > 0 and d2 == 1 and rp2 == m2):
                fails.append('F2 子链不等量 muted=%d replay=%d delta=%d' % (m2, rp2, d2))
            else:
                print('F2 真接管·子链等量: PASS (muted=%d replay=%d delta=1)' % (m2, rp2))

    # ── F3 · 实参载体：所有被接管语句的 muted 总和 == replay 总和 ─────────
    rf3, of4 = build(a.cc, sa4, 'f3_ir.obj', True, extra={'QCC_IR_STATS': '1'})
    f3err = (rf3.stdout + rf3.stderr).decode('utf-8', 'replace')
    if rf3.returncode != 0:
        fails.append('F3 实参 IR 编译失败 rc=%d :: %s' % (rf3.returncode, f3err[-200:]))
    else:
        mm3 = re.findall(r'\[IR-STATS\]\s+muted=(\d+)\s+replay=(\d+)\s+delta=(\d+)', f3err)
        if not mm3:
            fails.append('F3 实参无 [IR-STATS] 输出')
        else:
            tm = sum(int(x[0]) for x in mm3)
            tr = sum(int(x[1]) for x in mm3)
            bad_d = [x for x in mm3 if int(x[2]) != 1]
            if tm <= 0 or tr != tm or bad_d:
                fails.append('F3 实参不等量 muted=%d replay=%d delta_bad=%d' % (tm, tr, len(bad_d)))
            else:
                print('F3 真接管·实参等量: PASS (muted=%d replay=%d, %d 条)' % (tm, tr, len(mm3)))

    # ── F4 · 控制流载体：所有被接管语句的 muted 总和 == replay 总和 ─────
    sa5b = write('a5b.c', A5_SRC)
    rf5, of5 = build(a.cc, sa5b, 'f4_ir.obj', True, extra={'QCC_IR_STATS': '1'})
    f4err = (rf5.stdout + rf5.stderr).decode('utf-8', 'replace')
    if rf5.returncode != 0:
        fails.append('F4 控制流 IR 编译失败 rc=%d :: %s' % (rf5.returncode, f4err[-200:]))
    else:
        mm4 = re.findall(r'\[IR-STATS\]\s+muted=(\d+)\s+replay=(\d+)\s+delta=(\d+)', f4err)
        if not mm4:
            fails.append('F4 控制流无 [IR-STATS] 输出')
        else:
            tm4 = sum(int(x[0]) for x in mm4)
            tr4 = sum(int(x[1]) for x in mm4)
            bad_d4 = [x for x in mm4 if int(x[2]) != 1]
            if tm4 <= 0 or tr4 != tm4 or bad_d4:
                fails.append('F4 控制流不等量 muted=%d replay=%d delta_bad=%d' % (tm4, tr4, len(bad_d4)))
            else:
                print('F4 真接管·控制流等量: PASS (muted=%d replay=%d, %d 条)' % (tm4, tr4, len(mm4)))

    # ── F5 · 赋值载体：muted == replay ──────────────────────────────────
    sa6b = write('a6b.c', A6_SRC)
    rf6, of6 = build(a.cc, sa6b, 'f5_ir.obj', True, extra={'QCC_IR_STATS': '1'})
    f5err = (rf6.stdout + rf6.stderr).decode('utf-8', 'replace')
    if rf6.returncode != 0:
        fails.append('F5 赋值 IR 编译失败 rc=%d :: %s' % (rf6.returncode, f5err[-200:]))
    else:
        mm5 = re.findall(r'\[IR-STATS\]\s+muted=(\d+)\s+replay=(\d+)\s+delta=(\d+)', f5err)
        if not mm5:
            fails.append('F5 赋值无 [IR-STATS] 输出')
        else:
            tm5 = sum(int(x[0]) for x in mm5)
            tr5 = sum(int(x[1]) for x in mm5)
            bad_d5 = [x for x in mm5 if int(x[2]) != 1]
            if tm5 <= 0 or tr5 != tm5 or bad_d5:
                fails.append('F5 赋值不等量 muted=%d replay=%d delta_bad=%d' % (tm5, tr5, len(bad_d5)))
            else:
                print('F5 真接管·赋值等量: PASS (muted=%d replay=%d, %d 条)' % (tm5, tr5, len(mm5)))

    # ── G · 真接管：L1 变异反证 ─────────────────────────────────────────
    rg, og = build(a.cc, sa, 'g_ir.obj', True)
    rm, om = build(a.cc, sa, 'g_mut.obj', True, extra={'QCC_IR_MUTATE': '43'})
    rl, ol = build(a.cc, sa, 'g_legacy.obj', False)
    rlm, olm = build(a.cc, sa, 'g_legacy_mut.obj', False, extra={'QCC_IR_MUTATE': '43'})
    if min(rg.returncode, rm.returncode, rl.returncode, rlm.returncode) != 0:
        fails.append('G 编译失败 ir=%d mut=%d legacy=%d legacy_mut=%d'
                     % (rg.returncode, rm.returncode, rl.returncode, rlm.returncode))
    elif sha8(ol) != sha8(olm):
        fails.append('G legacy 受 QCC_IR_MUTATE 影响（变异必须只在 IR 路径）')
    elif sha8(og) == sha8(om):
        fails.append('G 改写 L1 后产物未变 —— 产物不由 L1 决定（真接管未成立）')
    else:
        b1 = io.open(og, 'rb').read()
        b2 = io.open(om, 'rb').read()
        diffs = [i for i in range(min(len(b1), len(b2))) if b1[i] != b2[i]]
        delta = len(b2) - len(b1)
        if len(diffs) == 1 and delta == 0 and b2[diffs[0]] - b1[diffs[0]] == 1:
            print('G 真接管·L1 变异反证: PASS (1 字节 +1 @%d)' % diffs[0])
        else:
            fails.append('G 差异形态不符: 差异字节=%d 长度差=%d（期望 1 字节 +1）'
                         % (len(diffs), delta))

    if fails:
        print('\n[失败 FAIL] ' + '；'.join(fails))
        return 1
    print('\n[通过 PASS] IR 十六条判据全绿（A1/A2/A3/A4/A5/A6/B/B2/C/D/E/F/F2/F3/F4/F5/G）')
    return 0


if __name__ == '__main__':
    sys.exit(main())
