# -*- coding: utf-8 -*-
"""check_ir_real_edges.py -- **控制流 真边** 判据（plan 验收判据 1 · 本 goal 新增）。

判据（**全部**须成立，缺一即 FAIL）:
  ① **非落空边 > 0** —— 控制流 是「图」不是「纯链」（dump 的 `非落空边(goto 真边) = N`）
  ② **条件块**（块首 `控式==2`）**后继数 == 2** —— 真/假 两路
  ③ **循环块**（`回边==1`）的 **回边目标 == 该块 `始`** —— 回边 指回 块首
  ④ **`断`/`继续` 目标**：含 `断`/`继续` 节点的块 必须 给出目标，或 **显式登记「待回填」**（不许静默缺）

判据 ④ 的口径（plan 风险条 逐字）: 「已回填的边必须正确 · 未回填的必须显式登记」——
  dump 里 每块 打 `断目标=`/`继续目标=`（-1 = 未回填），**未回填 必须** 在 dump 的
  `IR 待回填 登记` 段 里 逐块 出现（否则 FAIL）⟹ 不用「后继数>=1」这类恒真式糊过去。

用法:
  python scripts\\check_ir_real_edges.py [引擎.exe] [源名(相对 探针目录)] [--dump-file <dump.txt>]
  python scripts\\check_ir_real_edges.py --selftest        # 内置 **负向测试**（判据 能失败）
"""
import io, os, re, subprocess, sys

sys.stdout.reconfigure(encoding='utf-8', errors='replace')
QJ = r'C:\Users\Administrator\Desktop\qiyuan-jiayan'
T = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'probe')  # R821e: 探针 随仓走（原 只读 %TEMP%/qiyuan-ir-knives ⇹ 目录被清理后 假红）
BLK = re.compile(r'块\[(\d+)\] 始=(-?\d+) 终一=(-?\d+) 后继数=(\d+) 后继0=(-?\d+) 回边=(\d+) 首控式=(\d+)'
                 r'(?: 前驱数=(\d+) 前驱0=(-?\d+))?'
                 r'(?: 后继1=(-?\d+) 断目标=(-?\d+) 继续目标=(-?\d+))?')
# ⚠️ 字段序 必须与 qcc_zh.jy 的 块行 **逐字一致**：引擎 打
#    `后继1=%d 断目标=%d 继续目标=%d`（**无** `回边目标` —— 回边 改由 独立 `IR CFG 回边表` 段 承载）。
#    首版 正则 误把 `回边目标` 写进 可选组 ⟹ 真实 dump 上 整组 失配 ⟹ succ1/brk/cont **全 None**
#    ⟹ 判据④ **从不评估** 任何 断/继续 目标 = 恒过（skeptic 以 「brk/cont 非 None 的块 = 0」 为证）。


def parse(dump):
    """→ (块列表 dict, 段落 值 dict)"""
    blks = []
    for m in BLK.finditer(dump):
        g = m.groups()
        blks.append(dict(n=int(g[0]), start=int(g[1]), end=int(g[2]), nsucc=int(g[3]),
                         succ0=int(g[4]), backed=int(g[5]), ctrl=int(g[6]),
                         npred=(int(g[7]) if g[7] is not None else None),
                         succ1=(int(g[9]) if g[9] is not None else None),
                         brk=(int(g[10]) if g[10] is not None else None),
                         cont=(int(g[11]) if g[11] is not None else None)))
    seg = {}
    m = re.search(r'非落空边\(goto 真边\) = (\d+)', dump)
    seg['nonfall'] = int(m.group(1)) if m else None
    m = re.search(r'IR CFG 块表 .*?上退=(\d+)', dump)
    seg['nblk'] = int(m.group(1)) if m else None
    # 待回填 登记：**项级**（块号, 项）—— 段标题 与 块号 在 不同行 ⟹ 全 dump 搜；
    #   ⚠️ 粒度 = (块, 项)：块级集合 会 让「该块登记了 条件假路」**救回**「该块 继续 待回填」（自测 破坏④ 抓出）
    #   ⚠️ 措辞 必须与 引擎 dump **逐字对齐**：引擎 打 `块[N] 断目标 待回填` / `块[N] 继续目标 待回填`
    #      （**"断目标" 连写**）与 `块[N] 条件假路 目标 待回填`（**有空格**）—— 两种 形态 都要 收。
    seg['pending'] = set((int(a), ('条件假路' if b.startswith('条件假路') else b.rstrip('目标')))
                         for a, b in re.findall(r'块\[(\d+)\] (断目标|继续目标|条件假路 目标) 待回填', dump))
    seg['has_pending_sec'] = ('IR 待回填 登记' in dump)
    # 回边表: `回边 块[u]→块[v]`（u = 源块 · v = 目标块）
    seg['back_edges'] = set((int(a), int(b)) for a, b in re.findall(r'回边 块\[(\d+)\]→块\[(\d+)\]', dump))
    seg['has_back_sec'] = ('IR CFG 回边表' in dump)
    return blks, seg


def judge(dump):
    """→ (fails:list, report:list)"""
    blks, seg = parse(dump)
    bad, rep = [], []
    if not blks:
        return ['无 块表 行（dump 缺 CFG 段）'], ['✗ 块表 行 = 0']

    # ① 非落空边 > 0
    if seg['nonfall'] is None:
        bad.append('① 非落空边 段 缺失（dump 未打 `非落空边(goto 真边) = N`）')
        rep.append('① 非落空边 = **段缺失** ❌')
    elif seg['nonfall'] <= 0:
        bad.append('① 非落空边 = %d（须 > 0）' % seg['nonfall'])
        rep.append('① 非落空边 = %d ❌（须 > 0）' % seg['nonfall'])
    else:
        rep.append('① 非落空边 = %d ✓' % seg['nonfall'])

    # ② 条件块 后继数 == 2 —— **或** 后继数 == 1 且 该块 在 「待回填 登记」段里 显式登记
    #    （plan 风险条 逐字：「若当前阶段拿不到该上下文，必须以逐字依据登记为『待回填』而不是猜值，
    #      并同步把判据收紧成『已回填的边必须正确 · 未回填的必须显式登记』——不得用『后继数 >= 1』这类恒真式糊过去」）
    #    ⟹ 本判据 **不是恒真**：后继数 == 1 且 **未登记** ⟹ FAIL（见 selftest 破坏②）。
    cond = [b for b in blks if b['ctrl'] == 2]
    if not cond:
        bad.append('② 无 首控式==2 的块（探针 含 条件 ⟹ 疑 dump 段缺）')
        rep.append('② 条件块 = 0 ❌')
    else:
        # ②a：`后继数==2` **且 后继1 必须 严格前向（> 块号）且 ≠ 后继0** —— 否则 必须 显式登记。
        #   ⚠️ 首版 只查 `==2` ⟹ **11 个 伪自环**（`后继1 == 块号`）被 当 合规 ⟹ 「62 块真连」**虚高**
        #      （skeptic 实测：`块[156] … 后继数=2 后继0=157 … 后继1=156`，亦 157/178/369）。
        def ok2(b):
            if b['nsucc'] != 2:
                return False
            s1 = b['succ1']
            return (s1 is not None and s1 > b['n'] and s1 != b['succ0'])
        reg2 = [b for b in cond if (b['n'], '条件假路') in seg['pending']]
        bad2 = [b for b in cond if not ok2(b) and (b['n'], '条件假路') not in seg['pending']]
        # ②b：**全局** —— 非 `控式==1` 的块 不得 有 **伪自环**（`后继1 == 自身`）
        bad2c = [b for b in blks if b['succ1'] is not None and b['succ1'] == b['n'] and b['ctrl'] != 1]
        if bad2:
            b = bad2[0]
            bad.append('② 条件块：%d 个 既非「后继数==2 且 后继1 严格前向且≠后继0」也**未登记**'
                       '（例 块[%d] 后继数=%d 后继0=%s 后继1=%s）'
                       % (len(bad2), b['n'], b['nsucc'], b['succ0'], b['succ1']))
        if bad2c:
            bad.append('②b **伪自环**：%d 个 非循环块 的 `后继1 == 自身`（例 块[%d]）'
                       % (len(bad2c), bad2c[0]['n']))
        if not (bad2 or bad2c):
            real2 = [b for b in cond if ok2(b)]
            rep.append('② 条件块（%d）：**%d 块 有 真两路**（后继1 严格前向且≠后继0）· %d 块 显式登记 · **伪自环 0** ✓'
                       % (len(cond), len(real2), len(reg2)))

    # ③ 回边 —— **三层**（skeptic 指出 首版 ① 是 **构造性恒真** ⟹ 补 ②③ 使其 有 判别力）:
    #   ① 每个 `控式==1`（循环头）块 必须 有回边指回 —— **构造性**（引擎 注入自环 满足；如实标注）
    #   ② **非 `控式==1` 的块 不得有自环** —— 引擎 若 把 自环 错注入 到 普通块 ⟹ FAIL
    #   ③ **回边表 每条 `u→v` 必须是 真实 CFG 边**（`v ∈ 块[u]` 的 后继集）—— 假表/错表 ⟹ FAIL
    loop = [b for b in blks if b['ctrl'] == 1]
    succ_of = {}
    for b in blks:
        s = []
        if b['nsucc'] > 0 and b['succ0'] is not None and b['succ0'] >= 0:
            s.append(b['succ0'])
        if b['succ1'] is not None and b['succ1'] >= 0:
            s.append(b['succ1'])
        if b['ctrl'] == 1:
            s.append(b['n'])            # 引擎 注入的 循环头 自环（真边 ⟹ 也进 后继集）
        succ_of[b['n']] = s
    ctrl1 = set(b['n'] for b in loop)
    if not loop:
        bad.append('③ 无 首控式==1 的块（探针 含 循环 ⟹ 疑 dump 段缺）')
        rep.append('③ 循环块 = 0 ❌')
    elif not seg['has_back_sec']:
        bad.append('③ dump **无** `IR CFG 回边表` 段 ⟹ 回边 不可判（%d 个 循环头）' % len(loop))
        rep.append('③ 回边表 **段缺失** ❌')
    else:
        be = seg['back_edges']
        bad3a = [h for h in loop if not any(v == h['n'] for (u, v) in be)]
        bad3b = [(u, v) for (u, v) in be if u == v and u not in ctrl1]
        bad3c = [(u, v) for (u, v) in be if v not in succ_of.get(u, [])]
        if bad3a:
            h = bad3a[0]
            bad.append('③① 循环头 无回边指回：%d/%d（例 块[%d] 始=%d）' % (len(bad3a), len(loop), h['n'], h['start']))
        if bad3b:
            bad.append('③② **非循环块** 出现自环：%d 处（例 块[%d]→块[%d]）'
                       % (len(bad3b), bad3b[0][0], bad3b[0][1]))
        if bad3c:
            bad.append('③③ 回边表 有 **非真实边**：%d 处（例 块[%d]→块[%d]，后者 不在 前者 后继集 %s）'
                       % (len(bad3c), bad3c[0][0], bad3c[0][1], succ_of.get(bad3c[0][0])))
        if not (bad3a or bad3b or bad3c):
            rep.append('③ 回边 三层 ✓：循环头（%d）均有回边指回〔**构造性**〕· 非循环块 无自环 · 回边表 %d 条**全为真实边**'
                       % (len(loop), len(be)))

    # ④ 断/继续 目标：已回填 或 显式登记
    #    语义（避免 歧义）: `-1` = 该块 **无** 该节点（跳过）· `-2` = 有该节点 但 **未回填**（须登记）· `>=0` = 已回填
    need = []
    for b in blks:
        if b['brk'] is None and b['cont'] is None:
            continue
        for key, nm in (('brk', '断'), ('cont', '继续')):
            v = b[key]
            if v is None:
                continue
            if v >= 0:
                if not (0 <= v < len(blks)):
                    bad.append('④ 块[%d] %s目标=%d 越界' % (b['n'], nm, v))
            elif v == -2 and (b['n'], nm) not in seg['pending']:
                need.append((b['n'], nm))
    if not seg['has_pending_sec'] and need:
        bad.append('④ 有 未回填 的 断/继续 目标 但 dump **无** `IR 待回填 登记` 段（%d 处，例 块[%d] %s）'
                   % (len(need), need[0][0], need[0][1]))
        rep.append('④ 断/继续 未回填 %d 处 且 **无登记段** ❌' % len(need))
    elif need:
        bad.append('④ 断/继续 未回填 且 **未登记**：%d 处（例 块[%d] %s）' % (len(need), need[0][0], need[0][1]))
        rep.append('④ 断/继续 未回填 未登记 %d 处 ❌' % len(need))
    else:
        filled = sum(1 for b in blks if (b['brk'] is not None and b['brk'] >= 0)
                     or (b['cont'] is not None and b['cont'] >= 0))
        rep.append('④ 断/继续 目标：已回填/已登记 均合规 ✓（已回填块数=%d · 登记 %d 块）'
                   % (filled, len(seg['pending'])))
    return bad, rep


def get_dump(argv):
    """按 `--dump-file` / 编译 探针 取 dump。"""
    if '--dump-file' in argv:
        p = argv[argv.index('--dump-file') + 1]
        return io.open(p, 'rb').read().decode('utf-8', 'replace')
    cc = argv[1] if len(argv) > 1 and not argv[1].startswith('-') else os.path.join(QJ, 'qcc_zh_v2.exe')
    src = argv[2] if len(argv) > 2 and not argv[2].startswith('-') else 'ir_cfg_probe'
    if not src.endswith('.c'):
        src = src + '.c'
    sp = os.path.join(T, src)
    if not os.path.exists(sp):
        sys.exit('探针 缺失: %s（先把 scripts\\probe\\*.c 复制到 %s）' % (sp, T))
    r = subprocess.run([cc, sp, '-o', os.path.join(T, '_real_edges.exe')], capture_output=True, cwd=QJ,
                       env=dict(os.environ, QCC_DUMP='ir'))
    return ((r.stdout or b'') + (r.stderr or b'')).decode('utf-8', 'replace')


# ================= 内置 **负向测试**（证明 判据 非恒真）=================
def selftest():
    """构造 1 份 合规 dump + 4 种 **破坏** dump ⟹ 断言 判据 能 判失败。"""
    head = ('-- IR CFG 块表 (节点序 中块 到 末块; 上退=3) --\n'
            '非落空边(goto 真边) = 2\n'
            '-- IR 待回填 登记 (断/继续/条件假路 的目标 在 节点层 未编码) — 共 2 块 --\n'
            '  块[1] 继续目标 待回填（源内 无 循环上下文）\n'
            '  块[0] 条件假路 目标 待回填（节点层 无 假路 标记）\n'
            '-- IR CFG 回边表 --\n'
            '回边 块[1]→块[1]\n')
    ok = head + (
        '  块[0] 始=0 终一=4 后继数=2 后继0=1 回边=0 首控式=2 前驱数=0 前驱0=-1 后继1=2 断目标=-1 继续目标=-1\n'
        '  块[1] 始=5 终一=8 后继数=2 后继0=2 回边=1 首控式=1 前驱数=2 前驱0=0 后继1=1 断目标=2 继续目标=-1\n'
        '  块[2] 始=9 终一=9 后继数=1 后继0=1 回边=0 首控式=0 前驱数=2 前驱0=1 后继1=-1 断目标=-1 继续目标=-1\n')
    cases = [
        ('基线（合规）', ok, False),
        ('破坏① 非落空边=0', head.replace('= 2', '= 0', 1) + ok.split('\n', 7)[7], True),
        # 破坏②：条件块 后继数=1 **且 删掉 其 登记**（否则 会被 登记 救回 ⟹ 测不出）
        ('破坏② 条件块后继数=1 且无登记',
         ok.replace('  块[0] 条件假路 目标 待回填（节点层 无 假路 标记）\n', '')
           .replace('后继数=2 后继0=1', '后继数=1 后继0=1', 1), True),
        ('破坏③ 删 回边表', ok.replace('回边 块[1]→块[1]\n', ''), True),
        # 破坏④：继续目标=-2（未回填）**且 删掉 登记**
        ('破坏④ 继续目标未回填且无登记',
         ok.replace('  块[1] 继续目标 待回填（源内 无 循环上下文）\n', '')
           .replace('继续目标=-1', '继续目标=-2', 1), True),
        # 破坏⑤：**非循环块 自环**（`控式==1` 之外 出现自环 ⟹ ③② 必须 FAIL）
        ('破坏⑤ 非循环块自环', ok.replace('回边 块[1]→块[1]', '回边 块[0]→块[0]', 1), True),
        # 破坏⑥：回边表 写 **非真实边**（块[0] 的后继集 = [1,2]，不含 块[1] 之外的 … 用 块[2]→块[0] 造 假）
        ('破坏⑥ 回边表 非真实边', ok + '回边 块[2]→块[0]\n', True),
        # 破坏⑦：**伪自环** —— 条件块 的 `后继1` 设成 **自身**（skeptic 实测 的 11 处 形态）⟹ ② 与 ②b 必须 FAIL
        ('破坏⑦ 条件块 后继1==自身（伪自环）', ok.replace('块[0] 始=0 终一=4 后继数=2 后继0=1 回边=0 首控式=2',
                                                       '块[0] 始=0 终一=4 后继数=2 后继0=1 回边=0 首控式=2', 1)
                                                  .replace('后继1=2 断目标=-1', '后继1=0 断目标=-1', 1), True),
    ]
    print('=== 内置 负向测试（判据 必须 能失败）===')
    allok = True
    for name, dump, want_fail in cases:
        bad, _rep = judge(dump)
        got = bool(bad)
        okc = (got == want_fail)
        allok = allok and okc
        print('  [%s] %-28s 判失败=%s（期望 %s）%s'
              % ('OK' if okc else 'NG', name, got, want_fail, ('  ⟹ ' + bad[0][:80]) if bad else ''))
    print()
    print('[%s] 判据 **能失败**（非恒真）' % ('PASS' if allok else 'FAIL'))
    return 0 if allok else 1


if __name__ == '__main__':
    if '--selftest' in sys.argv:
        sys.exit(selftest())
    dump = get_dump(sys.argv)
    bad, rep = judge(dump)
    for l in rep:
        print('  ' + l)
    print()
    print('[%s] 控制流 真边 判据 %s' % ('PASS' if not bad else 'FAIL', ('' if not bad else bad[:3])))
    sys.exit(0 if not bad else 1)
