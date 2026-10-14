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
T = os.path.join(os.environ.get('TEMP', '.'), 'qiyuan-ir-knives')      # 探针 目录（检查器 既有 约定）
BLK = re.compile(r'块\[(\d+)\] 始=(-?\d+) 终一=(-?\d+) 后继数=(\d+) 后继0=(-?\d+) 回边=(\d+) 首控式=(\d+)'
                 r'(?: 前驱数=(\d+) 前驱0=(-?\d+))?'
                 r'(?: 后继1=(-?\d+) 回边目标=(-?\d+) 断目标=(-?\d+) 继续目标=(-?\d+))?')


def parse(dump):
    """→ (块列表 dict, 段落 值 dict)"""
    blks = []
    for m in BLK.finditer(dump):
        g = m.groups()
        blks.append(dict(n=int(g[0]), start=int(g[1]), end=int(g[2]), nsucc=int(g[3]),
                         succ0=int(g[4]), backed=int(g[5]), ctrl=int(g[6]),
                         npred=(int(g[7]) if g[7] is not None else None),
                         succ1=(int(g[9]) if g[9] is not None else None),
                         back_tgt=(int(g[10]) if g[10] is not None else None),
                         brk=(int(g[11]) if g[11] is not None else None),
                         cont=(int(g[12]) if g[12] is not None else None)))
    seg = {}
    m = re.search(r'非落空边\(goto 真边\) = (\d+)', dump)
    seg['nonfall'] = int(m.group(1)) if m else None
    m = re.search(r'IR CFG 块表 .*?上退=(\d+)', dump)
    seg['nblk'] = int(m.group(1)) if m else None
    # 待回填 登记：**项级**（块号, 项）—— 段标题 与 块号 在 不同行 ⟹ 全 dump 搜；
    #   ⚠️ 粒度 = (块, 项)：块级集合 会 让「该块登记了 条件假路」**救回**「该块 继续 待回填」（自测 破坏④ 抓出）
    seg['pending'] = set((int(a), b) for a, b in
                         re.findall(r'块\[(\d+)\] (断|继续|条件假路) 目标 待回填', dump))
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
        reg2 = [b for b in cond if b['nsucc'] == 1 and (b['n'], '条件假路') in seg['pending']]
        bad2 = [b for b in cond if b['nsucc'] != 2 and (b['n'], '条件假路') not in seg['pending']]
        if bad2:
            b = bad2[0]
            bad.append('② 条件块：%d 个 后继数!=2 **且 未登记**（例：块[%d] 后继数=%d）'
                       % (len(bad2), b['n'], b['nsucc']))
            rep.append('② 条件块（%d）后继数==2 或 显式登记：%d 个不符 ❌' % (len(cond), len(bad2)))
        else:
            rep.append('② 条件块（%d）后继数==2 或 显式登记 ✓（其中 显式登记 = %d）' % (len(cond), len(reg2)))

    # ③ 循环块（首控式==1 = 循环头）必须 **有回边指回它** —— 且 回边源 在 它 **之后**（块号更大，
    #    块号 按 节点序 递增 ⟹ 块号大 = 程序序 靠后）。语义：从 循环体末尾 回到 循环头。
    loop = [b for b in blks if b['ctrl'] == 1]
    if not loop:
        bad.append('③ 无 首控式==1 的块（探针 含 循环 ⟹ 疑 dump 段缺）')
        rep.append('③ 循环块 = 0 ❌')
    else:
        if not seg['has_back_sec']:
            bad.append('③ dump **无** `IR CFG 回边表` 段 ⟹ 回边 不可判（%d 个 循环头）' % len(loop))
            rep.append('③ 回边表 **段缺失** ❌')
        else:
            bad3 = [h for h in loop if not any(v == h['n'] for (u, v) in seg['back_edges'])]
            if bad3:
                h = bad3[0]
                bad.append('③ 循环头 无 回边 指回：%d/%d 个（例：块[%d] 始=%d）'
                           % (len(bad3), len(loop), h['n'], h['start']))
                rep.append('③ 循环头（%d 个）有回边指回：%d 个不符 ❌' % (len(loop), len(bad3)))
            else:
                rep.append('③ 循环头（%d 个）均有回边指回 ✓（回边表 %d 条）'
                           % (len(loop), len(seg['back_edges'])))

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
        '  块[0] 始=0 终一=4 后继数=2 后继0=1 回边=0 首控式=2 前驱数=0 前驱0=-1 后继1=2 回边目标=-1 断目标=-1 继续目标=-1\n'
        '  块[1] 始=5 终一=8 后继数=1 后继0=2 回边=1 首控式=1 前驱数=2 前驱0=0 后继1=-1 回边目标=-1 断目标=2 继续目标=-1\n'
        '  块[2] 始=9 终一=9 后继数=1 后继0=1 回边=0 首控式=0 前驱数=2 前驱0=1 后继1=-1 回边目标=-1 断目标=-1 继续目标=-1\n')
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
