# -*- coding: utf-8 -*-
"""分类普查判据器（镜像判据 × 出厂工具 两路互证；覆盖 8 条 pass）

做法（两条独立路径互证，非循环）：
  (甲) **驱动出厂工具**：跑 `opt_zh <in.asm> -o <out.asm>`，解析其收尾行的**逐类计数**。
  (乙) **镜像判据独立复算**：按 8 条 pass 的判据在**输入**上重新数一遍。
  ⟹ 断言「(乙) 逐类计数 == (甲) 出厂工具逐类计数」（逐类，不是只看总量）。

注：自「实测缩短量通道」落地后，优化器**不再输出任何字节常量**（`.缩 N`/`.缩总` 已删），
    缩短量一律由汇编器对 `.删`/`.替` 干跑实测 ⟹ 本判据只管**条数**对账，字节对账见
    `scripts/check_opt_per_pass.py`（逐类「产物 .text 差值 == 汇编器自报量」）。

用法: python scripts/check_opt_census.py [<in.asm>] [--selftest]
退出码: 0 = 逐类一致 ; 1 = 不一致
"""
import io, os, re, sys, collections, subprocess

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
os.chdir(ROOT)
OPT = 'opt_zh.exe'
CLS = ['栈spill', '栈合并', '抵消对', '不可达跳转', '不可达返回', '复制传播', '跳落到下一行', '帧栈存取同址', '死值写覆盖']
NUM = {'rax': 0, 'rcx': 1, 'rdx': 2, 'rbx': 3, 'rsp': 4, 'rbp': 5, 'rsi': 6, 'rdi': 7,
       'eax': 0, 'ecx': 1, 'edx': 2, 'ebx': 3, 'esp': 4, 'ebp': 5, 'esi': 6, 'edi': 7,
       'al': 0, 'cl': 1, 'dl': 2, 'bl': 3, 'ax': 0, 'cx': 1, 'dx': 2, 'bx': 3,
       'sp': 4, 'bp': 5, 'si': 6, 'di': 7}
PURE = ('移动', '移即', '移64', '移32', '取帧32', '取帧64', '取栈32', '取栈64',
        '取静32', '取静64', '取静址', '取帧址', '零扩展', '零扩展32', '零扩展64',
        '符号扩展', '符号扩展64', '符号扩展字节', '取静零8', '取静符8', '取静零16', '取静符16')
COND = ('为零跳', '非零跳', '非负跳', '非溢出跳', '负跳', '大跳', '小跳', '溢跳', '不溢跳',
        '奇跳', '偶跳', '大于等跳', '小于等跳', '高于等跳', '低于跳', '高于跳')


def regno(t):
    t = t.strip().strip('[]').strip()
    for s in ('+', '-', '*'):
        t = t.split(s)[0]
    t = t.strip()
    m = re.match(r'^r(\d+)$', t)
    return int(m.group(1)) if m else NUM.get(t, -1)


def mentions(seg, want):
    for t in re.split(r'[,\s\[\]\+\-\*]+', seg):
        if not t:
            continue
        r = regno(t)
        if r == want:
            return True
        if r < 0 and t[0].isalpha():
            return True
    return False


def toks(t):
    p = t.split(' ', 1)
    return [x for x in re.split(r'[,\s]+', p[1]) if x] if len(p) > 1 else []


def mirror(lines):
    """镜像判据：独立于出厂工具，按 8 条 pass 的判据重新数一遍"""
    c = collections.Counter()
    n = len(lines)
    k = 0
    while k < n:
        a = lines[k]
        b = lines[k + 1] if k + 1 < n else ''
        b2 = lines[k + 2] if k + 2 < n else ''
        done = False
        # ① 减栈 8 + 存栈64 [rsp+0], rX
        if a == '减栈 8' and b.startswith('存栈64 [rsp+0], '):
            c['栈spill'] += 1; k += 2; done = True
        # ② 取栈64 rX,[rsp+0] + 加栈 8
        if not done and a.startswith('取栈64 ') and a.endswith(', [rsp+0]') and b == '加栈 8':
            c['栈合并'] += 1; k += 2; done = True
        # ③ 加栈 N + 减栈 N（操作数逐字同）
        if not done and a.startswith('加栈 ') and b.startswith('减栈 ') \
                and a.split(' ', 1)[1].strip() == b.split(' ', 1)[1].strip():
            c['抵消对'] += 1; k += 2; done = True
        # ④ 跳转 X + 跳转/返回
        if not done and a.startswith('跳转 '):
            if b.startswith('跳转 '):
                c['不可达跳转'] += 1; k += 2; done = True
            elif b == '返回':
                c['不可达返回'] += 1; k += 2; done = True
        # ⑤ 移64/移32 rX,rY / rZ,rX / rX,存（第三行须纯写 rX 且不读 rX）
        if not done and (a.startswith('移64 ') or a.startswith('移32 ')) and k + 2 < n:
            op = '移64 ' if a.startswith('移64 ') else '移32 '
            if b.startswith(op) and b2.startswith(op):
                r1, r2, r3 = toks(a), toks(b), toks(b2)
                if len(r1) >= 2 and len(r2) >= 2 and len(r3) >= 2 and r2[1] == r1[0] and r3[0] == r1[0]:
                    xn = regno(r1[0])
                    if xn >= 0 and not r3[0].startswith('[') and regno(r3[0]) == xn \
                            and not mentions(r3[1], xn):
                        c['复制传播'] += 1; k += 2; done = True
        # ⑥ 跳转类 + 紧跟其后标号（目标标号唯一）
        if not done and k + 1 < n:
            mn = a.split(' ', 1)[0]
            jl = 5 if mn == '跳转' else (2 if mn == '跳转短' else (6 if mn in COND else 0))
            if jl:
                tg = a.split(' ', 1)[1].strip() if ' ' in a else ''
                if tg and b == tg + ':':
                    c['跳落到下一行'] += 1; k += 2; done = True
        # ⑦ 存<族><宽> [mem], rX + 取<族><宽> rY, [mem]（逐字同址）
        if not done and k + 1 < n:
            m1 = re.match(r'^存(帧|栈)(32|64) (\[[^\]]+\]), (r\d+)$', a)
            m2 = re.match(r'^取(帧|栈)(32|64) (r\d+), (\[[^\]]+\])$', b)
            if m1 and m2 and m1.group(1) == m2.group(1) and m1.group(2) == m2.group(2) \
                    and m1.group(3) == m2.group(4):
                c['帧栈存取同址'] += 1; k += 2; done = True
        # ⑧ 死值写覆盖：mov 类写 rX 紧跟「纯写 rX 且不读 rX」
        if not done and k + 1 < n:
            mnn = a.split(' ', 1)[0]
            if mnn in ('移动', '移即', '移64', '移32') and b and not b.endswith(':'):
                if b.split(' ', 1)[0] in PURE:
                    ta, tb = toks(a), toks(b)
                    if ta and tb and not tb[0].startswith('['):
                        xn = regno(ta[0])
                        if xn >= 0 and regno(tb[0]) == xn and len(tb) >= 2 and not mentions(tb[1], xn):
                            c['死值写覆盖'] += 1; k += 1; done = True
        if not done:
            k += 1
    return c


def tool_counts(p):
    r = subprocess.run([OPT, p, '-o', p + '.out'], capture_output=True)
    o = (r.stdout + r.stderr).decode('utf-8', 'replace')
    m = re.search(r'优化 \d+ 处 \[([^\]]*)\]', o)
    if not m:
        return None, o
    return [int(x) for x in re.findall(r'(\d+)', m.group(1))], o


def check(p, tag):
    lines = [l.strip(' \t\r') for l in io.open(p, encoding='utf-8', errors='replace').read().split('\n')]
    mir = mirror(lines)
    nums, o = tool_counts(p)
    if nums is None:
        print('  [FAIL] %s: 未取到出厂工具分类计数' % tag); return ['工具计数缺失']
    fails = []
    print('  %-10s %s' % (tag, o.strip().split('优化')[-1][:100] if '优化' in o else o[:80]))
    print('    %-16s %8s %8s' % ('pass 类', '镜像判据', '出厂工具'))
    for i2, cls in enumerate(CLS):
        mi, ti = mir.get(cls, 0), (nums[i2] if i2 < len(nums) else -1)
        flag = '' if mi == ti else '  ★不一致'
        print('    %-16s %8d %8d%s' % (cls, mi, ti, flag))
        if mi != ti:
            fails.append('%s/%s: 镜像 %d vs 工具 %d' % (tag, cls, mi, ti))
    return fails


if '--selftest' in sys.argv:
    trigger = ('_入口:\n'
               '    减栈 8\n    存栈64 [rsp+0], r0\n'
               '    取栈64 r0, [rsp+0]\n    加栈 8\n'
               '    加栈 24\n    减栈 24\n'
               '    跳转 .La\n    跳转 .Lb\n    跳转 .Lc\n    返回\n'
               '    移64 r2, r3\n    移64 r4, r2\n    移64 r2, r1\n'
               '    跳转 .Ld\n.Ld:\n'
               '    存帧64 [rbp-8], r0\n    取帧64 r3, [rbp-8]\n'
               '    移动 r0, 77\n    移64 r0, r1\n'
               '    移动 r0, 9\n    加 r0, 3\n'          # ⑧ 负向（读-改-写）
               '    移动 r5, 3\n    移64 r5, [rbp-8]\n'  # ⑧ 负向（别名）
               '.La:\n.Lb:\n.Lc:\n    返回\n')
    p = os.path.join('build', '_census_selftest.asm')
    io.open(p, 'w', encoding='utf-8', newline='\n').write(trigger)
    print('== check_opt_census --selftest（8 类全覆盖触发输入）==')
    fails = check(p, 'selftest')
    # 期望：触发输入里每类各命中 1 次；⑧ 命中 2 次（`移动 r0,77`+`移64 r0,r1` 与 `移64 r0,r1`+`移动 r0,9` 各一对）
    exp = dict(zip(CLS, [1, 1, 1, 1, 1, 1, 1, 1, 2]))
    lines = [l.strip(' \t\r') for l in io.open(p, encoding='utf-8', errors='replace').read().split('\n')]
    mir = mirror(lines)
    for cls, want in exp.items():
        if mir.get(cls, 0) != want:
            fails.append('selftest/镜像 %s = %d（期望 %d）' % (cls, mir.get(cls, 0), want))
    print('== 结果: %s ==' % ('逐类一致 + 8 类各命中 1 次 ✓' if not fails else 'FAIL=%d' % len(fails)))
    for f in fails:
        print('   -', f)
    sys.exit(0 if not fails else 1)

SRC = sys.argv[1] if len(sys.argv) > 1 and not sys.argv[1].startswith('-') else os.path.join('build', 'opt_t10b', 'Z.exe.asm')
if not os.path.exists(SRC):
    print('[SKIP] 输入不存在: %s' % SRC); sys.exit(1)
print('== 优化器分类普查（镜像判据 × 出厂工具 逐类互证）==')
fails = check(SRC, 'corpus')
print('== 结果: %s ==' % ('逐类一致（分类统计可复算）' if not fails else 'FAIL=%d' % len(fails)))
for f in fails:
    print('   -', f)
sys.exit(0 if not fails else 1)
