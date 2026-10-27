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
# R821: 允许指向待测优化器 —— 负控（用改动前的优化器）时必须变红。
if '--opt' in sys.argv: OPT = sys.argv[sys.argv.index('--opt') + 1]
if '--asm' in sys.argv: ASM = sys.argv[sys.argv.index('--asm') + 1]
# 工作目录按**进程**隔离：并发跑两个本判据（或本判据 + run_criteria）时不许互相踩同名造件。
W = os.path.join(os.environ.get('TEMP', '.'), 'qcc_per_pass_%d' % os.getpid())
os.makedirs(W, exist_ok=True)

CLS = ['栈spill', '栈合并', '抵消对', '不可达', '复制传播', '跳落到下一行', '帧栈存取同址', '死值写覆盖']

CASES = [
    ('① 栈spill→压栈', '    减栈 8\n    存栈64 [rsp+0], r0\n    返回\n', None, '栈spill'),
    ('② 栈合并→弹栈', '    取栈64 r0, [rsp+0]\n    加栈 8\n    返回\n', None, '栈合并'),
    ('③ 加栈N+减栈N 抵消对', '    加栈 24\n    减栈 24\n    返回\n', None, '抵消对'),
    ('④ 不可达 跳转/返回（两子计数）', '    跳转 .La\n    跳转 .Lb\n    跳转 .Lc\n    返回\n.La:\n.Lb:\n.Lc:\n    返回\n', None, '不可达跳转', ('不可达返回',)),
    ('⑤ 复制传播+死值', '    移64 r2, r3\n    移64 r4, r2\n    移64 r2, r1\n    返回\n', None, '复制传播'),
    ('⑥ 跳落到下一行', '    跳转 .Ld\n.Ld:\n    返回\n', None, '跳落到下一行'),
    ('⑦ 帧栈存取同址', '    存帧64 [rbp-8], r0\n    取帧64 r3, [rbp-8]\n    返回\n', None, '帧栈存取同址'),
    ('⑧ 死值写覆盖', '    移动 r0, 5\n    移64 r0, r1\n    返回\n', None, '死值写覆盖'),
    ('⑨ 零栈调整', '    加栈 0\n    返回\n', None, '零栈调整'),
    ('⑩ G-DCE 跨块死定义',
     '    移动 r2, 7\n    移动 r0, 3\n    返回\n',
     '; @QCCMETA 1 ins=3 funcs=1 loops=0 nodes=4\n'
     '; @I 0 f=1 n=1 k=1 t=-1 l=-1 d=0\n'
     '; @I 1 f=1 n=2 k=1 t=-1 l=-1 d=0\n'
     '; @I 2 f=1 n=3 k=2 t=-1 l=-1 d=0\n'
     '; @U 0 r=2 z=1 u=\n'
     '; @U 1 r=0 z=0 u=2\n'
     '; @QCCMETA-END\n', 'G-DCE'),
    ('⑪ 比较-分支 融合',
     '    压栈 r0\n    移动 r0, 5\n    弹栈 r3\n    比较 r3, r0\n    置小 al\n'
     '    零扩展 eax, al\n    测试 r0, r0\n    为零跳 .Lend\n    移动 r0, 1\n.Lend:\n    返回\n',
     None, '比较分支融合'),
    ('⑫ 算术-立即数 融合',
     '    压栈 r0\n    移动 r0, 5\n    弹栈 r3\n    减 r3, r0\n    移32 r0, r3\n    返回\n',
     None, '算术立即融合'),
    ('⑬ 循环槽提升 LSP',
     '    减栈 4416\n    移动 r0, 0\n    存帧32 [rbp-4400], r0\n'
     '.L环路:\n    取帧32 r0, [rbp-4400]\n    比较 r0, 100\n    大于等跳 .L出口\n'
     '    取帧32 r0, [rbp-4400]\n    加 r0, 1\n    存帧32 [rbp-4400], r0\n'
     '    跳转 .L环路\n.L出口:\n    取帧32 r0, [rbp-4400]\n    加栈 4416\n    返回\n',
     '; @QCCMETA 1 ins=13 funcs=1 loops=1 nodes=13\n'
     + ''.join('; @I %d f=1 n=%d k=1 t=-1 l=%d d=0\n'
               % (i, i, 1 if 3 <= i <= 9 else -1) for i in range(13))
     + '; @L 1 s=3 e=9 d=1\n'
     '; @QCCMETA-END\n', '循环槽提升'),
    ('⑭ 跨块 存→取 转发',
     '    移64 r5, r4\n    减栈 64\n    移动 r2, 7\n    存帧32 [rbp-8], r2\n'
     '.L跨:\n    取帧32 r0, [rbp-8]\n    加栈 64\n    返回\n',
     '; @QCCMETA 1 ins=7 funcs=1 loops=0 nodes=7\n'
     + ''.join('; @I %d f=1 n=%d k=1 t=-1 l=-1 d=0\n' % (i, i) for i in range(7))
     + '; @QCCMETA-END\n', '跨块转发'),
    ('⑮ 压栈回读折叠',
     '    压栈 r0\n    减栈 40\n    取栈64 r0, [rsp+40]\n    移64 r1, r0\n    加栈 48\n    返回\n',
     '@AUTO', '压栈回读'),
]

CLASS_KEYS = [
    ('栈spill', r'栈spill (\d+)'), ('栈合并', r'栈合并 (\d+)'), ('抵消对', r'抵消对 (\d+)'),
    ('不可达跳转', r'不可达 跳转 (\d+)'), ('不可达返回', r'不可达 跳转 \d+ 返回 (\d+)'),
    ('复制传播', r'复制传播 (\d+)'), ('跳落到下一行', r'跳落到下一行 (\d+)'),
    ('帧栈存取同址', r'帧栈存取同址 (\d+)'), ('死值写覆盖', r'死值写覆盖 (\d+)'),
    ('零栈调整', r'零栈调整 (\d+)'), ('G-DCE', r'G-DCE (\d+)'),
    ('比较分支融合', r'比较分支融合 (\d+)'), ('算术立即融合', r'算术立即融合 (\d+)'),
    ('循环槽提升', r'LSP 槽=(\d+)'), ('跨块转发', r'跨块转发 (\d+)'),
    ('压栈回读', r'压栈回读 (\d+)'),
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


def run_exit(exe, retry_ok_other=None):
    """跑一个 exe 取退出码；给了「另一条链的退出码」且首轮不同时，**再跑一轮**（真破坏是确定性的）。"""
    e = subprocess.run([exe], capture_output=True, timeout=120).returncode & 0xFFFFFFFF
    if retry_ok_other is not None and e != retry_ok_other:
        e = subprocess.run([exe], capture_output=True, timeout=120).returncode & 0xFFFFFFFF
    return e


def unknown_class_names(seg, known):
    """从收尾行 `[...]` 段里取出的类名，凡不在 known（归一化去空格）里的都返回。"""
    out = []
    for chunk in [c.strip() for c in seg.split('/') if c.strip()]:
        nm = ''
        for ch in chunk:
            if ch.isdigit():
                break
            nm += ch
        nm = nm.strip().replace(' ', '')
        if nm and nm not in known and nm not in out:
            out.append(nm)
    return out


PB = '''_入口:
    压栈 r5
    压栈 r3
    移64 r5, r4
    减栈 64
'''
PT = '''    加栈 64
    弹栈 r3
    弹栈 r5
    返回
'''


def _fold(sp, tail=''):
    """一次「压栈 / 减栈 40 / 回读」的造件；地址拼写由 sp 给。"""
    return (PB + '    移动 r0, 7\n    压栈 r0\n    减栈 40\n    取栈64 r0, ' + sp +
            '\n    移64 r1, r0\n' + tail + '    加栈 48\n    移64 r0, r1\n' + PT)


def with_meta(code):
    """给造件补一份**完备**元数据附录（结构层只在 `; @QCCMETA 1` 存在时启用）。幂等。"""
    if '@QCCMETA' in code:
        return code
    lines = [l for l in code.split('\n') if l.strip()]
    n = len([l for l in lines if not l.strip().endswith(':')])
    meta = ['; @QCCMETA 1 ins=%d funcs=1 loops=0 nodes=%d' % (n, n)]
    for i in range(n):
        meta.append('; @I %d f=1 n=%d k=1 t=-1 l=-1 d=0' % (i, i))
    meta.append('; @QCCMETA-END')
    return '\n'.join(lines) + '\n' + '\n'.join(meta) + '\n'


XBLK_POS = '''_入口:
    移64 r5, r4
    减栈 64
    移动 r2, 7
    存帧32 [rbp-8], r2
.Lb:
    取帧32 r0, [rbp-8]
    加栈 64
    返回
'''

XBLK_ADDR = '''_入口:
    移64 r5, r4
    减栈 64
    取帧址 r3, [rbp-8]
    移动 r2, 100
    存帧32 [rbp-8], r2
    移动 r0, 0
    存32 [r3], r0
.Lb:
    取帧32 r0, [rbp-8]
    加栈 64
    返回
'''

FOLDPAT = r'压栈回读 (\d+)'
XBLKPAT = r'存→取 转发 条=(\d+)'


def _wr(sp, mid):
    """「压栈 / 减栈 40 / 回读」造件（带完备附录）；mid = 回读行之后、收尾之前的中间行。"""
    return with_meta(PB + '    移动 r0, 7\n    压栈 r0\n    减栈 40\n    取栈64 r0, ' + sp + '\n' + mid +
                     '    加栈 48\n    移64 r0, r1\n' + PT)


def _fold(sp):
    return _wr(sp, '    移64 r1, r0\n')


# 生成式矩阵：稳定 ID → (造件, 类计数正则, 期望折叠数)。
# ⚠ ⑮（R881L 起）**永不删/改内存写** ⟹ 只与「三行形状 + 地址/寄存器前提」有关，
#    与「以后谁读这一格」（读/写/调用/控制流/汇合）**全都无关** ⟹ 那些格子一律期望 1。
#    真正会挡住它的是：地址不是 [rsp+N]、压栈源或回读目的是 rsp、减栈不是立即数、三行不连续。
MATRIX = []
for cid, sp in (('sp0', '[rsp+40]'), ('sp1', '[rsp + 40]'), ('sp2', '[r4+40]'), ('sp3', '[r4 + 40]'),
                ('sp4', '[r4\t+ 40]'), ('sp5', '[esp+40]'), ('sp6', '[ rsp + 40 ]'),
                ('sp7', '[ rsp\t+\t40 ]')):
    MATRIX.append(('%s 地址拼写 %r' % (cid, sp), _fold(sp), FOLDPAT, 1))
MATRIX.append(('nb0 非 rsp 基址 [rbp-8]', _fold('[rbp-8]'), FOLDPAT, 0))
MATRIX.append(('dm0 位移不匹配 [rsp+32]', _fold('[rsp+32]'), FOLDPAT, 0))
MATRIX.append(('rs0 压栈 rsp 自身', with_meta(PB + '    压栈 r4\n    减栈 40\n    取栈64 r0, [rsp+40]\n'
                                           '    移64 r1, r0\n    加栈 48\n    移64 r0, r1\n' + PT), FOLDPAT, 0))
MATRIX.append(('rs1 目的是 rsp', with_meta(PB + '    移动 r0, 7\n    压栈 r0\n    减栈 40\n    取栈64 r4, [rsp+40]\n'
                                        '    加栈 48\n    加栈 64\n    弹栈 r3\n    弹栈 r5\n    返回\n'), FOLDPAT, 0))
# —— 以下五格是 R881b–R881k 七轮打回过的形态。⑮ 改成「保内存」后它们**全部安全**：
#    读走的是同一格、写一个字节没动 ⟹ 两链见到完全相同的内存。
MATRIX.append(('tw0 二次回读', with_meta(PB + '    移动 r0, 7\n    压栈 r0\n    减栈 40\n    取栈64 r0, [rsp+40]\n'
                                      '    取栈64 r1, [rsp+40]\n    加栈 48\n    移64 r0, r1\n' + PT), FOLDPAT, 1))
MATRIX.append(('tw1 Tab 第二读', with_meta(PB + '    移动 r0, 7\n    压栈 r0\n    减栈 40\n    取栈64 r0, [rsp+40]\n'
                                        '    取栈64 r1, [r4\t+ 40]\n    加栈 48\n    移64 r0, r1\n' + PT), FOLDPAT, 1))
MATRIX.append(('tw2 Tab 整格重写', with_meta(PB + '    移动 r0, 7\n    压栈 r0\n    减栈 40\n    取栈64 r0, [rsp+40]\n'
                                         '    移动 r1, 9\n    存栈64 [r4\t+ 40], r1\n'
                                         '    取栈64 r1, [r4\t+ 40]\n    加栈 48\n    移64 r0, r1\n' + PT), FOLDPAT, 1))
MATRIX.append(('wd0 部分宽写 存栈32 后取栈64',
               with_meta(PB + '    移动 r2, 0\n    按位反64 r2\n    压栈 r2\n    弹栈 r2\n'
                          '    移动 r0, 7\n    压栈 r0\n    减栈 40\n    取栈64 r0, [rsp+40]\n'
                          '    移动 r1, 0\n    存栈32 [rsp+40], r1\n    取栈64 r1, [rsp+40]\n'
                          '    逻辑右移64 r1, 32\n    加栈 48\n    移64 r0, r1\n' + PT), FOLDPAT, 1))
MATRIX.append(('wd1 部分宽写落 A 高半 [rsp+44]',
               with_meta(PB + '    移动 r0, 7\n    压栈 r0\n    减栈 40\n    取栈64 r0, [rsp+40]\n'
                          '    移动 r1, 0\n    存栈32 [rsp+44], r1\n    取栈64 r1, [rsp+40]\n'
                          '    加栈 48\n    移64 r0, r1\n' + PT), FOLDPAT, 1))
MATRIX.append(('wd2 部分宽写不落 A [rsp+4]',
               with_meta(PB + '    移动 r0, 7\n    压栈 r0\n    减栈 40\n    取栈64 r0, [rsp+40]\n'
                          '    移动 r1, 0\n    存栈32 [rsp+4], r1\n'
                          '    加栈 48\n    移64 r0, r1\n' + PT), FOLDPAT, 1))
MATRIX.append(('ps0 恢复后再读 [rsp-8]', with_meta(PB + '    移动 r0, 7\n    压栈 r0\n    减栈 40\n    取栈64 r0, [rsp+40]\n'
                                              '    加栈 48\n    取栈64 r1, [rsp-8]\n    移64 r0, r1\n' + PT), FOLDPAT, 1))
MATRIX.append(('br0 条件跳过写', with_meta(PB + '    移动 r0, 7\n    压栈 r0\n    减栈 40\n    取栈64 r0, [rsp+40]\n'
                                       '    测试 r0, r0\n    非零跳 .Lskip\n    移动 r1, 7\n    存栈64 [rsp+40], r1\n'
                                       '.Lskip:\n    取栈64 r1, [rsp+40]\n    加栈 48\n    移64 r0, r1\n' + PT), FOLDPAT, 1))
MATRIX.append(('mg0 不同栈深汇合', with_meta(PB + '    移动 r0, 7\n    压栈 r0\n    减栈 40\n    取栈64 r0, [rsp+40]\n'
                                         '    测试 r0, r0\n    非零跳 .Lm\n    减栈 8\n.Lm:\n    取栈64 r1, [rsp+40]\n'
                                         '    移64 r0, r1\n    加栈 48\n' + PT), FOLDPAT, 1))
MATRIX.append(('es0 地址逃逸后经指针读',
               with_meta(PB.replace('    减栈 64\n', '    减栈 64\n    移动 r2, 7\n    存帧32 [rbp-8], r2\n') +
                         '    取帧址 r3, [rbp-8]\n    移动 r0, 7\n    压栈 r0\n    减栈 40\n'
                         '    取栈64 r0, [rsp+40]\n    取帧32 r1, [r3]\n    加栈 48\n'
                         '    移64 r0, r1\n' + PT), FOLDPAT, 1))
# 目的寄存器 != 被压寄存器（发送侧的 `移64 rY, rX` 分支）—— 必须被真跑覆盖。
MATRIX.append(('rd0 目的寄存器不同（发 移64 rY,rX）',
               with_meta(PB + '    移动 r0, 7\n    压栈 r0\n    减栈 40\n    取栈64 r1, [rsp+40]\n'
                          '    加栈 48\n    移64 r0, r1\n' + PT), FOLDPAT, 1))
# 真实产物形态：栈深先抬回去，再由一条 `压栈` 覆盖同址 —— 与本 pass 无关（写者保留）。
MATRIX.append(('pf0 栈深偏移后由 压栈 重写',
               with_meta(PB + '    移动 r0, 7\n    压栈 r0\n    减栈 40\n    取栈64 r0, [rsp+40]\n'
                          '    加栈 48\n    压栈 r1\n    加栈 8\n' + PT), FOLDPAT, 1))
# ★ 立即数口径（R881M · 评审第 9 条）：`减栈 N` 与 `[rsp±d]` 必须按**汇编器 NUM 的同一份语法**解析
#   （可选 '-'、0x 十六进制、十进制）。公式 `d == N` 对正负都成立（`减栈 N` ⟹ rsp = A - N）。
#   ng0 是评审给的复现件：旧解析把 '-' 当数字位（-8 变 -22）⟹ 与 [rsp-22] 误配 ⟹ 未优化 7 / 优化 5。
MATRIX.append(('ng0 负立即数 减栈 -8 与 [rsp-22]（错配必拒）',
               with_meta(PB + '    移动 r2, 7\n    存栈64 [rsp-22], r2\n    移动 r0, 5\n    压栈 r0\n'
                          '    减栈 -8\n    取栈64 r1, [rsp-22]\n    移64 r0, r1\n' + PT), FOLDPAT, 0))
MATRIX.append(('ng1 负立即数 减栈 -8 与 [rsp-8]（正配对必折）',
               with_meta(PB + '    移动 r0, 7\n    压栈 r0\n    减栈 -8\n    取栈64 r1, [rsp-8]\n'
                          '    移64 r0, r1\n' + PT), FOLDPAT, 1))
MATRIX.append(('nh0 十六进制立即数 减栈 0x28 与 [rsp+40]',
               with_meta(PB + '    移动 r0, 7\n    压栈 r0\n    减栈 0x28\n    取栈64 r1, [rsp+40]\n'
                          '    加栈 48\n    移64 r0, r1\n' + PT), FOLDPAT, 1))
MATRIX.append(('nh1 十六进制位移 减栈 0x28 与 [rsp+0x28]',
               with_meta(PB + '    移动 r0, 7\n    压栈 r0\n    减栈 0x28\n    取栈64 r1, [rsp+0x28]\n'
                          '    加栈 48\n    移64 r0, r1\n' + PT), FOLDPAT, 1))
# —— 符号后的空白必须是**空格与 Tab 都吞**；大写 `0X` 一律拒折（asm_zh NUM 只认小写 `0x`）——
# 预置值 9 写在「汇编器真正读的那一格」；压入值 7 在 A。误折会把 9 换成 7。
MATRIX.append(('nt0 减栈 0 与 [rsp+<TAB>40]（符号后 Tab，错配必拒）',
               with_meta(PB + '    移动 r2, 9\n    存栈64 [rsp+32], r2\n    移动 r0, 7\n    压栈 r0\n'
                          '    减栈 0\n    取栈64 r1, [rsp+\t40]\n    加栈 8\n    移64 r0, r1\n' + PT), FOLDPAT, 0))
MATRIX.append(('nt1 减栈 0 与 [rsp-<TAB>40]（负号后 Tab）',
               with_meta(PB + '    移动 r2, 9\n    存栈64 [rsp-48], r2\n    移动 r0, 7\n    压栈 r0\n'
                          '    减栈 0\n    取栈64 r1, [rsp-\t40]\n    加栈 8\n    移64 r0, r1\n' + PT), FOLDPAT, 0))
MATRIX.append(('nt2 减栈 -40 与 [rsp-<TAB>40]（正配对必须能解析 ⟹ 折）',
               with_meta(PB + '    移动 r0, 7\n    压栈 r0\n    减栈 -40\n    取栈64 r1, [rsp-\t40]\n'
                          '    减栈 32\n    移64 r0, r1\n' + PT), FOLDPAT, 1))
MATRIX.append(('nu0 减栈 0X28（大写 ⟹ 拒折）与 [rsp+40]',
               with_meta(PB + '    移动 r2, 9\n    存栈64 [rsp+32], r2\n    移动 r0, 7\n    压栈 r0\n'
                          '    减栈 0X28\n    取栈64 r1, [rsp+40]\n    加栈 8\n    移64 r0, r1\n' + PT), FOLDPAT, 0))
MATRIX.append(('nu1 减栈 40 与 [rsp+0X28]（大写 ⟹ 拒折）',
               with_meta(PB + '    移动 r2, 9\n    存栈64 [rsp-48], r2\n    移动 r0, 7\n    压栈 r0\n'
                          '    减栈 40\n    取栈64 r1, [rsp+0X28]\n    加栈 48\n    移64 r0, r1\n' + PT), FOLDPAT, 0))

# 无附录（⑮ 已不依赖结构层；结构闸只约束 LSP/跨块）—— 仍应命中，证明它是**局部等价变换**。
MATRIX.append(('nm0 无附录（⑮ 不依赖结构）',
               PB + '    移动 r0, 7\n    压栈 r0\n    减栈 40\n    取栈64 r0, [rsp+40]\n'
                    '    移64 r1, r0\n    加栈 48\n    移64 r0, r1\n' + PT, FOLDPAT, 1))
# ★ 评审第 8 条：callee 用正位移就能读到被压的那一格（调用者的 outgoing 区）。
#   旧版删掉 `压栈` ⟹ callee 读到残值（未优化 7 / 优化 0）；现在写者保留 ⟹ callee 读到同一个 7。
CALLEE = PB + '    移动 r0, 7\n    压栈 r0\n    减栈 40\n    取栈64 r0, [rsp+40]\n' \
              '    移64 r1, r0\n    调用 .Lget\n    加栈 48\n    移64 r0, r1\n' + PT + \
              '.Lget:\n    取栈64 r1, [rsp+48]\n    返回\n'
MATRIX.append(('ca0 callee 读 A（[rsp+48]）', with_meta(CALLEE), FOLDPAT, 1))
# —— 形状前提的负控（这些才是 ⑮ 真正该拒的）——
# 这四个负控件末尾统一 `移动 r0, 7`：程序行为确定（两链 exit 都是 7），断言只看「折叠=0」。
MATRIX.append(('nw0 回读宽度不是 64（取栈32）',
               with_meta(PB + '    移动 r0, 7\n    压栈 r0\n    减栈 40\n    取栈32 r1, [rsp+40]\n'
                          '    加栈 48\n    移动 r0, 7\n' + PT), FOLDPAT, 0))
MATRIX.append(('nd0 减栈不是立即数',
               with_meta(PB + '    移动 r0, 7\n    压栈 r0\n    移动 r3, 40\n    减栈 r3\n    取栈64 r0, [rsp+40]\n'
                          '    加栈 48\n    移动 r0, 7\n' + PT), FOLDPAT, 0))
MATRIX.append(('ns0 三行不连续（插 无操作）',
               with_meta(PB + '    移动 r0, 7\n    压栈 r0\n    无操作\n    减栈 40\n    取栈64 r0, [rsp+40]\n'
                          '    加栈 48\n    移动 r0, 7\n' + PT), FOLDPAT, 0))
MATRIX.append(('nx0 栈调整方向不对（加栈 顶替 减栈）',
               with_meta(PB + '    移动 r0, 7\n    压栈 r0\n    加栈 8\n    减栈 40\n    取栈64 r0, [rsp+40]\n'
                          '    加栈 40\n    移动 r0, 7\n' + PT), FOLDPAT, 0))
MATRIX.append(('bk0 ⑮ 块间存→取（应转发）', with_meta(XBLK_POS), XBLKPAT, 1))
MATRIX.append(('bk1 ⑮ 槽地址已逃逸（不许转发）', with_meta(XBLK_ADDR), XBLKPAT, 0))


# —— 基址族（R881O · 评审第 11 条）：`r4b`/`r4w`/`r4d` 在 asm 里基址只吃 `r4`、位移变 0；
#    我方必须认成 opaque（折叠=0）；而 `r4+40` 是闭表内规范拼写，必须**能折**（防「一律拒」恒绿）。
def _sent(sp):
    return with_meta(PB + '    移动 r2, 9\n    存栈64 [rsp-48], r2\n    移动 r0, 7\n    压栈 r0\n'
                         '    减栈 40\n    取栈64 r1, ' + sp + '\n    加栈 48\n    移64 r0, r1\n' + PT)


MATRIX.append(('nb1 基址 r4b（asm 位移变 0 ⟹ 必须拒）', _sent('[r4b+40]'), FOLDPAT, 0))
MATRIX.append(('nb2 基址 r4w（同上）', _sent('[r4w+40]'), FOLDPAT, 0))
MATRIX.append(('nb3 基址 r4d（同上）', _sent('[r4d+40]'), FOLDPAT, 0))
MATRIX.append(('nb4 基址 r8b（同上）', _sent('[r8b+40]'), FOLDPAT, 0))
MATRIX.append(('nbp 基址 r4 规范拼写（必须折）', _sent('[r4+40]'), FOLDPAT, 1))


def _qframe(push, dec, read, restore):
    """逗号位造件：哨兵铺满 [rsp-48 .. rsp+56]，每例自带配平收尾量（asm 对 `,40` 不减栈）。"""
    st = []
    v = 9
    for off in range(-48, 64, 8):
        st.append('    移动 r2, %d\n    存栈64 [rsp+%d], r2\n' % (v, off))
        v += 8
    body = ('    移动 r0, 7\n    ' + push + '\n    ' + dec + '\n    取栈64 r1, ' + read +
            '\n    ' + restore + '\n    移64 r0, r1\n')
    return with_meta(PB + ''.join(st) + body + PT)


# ★ 逗号位（R881Q · 评审第 12 条）：asm 的 SK/RG/NUM **从不跳逗号**，而 `元操` 会跳 ⟹ 曾经误折。
MATRIX.append(('nc0 减栈 ,40（asm NUM=0 ⟹ 必须拒）', _qframe('压栈 r0', '减栈 ,40', '[rsp+40]', '加栈 8'), FOLDPAT, 0))
MATRIX.append(('nc1 减栈 , 40（同上）', _qframe('压栈 r0', '减栈 , 40', '[rsp+40]', '加栈 8'), FOLDPAT, 0))
MATRIX.append(('nc2 压栈 ,r0（asm RG=-1 ⟹ 根本不压）', _qframe('压栈 ,r0', '减栈 40', '[rsp+40]', '加栈 40'), FOLDPAT, 0))
MATRIX.append(('nc3 减栈 40,（逗号在数值后 ⟹ 应折）', _qframe('压栈 r0', '减栈 40,', '[rsp+40]', '加栈 48'), FOLDPAT, 1))

# —— 逗号位在**其它 pass** 上的负控（R881R · 评审第 12 条的 gap）——
# `元操` 曾经把逗号当分隔符 ⟹ ⑫/⑪/⑤/⑭/LSP 会把 `压栈 ,r0` / `移64 ,r2, r3` 当成正常形状，
# 产出 `.删` 一条 NOEMIT 行 ⟹ 汇编器「.删/.增 实测长度非正」⟹ **优化产物不可汇编**。
# 现在 `元操` 与 asm 同构（按位置）⟹ 这些形状一律不命中；矩阵框架同时保证两链都**能汇编**、退出码/stdout 相同。
P12 = r'算术立即融合 (\d+)'
P11 = r'比较分支融合 (\d+)'
P05 = r'复制传播 (\d+)'
PLSP = r'LSP 槽=(\d+)'

MATRIX.append(('cm12 ⑫ 压栈逗号（asm NOEMIT ⟹ 不许融合）',
               '_入口:\n    压栈 ,r0\n    移动 r0, 5\n    弹栈 r3\n    加 r3, r0\n'
               '    移32 r0, r3\n    返回\n', P12, 0))
MATRIX.append(('cm11 ⑪ 压栈逗号（同上）',
               '_入口:\n    压栈 ,r0\n    移动 r0, 5\n    弹栈 r3\n    比较 r3, r0\n    置小 al\n'
               '    零扩展 eax, al\n    测试 r0, r0\n    为零跳 .Lend\n    移动 r0, 1\n.Lend:\n    返回\n',
               P11, 0))
MATRIX.append(('cm05 ⑤ 移64 逗号（第一操作数位是逗号）',
               '_入口:\n    移64 ,r2, r3\n    移64 r4, r2\n    返回\n', P05, 0))
XBLK_CM = '_入口:\n    移64 r5, r4\n    减栈 64\n    移动 r2, 7\n    存帧32 [rbp-8], ,r2\n'
XBLK_CM += '.Lb:\n    取帧32 r0, [rbp-8]\n    加栈 64\n    返回\n'
MATRIX.append(('cm14 ⑭ 源寄存器位是逗号（不许转发）', with_meta(XBLK_CM), XBLKPAT, 0))
LSP_CM = '_入口:\n    压栈 r5\n    压栈 r3\n    移64 r5, r4\n    减栈 4416\n    移动 r0, 0\n'
LSP_CM += '    存帧32 [rbp-4400], r0\n.L环路:\n    取帧32 ,r0, [rbp-4400]\n    比较 r0, 100\n'
LSP_CM += '    大于等跳 .L出口\n    取帧32 r0, [rbp-4400]\n    加 r0, 1\n    存帧32 [rbp-4400], r0\n'
LSP_CM += '    跳转 .L环路\n.L出口:\n    取帧32 r0, [rbp-4400]\n    加栈 4416\n    弹栈 r3\n    弹栈 r5\n    返回\n'
MATRIX.append(('cmL LSP 取帧目的位是逗号（该槽不许提升）', with_meta(LSP_CM), PLSP, 0))

def matrix_cells():
    return MATRIX


print('== 逐条 pass 精确对账（出厂 opt_zh → 出厂 asm_zh；粒度 = 该类）==')
print('  %-24s %6s %10s %10s %10s %s' % ('pass 类', '条数', '自报缩短', '.text 差', '其它类', '判定'))
fails = []
unknown_cls = set()
tot = 0
for name, body, meta, target, *rest in CASES:
    also = tuple(rest[0]) if rest else ()
    p = os.path.join(W, 'c.asm')
    txt = with_meta('_入口:\n' + body) if meta == '@AUTO' else '_入口:\n' + body + (meta or '')
    io.open(p, 'w', encoding='utf-8', newline='\n').write(txt)
    po = os.path.join(W, 'c_o.asm')
    rc1, o1 = sh([OPT, p, '-o', po])
    rc2, o2 = sh([ASM, p, '-o', os.path.join(W, 'c_n.exe')])       # 不加优化链
    rc3, o3 = sh([ASM, po, '-o', os.path.join(W, 'c_o.exe')])      # 优化链
    if rc1 or rc2 or rc3:
        fails.append('%s: 链路 rc=%d/%d/%d %s' % (name, rc1, rc2, rc3, o3.strip()[:80]))
        print('  %-24s ★链路失败' % name)
        continue
    m = re.search(r'优化 \d+ 处 \[([^\]]*)\]', o1)
    seg = m.group(1) if m else ''
    # 登记闸: 收尾行里出现的类名，必须都已在 CLASS_KEYS 登记（归一化: 去空格）
    known = set(k.replace(' ', '') for k, _ in CLASS_KEYS)
    for nm in unknown_class_names(seg, known):
        unknown_cls.add(nm)
    cls = {k: (int(re.search(pat, seg).group(1)) if re.search(pat, seg) else None) for k, pat in CLASS_KEYS}
    mz = re.search(r'总缩=(-?\d+)', o3)
    tz = int(mz.group(1)) if mz else -1
    mb = re.search(r'G-DCE 删字节=(-?\d+)', o1)
    selfb = int(mb.group(1)) if mb else None
    ml = re.search(r'LSP 槽=-?\d+ 净减字节=(-?\d+)', o1)
    selfl = int(ml.group(1)) if ml else None
    mx = re.search(r'\[跨块\] 存→取 转发 条=\d+ 净减字节=(-?\d+)', o1)
    selfx = int(mx.group(1)) if mx else None
    dn, do = ts(os.path.join(W, 'c_n.exe')), ts(os.path.join(W, 'c_o.exe'))
    delta = dn - do
    mine = cls.get(target) or 0
    if target == '循环槽提升':
        mm1 = re.search(r'LSP 槽=(\d+)', o1)
        mine = int(mm1.group(1)) if mm1 else 0
    also_hits = {k: (cls.get(k) or 0) for k in also}
    allow = {target} | set(also)
    others = {k: v for k, v in cls.items() if k not in allow and v}
    ok = (mine > 0) and all(v > 0 for v in also_hits.values()) and (not others) \
        and (delta == tz) and (tz > 0)
    if target == 'G-DCE':
        ok = ok and (selfb == delta)
    if target == '循环槽提升':
        # 循环级 pass 的字节可增可减（帧槽提升 = 2 条插入 + 若干「取/存 → 移」的缩短）⟹
        # 判据不是 `总缩>0`，而是 **自报 `净减字节` == `.text` 差**（模型漂移当场打红）
        ok = (mine > 0) and (not others) and (selfl is not None) and (delta == selfl)
    if target == '跨块转发':
        ok = (mine > 0) and (not others) and (selfx is not None) and (delta == selfx)
    print('  %-24s %6d %10d %10d %10s %s%s'
          % (name, mine, tz, delta, others if others else '{}', '✓' if ok else '★FAIL',
             ('  子计数=%s' % also_hits) if also else ''))
    if target == '循环槽提升':
        print('  %-24s %6s %10s %10d %10s'
              % ('  ↳ 该类的自报净减字节', '', selfl, delta, '一致' if delta == selfl else '漂移'))
    if target == '跨块转发':
        print('  %-24s %6s %10s %10d %10s'
              % ('  ↳ 该类的自报净减字节', '', selfx, delta, '一致' if delta == selfx else '漂移'))
    if not ok:
        fails.append('%s: 条数=%d 子计数=%s 自报=%s 实测=%d 差值=%d 其它类=%s'
                     % (name, mine, also_hits, selfb, tz, delta, others))
    tot += tz
print()
print('  逐类自报缩短合计 = %d 字节' % tot)
print('  （每类都做过「差值 == 自报量」，且优化器侧已无任何未实测的手工字节常量：'
      '①–⑥ 的原 `.缩 N` 常量已全部改为 `.删`/`.替` 声明，由汇编器干跑实测）')
for nm in sorted(unknown_cls):
    fails.append('登记闸: 收尾行出现未登记类名 %r（必须先在 CLASS_KEYS 登记并给最小触发件）' % nm)

# ---- 登记闸 负控：喂一条**含未登记类名**的收尾行，必须恰好报出该名字（闸不是恒绿） ----
_known = set(k.replace(' ', '') for k, _ in CLASS_KEYS)
_fake_seg = '栈spill 1 / 假类X 3 / 压栈回读 2'
_got = unknown_class_names(_fake_seg, _known)
_ok_neg = (_got == ['假类X'])
print('  [自检] 登记闸负控：喂 `%s` ⟹ 报出 %s ⟹ %s'
      % (_fake_seg, _got, 'RED（正确）' if _ok_neg else '★恒绿（闸坏了）'))
if not _ok_neg:
    fails.append('登记闸负控: 含未登记类名的收尾行没有被抓出来（闸恒绿）')
# ---- 可执行回归：真跑两链比退出码（捕捉「字节对得上但语义被改」）----
print()
print('  == 可执行回归（真跑两链比退出码）==')
for idx, (name, code, pat, want) in enumerate(matrix_cells()):
    p = os.path.join(W, 'x_%d.asm' % idx)
    po = os.path.join(W, 'x_%d_o.asm' % idx)
    io.open(p, 'w', encoding='utf-8', newline='\n').write(code)
    rc1, o1 = sh([OPT, p, '-o', po])
    rc2, o2 = sh([ASM, p, '-o', os.path.join(W, 'x_%d_n.exe' % idx)])
    rc3, o3 = sh([ASM, po, '-o', os.path.join(W, 'x_%d_o.exe' % idx)])
    if rc1 or rc2 or rc3:
        fails.append('%s: 链路 rc=%d/%d/%d' % (name, rc1, rc2, rc3))
        print('  %-26s ★链路失败 %d/%d/%d' % (name, rc1, rc2, rc3))
        continue
    m = re.search(pat, o1)
    hits = int(m.group(1)) if m else 0
    # ★ 写者保全闸（R881L 的核心不变式）：⑮ 命中的产物里**绝不许出现 `.删 压栈`**。
    #   历史：R881b–R881k 七轮打回都出在「删掉写者之后要证明没人读那一格」—— 那条证明做不干净
    #   （callee 用正位移就能读到调用者的 outgoing 区）。现在写者一律保留，本闸把它钉死。
    if pat == FOLDPAT and hits > 0:
        _po_txt = io.open(po, encoding='utf-8', errors='replace').read()
        _delp = [l for l in _po_txt.split('\n') if l.startswith('.删') and '压栈' in l]
        if _delp:
            fails.append('%s: ⑮ 命中了却删掉写者 %r ⟹ 违反「永不删内存写」' % (name, _delp[:1]))
            print('  %-32s ★删写者 %r' % (name, _delp[:1]))
    p1 = subprocess.run([os.path.join(W, 'x_%d_n.exe' % idx)], capture_output=True, timeout=60)
    p2 = subprocess.run([os.path.join(W, 'x_%d_o.exe' % idx)], capture_output=True, timeout=60)
    e1, e2 = p1.returncode, p2.returncode
    o1x, o2x = p1.stdout, p2.stdout
    if e1 != e2:      # 瞬时抖动复核：两链各再跑一次（真破坏是确定性的）
        e1 = run_exit(os.path.join(W, 'x_%d_n.exe' % idx), e2)
        e2 = run_exit(os.path.join(W, 'x_%d_o.exe' % idx), e1)
    ok = (hits == want) and (e1 == e2) and (o1x == o2x)
    print('  %-32s 折叠=%d（期望 %d）· 两链 exit=%d/%d · stdout 相同=%s · %s'
          % (name, hits, want, e1, e2, o1x == o2x, '✓' if ok else '★FAIL'))
    if hits != want:
        fails.append('%s: 折叠 %d 条（期望 %d）' % (name, hits, want))
    if e1 != e2:
        fails.append('%s: 两链退出码不同（%d vs %d）⟹ 语义被改' % (name, e1, e2))

# ---- 生成式轴测（R881O）：把「拼写覆盖」从人挑格子改成**文法轴的积** ----
# 为什么：前几轮每轮漏一个「没被想到的拼写」（`r4b` 就是这轮才漏的）。根因是 asm 的 RG 按**前缀**消费、
# 而优化器曾按「整 token」判基址。现在优化器侧改成**闭表 + fail-closed**（表外 opaque），
# 判据侧则按轴生成、用**汇编器实测**当真值（哨兵帧），期望不写死。
AX_BASES = ['rsp', 'r4', 'esp', 'sp', 'rbp', 'r4b', 'r4w', 'r4d', 'r8b', 'rX', 'rspx', 'r4_']
AX_SIGNS = ['+', '-']
AX_DISPS = ['40', '040', '0x28', '0X28', '40junk', '']
AX_TAILS = [']', ' ]', ']x']
AX_CANON = {('rsp', '+', '40'), ('r4', '+', '40'), ('esp', '+', '40')}   # 闭表内 + 规范位移
AX_CLOSED = {'rsp', 'r4', 'esp'}                                          # 优化器侧闭表（登记闸用）


def _axis_frame(operand):
    """哨兵帧：rsp=B 时写 B-48（位移 0 处，哨兵 9）与 B+8..B+56（17,25,…）；A=[rsp+40] 为压入的 7。"""
    head = PB
    st = ['    移动 r2, 9\n    存栈64 [rsp-48], r2\n']
    k = 1
    for off in range(8, 64, 8):
        st.append('    移动 r2, %d\n    存栈64 [rsp+%d], r2\n' % (9 + 8 * k, off))
        k += 1
    body = ('    移动 r0, 7\n    压栈 r0\n    减栈 40\n    取栈64 r1, ' + operand +
            '\n    加栈 48\n    移64 r0, r1\n')
    return with_meta(head + ''.join(st) + body + PT)


AX_COMMA = [
    # (名字, push 行, 减栈 行, 取栈 行, 该例的收尾量, 期望折叠)
    ('逗号: 减栈 ,40', '压栈 r0', '减栈 ,40', '[rsp+40]', '加栈 8', 0),
    ('逗号: 减栈 , 40', '压栈 r0', '减栈 , 40', '[rsp+40]', '加栈 8', 0),
    ('逗号: 压栈 ,r0', '压栈 ,r0', '减栈 40', '[rsp+40]', '加栈 40', 0),
    ('逗号: 减栈 40,（数值后）', '压栈 r0', '减栈 40,', '[rsp+40]', '加栈 48', 1),
]


def _axis_cases():
    out = []
    for b in AX_BASES:
        for sg in AX_SIGNS:
            for d in AX_DISPS:
                out.append(('[%s%s%s]' % (b, sg, d), b, sg, d))
    # 尾部变体（` ]` / `]x`）
    for b in ('rsp', 'r4', 'r4b'):
        for t in AX_TAILS[1:]:
            out.append(('[%s+40%s' % (b, t), b, '+', '40'))
    return out


print()
print('  == 生成式轴测（基址 × 符号 × 位移 × 尾；哨兵帧 ⟹ 汇编器实测当真值）==')
_ax_n = 0
_ax_skip = 0
_ax_bad = 0
for _i, (_op, _b, _sg, _d) in enumerate(_axis_cases()):
    _p = os.path.join(W, 'ax_%03d.asm' % _i)
    io.open(_p, 'w', encoding='utf-8', newline='\n').write(_axis_frame(_op))
    _rc1, _o1 = sh([OPT, _p, '-o', _p + '.o.asm'])
    _rc2, _o2 = sh([ASM, _p, '-o', _p + '.n.exe'])
    _rc3, _o3 = sh([ASM, _p + '.o.asm', '-o', _p + '.o.exe'])
    if _rc1 or _rc2 or _rc3:
        _ax_skip += 1
        continue
    _ax_n += 1
    _e1 = subprocess.run([_p + '.n.exe'], capture_output=True, timeout=60).returncode & 0xFFFFFFFF
    _e2 = subprocess.run([_p + '.o.exe'], capture_output=True, timeout=60).returncode & 0xFFFFFFFF
    if _e1 != _e2:    # 瞬时抖动复核
        _e1 = run_exit(_p + '.n.exe', _e2)
        _e2 = run_exit(_p + '.o.exe', _e1)
    _m = re.search(FOLDPAT, _o1)
    _f = int(_m.group(1)) if _m else 0
    _ok = (_e1 == _e2)
    _note = ''
    if (_b, _sg, _d) in AX_CANON and _op.endswith(']'):
        if _f != 1:
            _ok = False
            _note = '· ★规范拼写应折（闭表内）'
    else:
        if _b not in AX_CLOSED and _sg == '+' and _d == '40' and _f != 0:
            _ok = False
            _note = '· ★闭表外不许折'
    if not _ok:
        _ax_bad += 1
        fails.append('轴测 %s: 折叠=%d 未优化=%d 优化=%d %s' % (_op, _f, _e1, _e2, _note))
        print('  %-16s 折叠=%d · exit=%d/%d %s' % (_op, _f, _e1, _e2, '★FAIL ' + _note))
for _i, (_nm, _pu, _de, _rd, _rs, _want) in enumerate(AX_COMMA):
    _p = os.path.join(W, 'cm_%02d.asm' % _i)
    io.open(_p, 'w', encoding='utf-8', newline='\n').write(_qframe(_pu, _de, _rd, _rs))
    _rc, _o = sh([OPT, _p, '-o', _p + '.o.asm'])
    _rc2, _ = sh([ASM, _p, '-o', _p + '.n.exe'])
    _rc3, _ = sh([ASM, _p + '.o.asm', '-o', _p + '.o.exe'])
    _m = re.search(FOLDPAT, _o)
    _f = int(_m.group(1)) if _m else 0
    if _rc or _rc2 or _rc3:
        _ax_skip += 1
        fails.append('逗号轴 %s: 链路 rc=%d/%d/%d' % (_nm, _rc, _rc2, _rc3))
        continue
    _e1 = subprocess.run([_p + '.n.exe'], capture_output=True, timeout=60).returncode & 0xFFFFFFFF
    _e2 = subprocess.run([_p + '.o.exe'], capture_output=True, timeout=60).returncode & 0xFFFFFFFF
    if _e1 != _e2:    # 瞬时抖动复核
        _e1 = run_exit(_p + '.n.exe', _e2)
        _e2 = run_exit(_p + '.o.exe', _e1)
    _ok = (_f == _want) and (_e1 == _e2)
    if not _ok:
        _ax_bad += 1
        fails.append('逗号轴 %s: 折叠=%d（期望 %d）· exit=%d/%d' % (_nm, _f, _want, _e1, _e2))
    print('  %-24s 折叠=%d（期望 %d）· exit=%d/%d %s'
          % (_nm, _f, _want, _e1, _e2, '✓' if _ok else '★FAIL'))
print('  ↳ 轴测 %d 格 + 逗号轴 %d 格（组装失败/跳过 %d）⟹ %s'
      % (_ax_n, len(AX_COMMA), _ax_skip, '全过' if _ax_bad == 0 else '红 %d' % _ax_bad))
if _ax_n < 100:
    fails.append('轴测只组装出 %d 格（<100）⟹ 生成器或造件坏了' % _ax_n)

# ---- 结构终止闸：块终止子必须**认全**（表在 asm_zh；判据在 opt_zh 的 `控流令 / 野控流`）----
# 造件：`存帧32 [rbp-8], r2` 之后插一条控流令，再跟 `取帧32 r0,[rbp-8]`（⑭ 的转发候选）。
#   ① 该控流令**必须把块断开** ⟹ `[结构] 块=` 比基准 +1（`无操作` 作正控必须 +0）；
#   ② 目标**不可静态确定**的控流（野控流：间跳/环跳/环等跳/环不等跳/中断/中断返/陷阱）⟹ ⑭ 转发必须 0；
#   ③ 条件跳 ⟹ ⑭ **应当**转发（边的目标是已知标签、落空边到 b）⟹ 1，证明这道闸不是「一律拒」。
GATE_HEAD = PB + '    移动 r2, 7\n    存帧32 [rbp-8], r2\n'
GATE_TAIL = '    取帧32 r0, [rbp-8]\n    加栈 48\n' + PT + '.Lx:\n    返回\n'
CTRL_MNS = ['跳转 .Lx', '跳转短 .Lx', '为零跳 .Lx', '非零跳 .Lx', '负跳 .Lx', '非负跳 .Lx',
            '大跳 .Lx', '小跳 .Lx', '大于等跳 .Lx', '小于等跳 .Lx', '高于跳 .Lx', '高于等跳 .Lx',
            '低于跳 .Lx', '奇跳 .Lx', '偶跳 .Lx', '溢跳 .Lx', '不溢跳 .Lx', '非溢出跳 .Lx',
            '间跳 r3', '环跳 .Lx', '环等跳 .Lx', '环不等跳 .Lx', '中断 3', '中断返', '陷阱', '返回']
WILD = ['间跳 r3', '环跳 .Lx', '环等跳 .Lx', '环不等跳 .Lx', '中断 3', '中断返', '陷阱', '返回']
# 调用族：**不**终止块（会落空），但 ⑭ 跨块转必须因「有调用」拒绝转发（rX 可能被 callee 踩、也可能改槽）
CALLMNS = ['调用 .Lx', '调系统 95']
# 无条件跳转：它的边直达 .Lx，块 b（`取帧`）**没有前驱** ⟹ ⑭ 必然不转发（与野控流不同，但结果一样是 0）
UNCOND = ['跳转 .Lx', '跳转短 .Lx']


def _gate_cell(mn):
    return with_meta(GATE_HEAD + ('    ' + mn + '\n' if mn else '') + GATE_TAIL)


def _blocks(out):
    m = re.search(r'\[结构\] 块=(\d+) 边=(\d+)', out)
    return (int(m.group(1)), int(m.group(2))) if m else (-1, -1)


print()
print('  == 结构终止闸（块终止子认全 + 野控流不许跨块转发）==')
_p0 = os.path.join(W, 'g_base.asm')
io.open(_p0, 'w', encoding='utf-8', newline='\n').write(_gate_cell(''))
_rc0, _o0 = sh([OPT, _p0, '-o', _p0 + '.o.asm'])
_b0 = _blocks(_o0)
_m0 = re.search(XBLKPAT, _o0)
_f0 = int(_m0.group(1)) if _m0 else 0
print('  %-14s 块=%d · 转发=%d（基准：不插控流令）' % ('基准', _b0[0], _f0))
_gbad = 0
for _i, _mn in enumerate(['无操作'] + CTRL_MNS + CALLMNS):
    _p = os.path.join(W, 'g_%d.asm' % _i)
    io.open(_p, 'w', encoding='utf-8', newline='\n').write(_gate_cell(_mn))
    _rc, _out = sh([OPT, _p, '-o', _p + '.o.asm'])
    _bl = _blocks(_out)
    _m = re.search(XBLKPAT, _out)
    _fw = int(_m.group(1)) if _m else 0
    _want_blk = _b0[0] + 1 if _mn in CTRL_MNS else _b0[0]
    _want_fw = 0 if (_mn == '无操作' or _mn in WILD or _mn in UNCOND or _mn in CALLMNS) else 1
    _ok = (_rc == 0) and (_bl[0] == _want_blk) and (_fw == _want_fw)
    if not _ok:
        _gbad += 1
        fails.append('结构终止闸 %s: rc=%d 块=%d(期望 %d) 转发=%d(期望 %d)'
                     % (_mn, _rc, _bl[0], _want_blk, _fw, _want_fw))
    print('  %-14s 块=%d（期望 %d）· 转发=%d（期望 %d）· rc=%d · %s'
          % (_mn, _bl[0], _want_blk, _fw, _want_fw, _rc, '✓' if _ok else '★FAIL'))
print('  ↳ 结构终止闸 %d 条（控流 %d + 调用族 %d）+ 正控 1 条 ⟹ %s'
      % (len(CTRL_MNS) + len(CALLMNS), len(CTRL_MNS), len(CALLMNS),
         '全过' if _gbad == 0 else '红 %d' % _gbad))

print('== 结果: %s ==' % ('逐条全过' if not fails else 'FAIL=%d' % len(fails)))
for f in fails:
    print('   -', f)
sys.exit(0 if not fails else 1)
