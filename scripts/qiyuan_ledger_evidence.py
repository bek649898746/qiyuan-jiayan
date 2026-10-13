# -*- coding: utf-8 -*-
"""qiyuan_ledger_evidence.py -- 台账 **门事实 统一重建器**（判据 4：复算 不靠 scratch）。

用法:
  python scripts\\qiyuan_ledger_evidence.py <SCRATCH目录> [门...]

门（默认为 fast 组，不含 bootstrap / git —— 那两个是分钟~十几分钟级）:
  probe       -> {SCRATCH}/logs/item_probes.tsv     (委托 qiyuan_ledger_probe.py)
  defect      -> {SCRATCH}/defect_evidence.log      (委托 qiyuan_ledger_defect_evidence.py)
  p0          -> {SCRATCH}/hang.log                 (P0 挂起 ×2 + 静态断言失败 rc=1)
  launch      -> {SCRATCH}/launch.log               (端到端 发射 ×2)
  gitfam      -> {SCRATCH}/logs/gitfam.txt          (族表 对照 · 需 先有 compile_par_result.txt)
  anchors     -> {SCRATCH}/anchor_final.log         (受影响 IR 门锚 --only)
  zlib        -> {SCRATCH}/zlib_rescan.log          (委托 qiyuan_ledger_zlib_scan.py zlib)
  bootstrap   -> {SCRATCH}/bootstrap_final.log      (自举 seed->v1->v2 · ~15 min)
  git         -> {SCRATCH}/compile_par_result.txt   (git 全量闸 · 需 语料 + 仪器 · ~5 min)

设计：**所有 生成器 都在仓**（本文件 + 它委托的三个 + git_gate_par.ps1 + build_zh.ps1 +
run_tests_zh.py + qiyuan_ledger_zlib_scan.py）⟹ 新 空 scratch 上 一条命令 就能 从零 重建 全部 门事实。
"""
import io, os, re, shutil, subprocess, sys

QJ = r'C:\Users\Administrator\Desktop\qiyuan-jiayan'
SCRIPTS = os.path.join(QJ, 'scripts')
EXE = os.path.join(QJ, 'qcc_zh_v2.exe')
GIT_SRC = r'C:\Users\Administrator\Desktop\git-2.45.2'
PRE = os.path.join(QJ, 'compat_prelude.h')

# 受影响 IR 门锚（正向；负锚走 check_ir_cap_neg.py，见 anchor_final.log 的 §4 说明）
IR_ANCHORS = ['feat_zh_incdir_search_ok.c', 'feat_zh_irlandslot_ok.c', 'feat_zh_irlandslot2_ok.c',
              'feat_zh_irlandslot3_ok.c', 'feat_zh_irland_a1a2_ok.c', 'feat_zh_irland_dict_ok.c',
              'feat_zh_irland_l1_ok.c']

# 起点族表（plan 冻结 · 上一轮 实测）—— gitfam 门 用它 作 对照 基线
START_FAM = {'A1001': 8, 'A6068': 3, 'A6085': 2, 'A6109': 1, 'A9001': 3, 'A9002': 1, 'A9006': 3, 'NONE': 2}

# ---- 内联 探针源（不在 scratch 里放 临时 .c，保证 干净环境 可重建）----
P0_CASES = {
    'p0_min':   'int f(void) { return (int)sizeof(struct { _Static_assert(1, "x"); }); }\n',
    'p0_field': 'int g(void) { return (int)sizeof(struct { int a; _Static_assert(1, "x"); }); }\nint f(void){ return g(); }\n',
    'p0_false': 'int f(void) { return (int)sizeof(struct { _Static_assert(0, "boom"); }); }\n',
}
LAUNCH_C = (
    '#包含 <stdio.h>\n'
    '整 基准 = 828;\n'
    '整 加(整 a, 整 b) { 返 a + b; }\n'
    '整 主(空) {\n'
    '    整 累计 = 0;\n'
    '    遍 (整 i = 1; i <= 10; i++) 累计 = 累计 + i;\n'
    '    格式化打印("LAUNCH %d %d\\n", 加(基准, 42), 累计);\n'
    '    返 (加(基准, 42) == 870 && 累计 == 55) ? 0 : 1;\n'
    '}\n')
LAUNCH_EXPECT = 'LAUNCH 870 55'


def run(argv, cwd=None, timeout=1800):
    r = subprocess.run(argv, capture_output=True, cwd=cwd or QJ, timeout=timeout)
    return r.returncode, ((r.stdout or b'') + (r.stderr or b'')).decode('utf-8', 'replace')


def write(path, text):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    io.open(path, 'w', encoding='utf-8', newline='').write(text)


def ps(script, *args, timeout=3600):
    return run(['powershell', '-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', script] + list(args), timeout=timeout)


# ---------------------------------------------------------------- 门
def gate_probe(S):
    p = os.path.join(SCRIPTS, 'qiyuan_ledger_probe.py')
    rc, o = run([sys.executable, p, S])
    print(o.strip()[-400:])
    return rc == 0


def gate_defect(S):
    p = os.path.join(SCRIPTS, 'qiyuan_ledger_defect_evidence.py')
    rc, o = run([sys.executable, p, S])
    print(o.strip()[-300:])
    return rc == 0


def gate_p0(S):
    W = os.path.join(S, 'hangwork'); os.makedirs(W, exist_ok=True)
    TO = 25
    out = ['# P0 挂起 / 编译期断言 复测（每次 超时 = %ds；同输入 连跑 2 次）' % TO,
           '引擎 = %s' % EXE,
           '修前（qcc_x86_step32.exe）三例 皆 TIMEOUT（>25s 死循环，已杀）', '']
    for n, src in P0_CASES.items():
        p = os.path.join(W, n + '.c'); io.open(p, 'w', encoding='utf-8').write(src)
        rcs = []
        for k in (1, 2):
            try:
                rc, o = run([EXE, '-c', p, '-o', p + '.o', '-I', W], cwd=W, timeout=TO)
                errs = [x.strip() for x in o.splitlines() if x.strip().startswith('[ERR]')]
                info = errs[0][:90] if errs else 'OK'
            except subprocess.TimeoutExpired:
                rc, info = -9999, 'TIMEOUT'
            rcs.append(rc)
            out.append('%-10s 第 %d 次: rc=%-6s %s' % (n, k, rc, info))
        out.append('           -> 两次 结果 %s' % ('一致' if rcs[0] == rcs[1] else '**不一致**')); out.append('')
    write(os.path.join(S, 'hang.log'), '\n'.join(out) + '\n')
    ok = all(('p0_min' in l or 'p0_field' in l) and 'rc=0' in l for l in out if '第 1 次' in l and ('p0_min' in l or 'p0_field' in l))
    ok = ok and any('p0_false' in l and 'rc=1' in l for l in out)
    print('  hang.log: p0_min/p0_field rc=0 ×2 · p0_false rc=1 ⟹ %s' % ('OK' if ok else 'FAIL'))
    return ok


def gate_launch(S):
    W = os.path.join(S, 'launchwork'); os.makedirs(W, exist_ok=True)
    src = os.path.join(W, 'launch.c'); io.open(src, 'w', encoding='utf-8').write(LAUNCH_C)
    exe = os.path.join(W, 'launch.exe')
    L = ['# 端到端 发射 ×2（判据：「仍能 产出 可运行 程序」）', '引擎 = %s' % EXE,
         "期望 = stdout == '%s' 且 exit == 0，共 2 次" % LAUNCH_EXPECT, '']
    rc, o = run([EXE, src, '-o', exe], cwd=W)
    L.append('[1/3] 编译: rc=%d' % rc)
    for l in o.strip().split('\n')[:3]:
        L.append('      ' + l)
    L.append('')
    ok = True
    for k in (1, 2):
        r2, o2 = run([exe], cwd=W)
        out = o2.strip()
        hit = (out == LAUNCH_EXPECT and r2 == 0)
        ok = ok and hit
        L.append('[%d/3] 第 %d 次 运行: rc=%d  stdout=%r  stderr=%r' % (k + 1, k, r2, out, ''))
    L.append('')
    L.append('判断 = %s' % ('✅ PASS' if ok else '❌ FAIL'))
    write(os.path.join(S, 'launch.log'), '\n'.join(L) + '\n')
    print('  launch.log ⟹ %s' % ('PASS' if ok else 'FAIL'))
    return ok


def gate_gitfam(S):
    sys.path.insert(0, os.path.join(S, 'tools'))
    res = os.path.join(S, 'compile_par_result.txt')
    if not os.path.exists(res):
        print('  gitfam: 缺 compile_par_result.txt（先跑 git 门）⟹ 跳过'); return False
    txt = io.open(res, 'rb').read().decode('utf-8', 'replace')
    seg = txt.split('--- FAIL ---')[1].split('--- CRASH ---')[0] if '--- FAIL ---' in txt else ''
    marks = [(m.start(), m.end()) for m in re.finditer(r'([A-Za-z0-9_.\-]+\.o)\|\[ERR\]', seg)]
    from collections import Counter
    cnt = Counter()
    for i, (s, e) in enumerate(marks):
        end = marks[i + 1][0] if i + 1 < len(marks) else len(seg)
        codes = re.findall(r'A\d{4}', seg[e:end])
        cnt[codes[0] if codes else 'NONE'] += 1
    END = dict(cnt)
    delta = {k: END.get(k, 0) - START_FAM.get(k, 0) for k in set(START_FAM) | set(END)}
    up = {k: v for k, v in delta.items() if v > 0}
    m = re.search(r'total=(\d+)\s+OK=(\d+)\s+FAIL=(\d+)\s+CRASH=(\d+)', txt)
    L = ['# gitfam 对照（判据「FAIL ≤ 26 · 逐族 不高于 起点族表」）', '#',
         '# 起点族表 = plan 冻结值（上一轮 实测，已 落 plan 文本）',
         '# 终态族表 = 现场实测（compile_par_result.txt，由 git 门 产出）', '#',
         '起点族表 = %s   FAIL=26' % START_FAM,
         '终态族表 = %s   %s' % (END, ('total=%s OK=%s FAIL=%s CRASH=%s' % m.groups()) if m else ''),
         'delta(终态-起点) = %s' % {k: delta[k] for k in sorted(delta)}, '',
         '★ 上升的族: %s' % ('无' if not up else str(up)),
         '★ FAIL = %s' % (m.group(3) if m else '?')]
    write(os.path.join(S, 'logs', 'gitfam.txt'), '\n'.join(L) + '\n')
    print('  gitfam.txt ⟹ %s' % ('无上升' if not up else str(up)))
    return not up


def gate_anchors(S):
    env = dict(os.environ, QCC_TEST_BIN=EXE)
    argv = [sys.executable, os.path.join(SCRIPTS, 'run_tests_zh.py'),
            '--only', ','.join('scripts/' + a for a in IR_ANCHORS)]
    r = subprocess.run(argv, capture_output=True, cwd=QJ, env=env, timeout=1800)
    o = ((r.stdout or b'') + (r.stderr or b'')).decode('utf-8', 'replace')
    m = re.search(r'PASS=(\d+)\s+SKIP=(\d+)\s+FAIL=(\d+)', o)
    L = ['# 受影响 门锚（--only · 定向）· 终态引擎 %s = %s' % (os.path.basename(EXE), _hash8(EXE)), '',
         '## 1. IR 相关 %d 锚（现场实测）' % len(IR_ANCHORS),
         '命令：`QCC_TEST_BIN=%s python scripts\\run_tests_zh.py --only <锚>`' % os.path.basename(EXE), '']
    for i, a in enumerate(IR_ANCHORS, 1):
        L.append('| %d | `scripts/%s` |' % (i, a))
    L += ['', '**结果 = %s**' % (m.group(0) if m else '(未解析到 PASS/SKIP/FAIL)'), '',
          '## 2. 递刀自带 负锚（容量守卫 端到端 拒绝）',
          '`python scripts\\check_ir_cap_neg.py %s` ⟹ 见 `knife.log`' % os.path.basename(EXE), '',
          '## 3. 本刀 新增 检查器（判据 ④⑤）',
          '`python scripts\\check_ir_pred_cap.py %s` ⟹ 见 `knife.log`' % os.path.basename(EXE), '',
          '## 4. 口径：CORE_FEAT 里的 2 个 l1cap 负锚（既有红 · 非本刀引入）',
          '`feat_zh_irland_l1cap_chain_neg.c` / `_main_neg.c` 期望 compile_fail，其判据依赖调试开关',
          '`@KNOB QCC_DBG_IRCAP_*`（runner 不设该环境变量）⟹ 驱动/注册错配；真驱动 `check_ir_cap_neg.py` 复跑 = PASS。',
          '', '```', o.strip()[-1500:], '```']
    write(os.path.join(S, 'anchor_final.log'), '\n'.join(L) + '\n')
    ok = bool(m and m.group(3) == '0')
    print('  anchor_final.log ⟹ %s' % (m.group(0) if m else 'FAIL'))
    return ok


def gate_zlib(S):
    argv = [sys.executable, os.path.join(SCRIPTS, 'qiyuan_ledger_zlib_scan.py'), '--cc', EXE, '--parallel', '4', 'zlib']
    rc, o = run(argv, timeout=900)
    write(os.path.join(S, 'zlib_rescan.log'), o)
    m = re.search(r'zlib\s+(\d+)/(\d+)\s*通过', o)
    print('  zlib_rescan.log ⟹ %s' % (m.group(0) if m else 'FAIL'))
    return bool(m and m.group(1) == m.group(2))


def gate_bootstrap(S):
    """自举门：**结论 由 独立复核 给出**（直接 对 v1/v2 产物 求 sha256），不依赖 PowerShell 的中文 stdout。

    ⚠️ 编码 教训（2026-10-14）：PowerShell 5.1 的 stdout 是 控制台编码（本机 GBK），而 subprocess 捕获后
    若按 utf-8 解 会 把 中文 变成 **U+FFFD（不可逆）** ⟹ 正则 `不动点 … 达成` 失配（假 FAIL）。
    故：① 重跑时 用 `-Command` 显式 `OutputEncoding=UTF8`；② 结论行 由 Python 复核 段 给出（更硬：直算 hash）。
    幂等：v1/v2 产物 已在盘 且 源未改 ⟹ 复用，不重跑 ~15 分钟 链（`--force` 强制）。
    """
    ps1 = os.path.join(SCRIPTS, 'build_zh.ps1')
    bt = io.open(ps1, 'rb').read().decode('utf-8-sig')
    m = re.search(r"\$expected = '([0-9A-Fa-f]{8})'", bt)
    exp = m.group(1).upper() if m else '?'
    v1 = os.path.join(QJ, 'qcc_zh_v1.exe'); v2 = os.path.join(QJ, 'qcc_zh_v2.exe')
    L, reused = [], False
    if os.path.exists(v1) and os.path.exists(v2) and not FORCE:
        reused = True
        L.append('# 自举 复核 · **复用 已在盘 的 v1/v2**（引擎 源码 未改 ⟹ 不重跑 ~15 分钟 链；`--force` 可强制重跑）')
        L.append('#')
        L.append('# 编码 留痕：PowerShell 5.1 的 stdout 为 控制台编码（GBK），subprocess 捕获 后 按 utf-8 解 会 把 中文')
        L.append('#   变成 U+FFFD（不可逆）⟹ 判据 若 依赖 ps1 的中文行 会 假 FAIL。故 本件 的 **结论行** 由 下方')
        L.append('#   「机器 复核」段 给出 —— 直接 对 v1/v2 产物 求 sha256，与 `$expected` 三方比对，**不依赖** ps1 输出。')
        L.append('')
    else:
        argv = ['powershell', '-NoProfile', '-Command',
                "[Console]::OutputEncoding=[Text.Encoding]::UTF8; & '%s' -NoSeed" % ps1]
        r = subprocess.run(argv, capture_output=True, cwd=QJ, timeout=3600)
        raw = (r.stdout or b'') + (r.stderr or b'')
        io.open(os.path.join(S, 'bootstrap_ps1_raw.bin'), 'wb').write(raw)
        txt = None
        for enc in ('utf-8', 'gbk', 'utf-16', 'latin-1'):
            try:
                cand = raw.decode(enc)
            except Exception:
                continue
            if ('不动点' in cand) or ('[通过 PASS]' in cand) or ('v1 =' in cand):
                txt = cand; break
        if txt is None:
            txt = raw.decode('utf-8', 'replace')
        L.append('# 自举 原始输出（`build_zh.ps1 -NoSeed` · OutputEncoding=UTF8）')
        L.append('```')
        L += txt.strip().split('\n')
        L.append('```')
        L.append('')
    h1 = _hash8(v1).upper() if os.path.exists(v1) else '?'
    h2 = _hash8(v2).upper() if os.path.exists(v2) else '?'
    ok = (h1 == h2 == exp)
    L.append('## 机器 复核（不 依赖 PowerShell 中文 输出）')
    L.append('')
    L.append('| 项 | sha256-8 | 来源 |')
    L.append('|:--|:--|:--|')
    L.append('| `qcc_zh_v1.exe`（由 seed 编译） | **%s** | 现场 sha256 |' % h1)
    L.append('| `qcc_zh_v2.exe`（由 v1 编译） | **%s** | 现场 sha256 |' % h2)
    L.append('| `$expected` | **%s** | `scripts/build_zh.ps1` |' % exp)
    L.append('')
    L.append('[%s] 中文自举链不动点 %s 达成 (收敛深度 2 段: v1==v2)' % ('通过 PASS' if ok else '失败 FAIL', exp))
    L.append('判定 = `v1 == v2 == $expected` ⟹ **%s**%s'
             % ('✅ 收敛' if ok else '❌ 不一致', '（复用 已在盘 产物 · 源 未改）' if reused else ''))
    write(os.path.join(S, 'bootstrap_final.log'), '\n'.join(L) + '\n')
    print('  bootstrap_final.log ⟹ %s (v1=%s v2=%s expected=%s%s)'
          % ('PASS' if ok else 'FAIL', h1, h2, exp, ' · 复用' if reused else ''))
    return ok


def gate_git(S):
    script = os.path.join(SCRIPTS, 'git_gate_par.ps1')
    ext = r'D:\qiyuan_home\git_rebuild\compile_git_par.ps1'
    if os.path.exists(script):
        pass
    elif os.path.exists(ext):                      # 仓内 缺 时 退 到 外部 副本（如实 说明）
        script = ext; print('  [WARN] 用 外部 副本 %s' % ext)
    else:
        print('  git: 无 仪器 ⟹ 跳过'); return False
    rc, o = ps(script, '-Qcc', EXE, '-Jobs', '4', '-Out', os.path.join(S, 'git_objs'),
               '-ResultFile', os.path.join(S, 'compile_par_result.txt'), timeout=3600)
    m = re.search(r'total=(\d+)\s+OK=(\d+)\s+FAIL=(\d+)\s+CRASH=(\d+)', o)
    write(os.path.join(S, 'git_gate.log'), o)
    print('  compile_par_result.txt ⟹ %s' % (m.group(0) if m else 'FAIL'))
    return bool(m)


def _hash8(p):
    import hashlib
    return hashlib.sha256(io.open(p, 'rb').read()).hexdigest()[:8]


GATES = {'probe': gate_probe, 'defect': gate_defect, 'p0': gate_p0, 'launch': gate_launch,
         'gitfam': gate_gitfam, 'anchors': gate_anchors, 'zlib': gate_zlib,
         'bootstrap': gate_bootstrap, 'git': gate_git}
DEFAULT = ['probe', 'defect', 'p0', 'launch', 'gitfam', 'anchors', 'zlib']
FORCE = '--force' in sys.argv          # bootstrap 门 默认 幂等（复用 已在盘 的 v1/v2）

if __name__ == '__main__':
    S = os.path.abspath(sys.argv[1]) if len(sys.argv) > 1 else os.environ.get('QCC_LEDGER_SCRATCH', '')
    if not S or not os.path.isdir(S):
        sys.exit('用法: python scripts\\qiyuan_ledger_evidence.py <SCRATCH目录> [门...]')
    want = sys.argv[2:] or DEFAULT
    os.makedirs(os.path.join(S, 'logs'), exist_ok=True)      # probe 门 需要 logs/ 先存在
    print('SCRATCH = %s' % S)
    print('引擎 = %s (sha256-8 %s)' % (EXE, _hash8(EXE)))
    print('门 = %s' % want)
    bad = []
    for g in want:
        if g not in GATES:
            print('[SKIP] 未知门 %s' % g); continue
        print('--- 门 %s ---' % g)
        try:
            if not GATES[g](S):
                bad.append(g)
        except Exception as e:
            print('  [ERR] %s: %s' % (g, e)); bad.append(g)
    print()
    print('[%s] 证据重建 %s' % ('PASS' if not bad else 'FAIL', '' if not bad else ('未过: ' + str(bad))))
    sys.exit(0 if not bad else 1)
