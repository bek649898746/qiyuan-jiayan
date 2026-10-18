# -*- coding: utf-8 -*-
"""逐条 pass 精确对账判据器（plan Verification 1）

对**每一类** pass 造一个最小触发输入（`.asm`），走真实链路
    `出厂 opt_zh → 出厂 asm_zh`（不需要编译器，秒级），然后断言三件事：
  ① 该类条数 > 0（读出厂优化器的分类计数行）；
  ② 其余各类条数 == 0（输入只触发该类，做到**逐类隔离**）；
  ③ **产物 `.text` 有效内容差值 == 汇编器自报缩短量**（单源真相在编码器 ⟹ 优化器侧不得有未实测的字节常量）。

用法: python scripts/check_opt_per_pass.py        退出码 0 = 逐条全过
"""
import io, os, re, struct, subprocess, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
os.chdir(ROOT)
OPT, ASM = 'opt_zh.exe', 'asm_zh.exe'
W = os.path.join('build', 'per_pass')
os.makedirs(W, exist_ok=True)

CLS = ['栈spill', '栈合并', '抵消对', '不可达', '复制传播', '跳落到下一行', '帧栈存取同址', '死值写覆盖']

CASES = [
    ('① 栈spill→压栈', '    减栈 8\n    存栈64 [rsp+0], r0\n    返回\n'),
    ('② 栈合并→弹栈', '    取栈64 r0, [rsp+0]\n    加栈 8\n    返回\n'),
    ('③ 加栈N+减栈N 抵消对', '    加栈 24\n    减栈 24\n    返回\n'),
    ('④ 不可达 跳转/返回', '    跳转 .La\n    跳转 .Lb\n    跳转 .Lc\n    返回\n.La:\n.Lb:\n.Lc:\n    返回\n'),
    ('⑤ 复制传播+死值', '    移64 r2, r3\n    移64 r4, r2\n    移64 r2, r1\n    返回\n'),
    ('⑥ 跳落到下一行', '    跳转 .Ld\n.Ld:\n    返回\n'),
    ('⑦ 帧栈存取同址', '    存帧64 [rbp-8], r0\n    取帧64 r3, [rbp-8]\n    返回\n'),
    ('⑧ 死值写覆盖', '    移动 r0, 5\n    移64 r0, r1\n    返回\n'),
]


def sh(a, t=600):
    r = subprocess.run(a, capture_output=True, timeout=t)
    return r.returncode, (r.stdout + r.stderr).decode('utf-8', 'replace')


def ts(p):
    d = io.open(p, 'rb').read()
    pe = struct.unpack_from('<I', d, 0x3C)[0]
    n = struct.unpack_from('<H', d, pe + 6)[0]
    o = struct.unpack_from('<H', d, pe + 20)[0]
    for i in range(n):
        q = pe + 24 + o + 40 * i
        if d[q:q + 8].rstrip(b'\x00').decode('latin1') == '.text':
            vsz, va, rsz, ro = struct.unpack_from('<IIII', d, q + 8)
            b = d[ro:ro + rsz]
            j = len(b) - 1
            while j >= 0 and b[j] == 0:
                j -= 1
            return j + 1
    return -1


print('== 逐条 pass 精确对账（出厂 opt_zh → 出厂 asm_zh；粒度 = 该类）==')
print('  %-24s %6s %10s %10s %10s %s' % ('pass 类', '条数', '自报缩短', '.text 差', '其它类', '判定'))
fails = []
tot = 0
for name, body in CASES:
    p = os.path.join(W, 'c.asm')
    io.open(p, 'w', encoding='utf-8', newline='\n').write('_入口:\n' + body)
    po = os.path.join(W, 'c_o.asm')
    rc1, o1 = sh([OPT, p, '-o', po])
    rc2, o2 = sh([ASM, p, '-o', os.path.join(W, 'c_n.exe')])       # 不加优化链
    rc3, o3 = sh([ASM, po, '-o', os.path.join(W, 'c_o.exe')])      # 优化链
    if rc1 or rc2 or rc3:
        fails.append('%s: 链路 rc=%d/%d/%d %s' % (name, rc1, rc2, rc3, o3.strip()[:80]))
        print('  %-24s ★链路失败' % name)
        continue
    m = re.search(r'优化 \d+ 处 \[([^\]]*)\]', o1)
    segs = [x.strip() for x in m.group(1).split('/')] if m else []
    nums = [int(re.search(r'(\d+)$', s).group(1)) for s in segs if re.search(r'(\d+)$', s)]
    mz = re.search(r'总缩=(-?\d+)', o3)
    tz = int(mz.group(1)) if mz else -1
    dn, do = ts(os.path.join(W, 'c_n.exe')), ts(os.path.join(W, 'c_o.exe'))
    delta = dn - do
    idx = CASES.index((name, body))
    mine = nums[idx] if idx < len(nums) else -1
    others = sum(n for i2, n in enumerate(nums) if i2 != idx)
    ok = (mine > 0) and (others == 0) and (delta == tz) and (tz > 0)
    print('  %-24s %6d %10d %10d %10s %s'
          % (name, mine, tz, delta, others, '✓' if ok else '★FAIL'))
    if not ok:
        fails.append('%s: 条数=%d 自报=%d 差值=%d 其它类=%d' % (name, mine, tz, delta, others))
    tot += tz
print()
print('  逐类自报缩短合计 = %d 字节' % tot)
print('  （每类都做过「差值 == 自报量」，且优化器侧已无任何未实测的手工字节常量：'
      '①–⑥ 的原 `.缩 N` 常量已全部改为 `.删`/`.替` 声明，由汇编器干跑实测）')
print('== 结果: %s ==' % ('逐条全过' if not fails else 'FAIL=%d' % len(fails)))
for f in fails:
    print('   -', f)
sys.exit(0 if not fails else 1)
