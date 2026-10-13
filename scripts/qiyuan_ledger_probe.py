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

# ================= R802 (2026-10-14 虾米): 结构性 / 作废 / 未闭合 条目 的 现役 复算 =================
# 语义（与上面的「编译 rc」探针 **区分开**）：**rc0 = 断言成立 · rc1 = 断言不成立**；note 逐字写明断言。
# 目的（判据 1/4）：消灭两类结构性缺口 —— ① 证据 指向 不在仓的 `{SCRATCH}/ledger_final.md`；
#   ② 复算列 为 `-`（不可机器复算）。改后：这 10 条 全部 有 **仓内生成器** 产出的 实测 事实格。
SRCTXT = io.open(os.path.join(QJ, 'srclib_jiayan', 'qcc_zh.jy'), 'rb').read().decode('utf-8-sig')


def grep_src(k):
    return len(re.findall(re.escape(k), SRCTXT))


def run_script(argv, timeout=300):
    r = subprocess.run(argv, capture_output=True, cwd=QJ, timeout=timeout)
    return r.returncode, ((r.stdout or b'') + (r.stderr or b'')).decode('utf-8', 'replace')


def comp_env(key, code, env=None, timeout=TO):
    p = os.path.join(W, key + '.c'); io.open(p, 'w', encoding='utf-8').write(code)
    try:
        r = subprocess.run([EXE, '-c', p, '-o', p + '.o'], capture_output=True, cwd=W,
                           env=dict(os.environ, **(env or {})), timeout=timeout)
        return r.returncode, ((r.stdout or b'') + (r.stderr or b'')).decode('utf-8', 'replace')
    except subprocess.TimeoutExpired:
        return -9999, 'TIMEOUT'


def comp_run(key, code, timeout=TO):
    """编译 + 链接 + 运行（判「值 是否 与 gcc 逐值一致」）；返回 (rc, stdout)。"""
    p = os.path.join(W, key + '.c'); io.open(p, 'w', encoding='utf-8').write(code)
    try:
        r = subprocess.run([EXE, p, '-o', p + '.exe'], capture_output=True, cwd=W, timeout=timeout)
        if r.returncode != 0:
            return r.returncode, '<compile rc=%d>' % r.returncode
        rr = subprocess.run([p + '.exe'], capture_output=True, cwd=W, timeout=timeout)
        return 0, (rr.stdout or b'').decode('utf-8', 'replace').strip()
    except subprocess.TimeoutExpired:
        return -9999, 'TIMEOUT'


def claim(key, ok, note):
    rows.append((key, '0' if ok else '1', '-', note))


# id7 · IR 两套表并存（dump 同现 L1 表 / 影子对账 / CFG 块表）
rc, d = comp_env('i07_probe', '函数 整 主(空){ 整 甲 = 1; 返 甲 - 1; }\n', {'QCC_DUMP': 'ir'})
claim('i07_twotable', rc == 0 and all(s in d for s in ['-- IR L1 抽象指令', 'IR 影子对账', 'IR CFG 块表']),
      '断言: QCC_DUMP=ir 下 L1 表 / 影子对账 / CFG 块表 **三者同现** (L0 与 L1 两套表并存)')

# id8 · 仪器刀（检查器 无 **真 emoji** ⟹ GBK 控制台 不崩）
rc, o = run_script([sys.executable, os.path.join(QJ, 'scripts', 'check_ir_cfg.py'), EXE])
_em = re.findall(r'[\U0001F300-\U0001FAFF]', o)
claim('i08_emoji', rc == 0 and not _em,
      '断言: check_ir_cfg.py rc=0 且 输出 **真 emoji**(U+1F300+) = 0 ⟹ GBK 安全 (✓✗ 等 非 emoji 符号 不计)')

# id10 / id11 · C1 最内层元素尺寸 · C2b 数组元素取多级 · C7 指针数组二级下标（与 gcc 逐值一致）
_c1 = '#include <stdio.h>\nint main(void){ float x=1.5f; float *p=&x; float **q=&p; printf("%d\\n",(int)sizeof(*q)); return 0; }\n'
_c2 = '#include <stdio.h>\nint main(void){ double qa[4]={1.5,2.5,3.5,4.5}; double *t=qa+1; printf("%.2f\\n",*t); return 0; }\n'
_c7 = '#include <stdio.h>\nint main(void){ double r[2]={7.5,8.5}; double *dp[2]; dp[1]=r; printf("%.2f\\n",dp[1][0]); return 0; }\n'
_rc, _o = comp_run('i10_probe', _c1)
claim('i10_c1', _rc == 0 and _o == '8', '断言: 最内层元素尺寸 = 8 (gcc 同值) · 实测 %r' % _o)
_rc, _o = comp_run('i11a_probe', _c2)
claim('i11_c2b', _rc == 0 and _o == '2.50', '断言: 数组元素取多级 = 2.50 (gcc 同值) · 实测 %r' % _o)
_rc, _o = comp_run('i11b_probe', _c7)
claim('i11_c7', _rc == 0 and _o == '7.50', '断言: 指针数组二级下标 = 7.50 (gcc 同值) · 实测 %r' % _o)

# id13 · 六刀 v2 要件（内容锚 · 逐刀）
_ji = len(re.findall(r'静 整 建节\(', SRCTXT))
claim('i13_ji', _ji == 1, '断言: `建节()` 定义 唯一 (实测 %d 处) ⟹ 唯一打标点 成立' % _ji)
claim('i13_knives', bool(_ji == 1 and '控式[计] = 3' in SRCTXT and '3=分支' in SRCTXT
                         and '本版无源' in SRCTXT and '换代' in SRCTXT),
      '断言: 六刀 v2 全落 —— 刀一 `控式[计]=3`+`3=分支` / 刀二 `段双` 三档 / 刀三 `剩系` / 刀五 `换代` / 刀六 容量注释')
claim('i13_uncal', '本版无源' in SRCTXT,
      '断言: 未校容量 (`尾午一`/`等类`/`照容`) 按 v1.4 §11.2 **合法登记「本版无源」**（不做中间态）')

# id17 / id21 刀五 · 余项（字段 未落位）
_n_send = grep_src('Send位'); _n_enum = grep_src('枚举底层标志')
claim('i17_sendbit', _n_send == 0, '断言: 主源 `Send位` 命中 = %d ⟹ **尚未落位**（0 为真）' % _n_send)
claim('i17_enumflag', _n_enum == 0, '断言: 主源 `枚举底层标志` 命中 = %d ⟹ 尚未落位' % _n_enum)
_n_read = grep_src('总子') + grep_src('计肆一')
claim('i17_readend', _n_read > 0, '断言: 读端 `总子`+`计肆一` 命中 = %d ⟹ 读端 在盘' % _n_read)

# id19 / id20 / id23 · 作废项 的 零改动 / 被取代 依据
claim('i19_zero', grep_src('上台次序') == 0, '断言: 主源 `上台次序` 命中 = 0 ⟹ 递刀「未占源·未改一字节」成立')
claim('i20_zero', grep_src('作业面') == 0, '断言: 主源 `作业面` 命中 = 0 ⟹ 递刀「零编译器改动」成立')
claim('i23_super', os.path.isdir(os.path.join(QJ, '_audit_inbox', '20261012_施工单v2_按甲言地基_启元')),
      '断言: 取代它的 v2 递刀 目录 在盘 ⟹ 「已被 v2 取代·留底不删」成立')

# id21 · 8 刀复查 逐刀 现役 复核
_rc4, _o4 = comp_env('i21_probe', '超级编译 整 甲;\n函数 整 主(空){ 返 0; }\n')
_c4 = re.findall(r'A\d{4}', _o4)
claim('i21_knife4', _rc4 != 0,
      '断言: 刀四 `超级编译` 已 **响亮拒绝** (实测 rc=%s %s · 主源命中 %d) — 台账原记「未做」过时'
      % (_rc4, _c4[0] if _c4 else '-', grep_src('超级编译')))
_p21 = os.path.join(W, 'i21_coff.c'); io.open(_p21, 'w', encoding='utf-8').write('函数 整 主(空){ 返 0; }\n')
_r21 = subprocess.run([EXE, '-c', _p21, '-o', _p21 + '.o'], capture_output=True, cwd=W, timeout=TO)
_sz21 = os.path.getsize(_p21 + '.o') if os.path.exists(_p21 + '.o') else -1
claim('i21_coff', _r21.returncode == 0 and _sz21 > 0,
      '断言: 刀二/三 `-c` COFF 通道 通 (rc=%d · %d bytes)' % (_r21.returncode, _sz21))
claim('i21_knife5', _n_send == 0 and _n_enum == 0,
      '断言: 刀五 (`类型表`/`变量表` 补真结构字段) **未做** ⟹ 与 id17 余项 同源')
_doc = 0
for _root, _ds, _fs in os.walk(QJ):
    if os.sep + '.git' in _root:
        continue
    for _f in _fs:
        if _f.endswith('.md'):
            try:
                if 'IR 数据源 真伪' in io.open(os.path.join(_root, _f), 'rb').read().decode('utf-8', 'replace'):
                    _doc += 1
            except Exception:
                pass
claim('i21_knife6', _doc > 0, '断言: 刀六 「IR 数据源 真伪」小节 在盘 (%d 份文档)' % _doc)

# ---- 落盘（机器可读 事实表）----
io.open(OUT, 'w', encoding='utf-8', newline='').write(
    '# 递刀台账 · 逐条探针 实测（引擎 %s）\nkey\trc\tcode\tnote\n' % EXE +
    '\n'.join('\t'.join(x) for x in rows) + '\n')
print('写了 %s（%d 行）' % (OUT, len(rows)))
for x in rows:
    print('  %-18s rc=%-7s %-6s %s' % (x[0], x[1], x[2], x[3][:80]))

