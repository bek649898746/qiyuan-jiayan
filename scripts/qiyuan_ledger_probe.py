# -*- coding: utf-8 -*-
"""qiyuan_ledger_probe.py -- 递刀台账 · 逐条探针（**单一事实源** 的 事实产生器）。

作用：用 **现役终态引擎** 对 台账 里 每一条「已解决/部分」所 引用的 最小复现 跑一遍，
产出 一行 一格 的 **机器可读** 事实表 `{SCRATCH}/logs/item_probes.tsv`（列：key rc code note）。

设计 纪律：
  * **不硬编码 期望值** —— 本脚本 只 采集 实测；「期望」写 在 `qiyuan_ledger_facts.tsv` 里，
    由 `qiyuan_ledger_check.py` 逐格 对账（实测 ≠ 台账 ⟹ FAIL）。
  * 探针 一律 带 超时；同一时刻 只 跑 一个。
  * P1 探针表 直接 调 `r799_verify.py`（不 重实现）。
"""
import io, os, re, subprocess, sys

QJ = r'C:\Users\Administrator\Desktop\qiyuan-jiayan'
# {SCRATCH} 由 调用方 传入（argv[1] 或 环境变量）—— 仓内 不 持久化 scratch 引用
S = sys.argv[1] if len(sys.argv) > 1 else os.environ.get('QCC_LEDGER_SCRATCH')
if not S or not os.path.isdir(S):
    sys.exit('用法: python scripts\\qiyuan_ledger_probe.py <SCRATCH目录>  (或 设 QCC_LEDGER_SCRATCH)')
S = os.path.abspath(S)
EXE = os.path.join(QJ, 'qcc_zh_v2.exe')
assert os.path.exists(EXE), '终态引擎 缺失: %s' % EXE
W = os.path.join(S, 'itemwork')
os.makedirs(W, exist_ok=True)
OUT = os.path.join(S, 'logs', 'item_probes.tsv')
TO = 25

def run_one(key, src, cwd=None, extra=None, srcpath=None):
    if srcpath:
        p = srcpath
    else:
        p = os.path.join(W, key + '.c')
        io.open(p, 'w', encoding='utf-8').write(src)
    cmd = [EXE, '-c', p, '-o', p + '.o', '-I', W] + (extra or [])
    try:
        r = subprocess.run(cmd, capture_output=True, cwd=cwd or W, timeout=TO)
        rc, raw = r.returncode, (r.stdout or b'') + (r.stderr or b'')
    except subprocess.TimeoutExpired:
        rc, raw = -9999, b'TIMEOUT'
    s = raw.decode('utf-8', 'replace')
    errs = [x.strip() for x in s.splitlines() if x.strip().startswith('[ERR]')]
    # code 只 取 **首条 [ERR] 行** 里 的 首个 A 码（输出里 其它 位置 提到 A 码 不算 —— 如 引导文案 会 复述）
    codes = re.findall(r'A\d{4}', errs[0]) if errs else []
    return (key, str(rc), (codes[0] if codes else '-'),
            (errs[0][:110] if errs else 'OK').replace('\t', ' '))

rows = []
D = '函数 整 主(空){ 返 0; }'
# ---- 逐条 探针（来源 = 各 递刀 最小复现 / 台账 引用 的 真载体）----
rows.append(run_one('i01_kw_assign', '函数 整 主(空){ 跳 = 1; 返 0; }\n'))
rows.append(run_one('i01b_ctrl',    '函数 整 主(空){ 整 要跳 = 1; 返 要跳 - 1; }\n'))
rows.append(run_one('i02_undecl',    '函数 整 主(空){ zzz = 1; 返 0; }\n'))
rows.append(run_one('i03_fwd',       '函数 整 主(空){ 全局乙 = 5; 返 0; }\n整 全局乙;\n'))
rows.append(run_one('i04_twparam',   '函数 整 探甲(字 *前, 字 *短){ 返 0; }\n' + D))
rows.append(run_one('i06a_strlen',   '函数 整 主(空){ 整 甲[8]; 返 串长(&甲[0]); }\n'))
rows.append(run_one('i06b_len',      '函数 整 主(空){ 整 甲[8]; 返 长度(甲); }\n'))
rows.append(run_one('i12_kwvar',     '静 整 变量[16];\n函数 整 主(空){ 变量[0] = 7; 返 变量[0]; }\n'))
rows.append(run_one('i12b_auto_ok',  '函数 整 主(空){ 自动 x = 5; 返 x - 5; }\n'))
rows.append(run_one('i12c_auto_kw',  '函数 整 主(空){ 自动 跳 = 5; 返 0; }\n'))
rows.append(run_one('i12d_auto_ni',  '函数 整 主(空){ 自动 x; 返 0; }\n'))
rows.append(run_one('i14_p0',        'int f(void) { return (int)sizeof(struct { _Static_assert(1, "x"); }); }\n'))
# 真载体（递刀 点名 的 路径）
VLA = r'D:\qiyuan_home\diff_out\t1101004_vla.c'
rows.append(run_one('i17_vla', '', srcpath=VLA) if os.path.exists(VLA) else ('i17_vla', 'MISSING', '-', '真载体 不存在: %s' % VLA))
Z = r'D:\qiyuan_home\corpus\zlib-1.3.1'
MIN = os.path.join(QJ, r'_audit_inbox\20261012_施工单_A9001根因_函数体内include_启元\evidence\final_min.c')
rows.append(run_one('i22_min', '', srcpath=MIN, extra=['-I', Z]) if os.path.exists(MIN) else ('i22_min', 'MISSING', '-', MIN))
for nm, f in (('i22_infback', 'infback.c'), ('i22_inflate', 'inflate.c')):
    p = os.path.join(Z, f)
    rows.append(run_one(nm, '', srcpath=p, extra=['-I', Z]) if os.path.exists(p) else (nm, 'MISSING', '-', p))

# ---- P1 探针表（**自足**：本脚本 内 直接 跑，不 依赖 任何 易失 工具）+ P0 三例 ----
H = '#define UH(a,b,c) 0\n'
P1 = {
    'em1': H + 'void g(void) {\n#if 0\n    Call UH() x\n#endif\n}\n',
    'de2': H + 'void g(void) {\n#if 0\n    UH() ;\n#endif\n}\n',
    'de1': H + 'void g(void) {\n#if 3 != 3\n    UH() ;\n#endif\n}\n',
    'em4': H + 'void g(void) {\n#ifdef NOTDEF\n    UH() ;\n#endif\n}\n',
    'em8': H + 'void g(void) {\n#if 0\nint zz = UH(1);\n#endif\n}\n',
    'em2': H + 'void g(void) {\n#if 0\n    Call plainname() x\n#endif\n}\n',
    'em7': H + 'void g(void) {\n#if 0\n    if (1) { }\n#endif\n}\n',
    'de5': H + 'void g(void) {\n#if 1\n    UH() ;\n#endif\n}\n',
    'de4': H + 'void g(void) {\n#if 1\n    int zz = UH(1,2,3);\n#endif\n}\n',
}
for k, src in P1.items():
    rows.append(run_one('i16_' + k, src))
rows.append(run_one('i14_hang_a', 'int f(void) { return (int)sizeof(struct { _Static_assert(1, "x"); }); }\n'))
rows.append(run_one('i14_hang_b', 'int g(void) { return (int)sizeof(struct { int a; _Static_assert(1, "x"); }); }\nint f(void){ return g(); }\n'))
rows.append(run_one('i14_loud_c', 'int f(void) { return (int)sizeof(struct { _Static_assert(0, "boom"); }); }\n'))

# ---- 落盘（机器可读 事实表）----
io.open(OUT, 'w', encoding='utf-8', newline='').write(
    '# 递刀台账 · 逐条探针 实测（引擎 %s）\nkey\trc\tcode\tnote\n' % EXE +
    '\n'.join('\t'.join(x) for x in rows) + '\n')
print('写了 %s（%d 行）' % (OUT, len(rows)))
for x in rows:
    print('  %-18s rc=%-7s %-6s %s' % (x[0], x[1], x[2], x[3][:80]))

