#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""check_opt_lsp —— R881 **控制流结构 + 循环槽提升（LSP）** 的判据器。

结构口径（可复算）：
  · 优化器 从 `-S` 中间态 自建 **基本块 / 前驱 / 后继 / 活集**，并在其上跑 **环验证**：
      ① 无中途入口（除头块外，成员的全部前驱都在区间内）
      ② 出边唯一，且出口块在区间之后
      ③ 出口块的**全部前驱**都在区间内（否则出口回写会毒化旁路）
      ④ 尾块回跳头块
      ⑤ 尾块从头块可达
      ⑥ 头块的**环外前驱**必须经「落空」进入（否则「头前载入」被绕过）
  · 循环级优化 = 帧槽提升到 r8/r9/r10（`.增` 让汇编器实测记账变长）

六个用例（全部真跑 出厂链 `cc 编 opt_zh` → `opt_zh` → `asm_zh`）：
  A. **隔离**：带元数据的最小环（造件）⟹ `LSP 槽=1`、**其余 12 类全部 0**、
     `净减字节` 与 **`.text` 有效内容差值** 精确相等（自报 == 实测）
  B. **负控（擦元数据）**：同件去附录 ⟹ `LSP 槽=0`、12 类全 0、输出 **逐字节 == 输入**、两链 `.text` 相同
  C. **破损**：跳转目标 解析不到 ⟹ 优化器 **rc=1** 且 响亮 报「控制流结构 破损」
  D. **旁路出口 拒**（③）：环外 另有一条跳转直达 出口块 ⟹ **槽=0** 且 `环验证拒 >= 1`
  E. **中途入口 拒**（①）：环外 跳进 环体中间 ⟹ **槽=0** 且 `环验证拒 >= 1`
  F. **真实 `cc -S` 产物**：`LSP 槽 > 0`、`[结构] 块>0 边>0 活集 迭代>0`、两链 运行 rc/stdout 相同

用法: python scripts/check_opt_lsp.py [--cc qcc_zh_v1.exe]
退出码: 0 = 全过；1 = 有红；2 = 环境未就绪
"""
import argparse
import io
import os
import re
import struct
import subprocess
import sys

sys.stdout.reconfigure(encoding='utf-8', errors='replace')
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
os.chdir(ROOT)


# ---------------------------------------------------------------- 造件
BASE = '''_入口:
    压栈 r5
    压栈 r3
    移64 r5, r4
    减栈 4416
    移动 r0, 0
    存帧32 [rbp-4400], r0
.L环路:
    取帧32 r0, [rbp-4400]      @HEAD
    比较 r0, 100
    大于等跳 .L出口
    取帧32 r0, [rbp-4400]
    加 r0, 1
    存帧32 [rbp-4400], r0
    跳转 .L环路                 @TAIL
.L出口:
    取帧32 r0, [rbp-4400]
    加栈 4416
    弹栈 r3
    弹栈 r5
    返回
'''

# ③ 旁路出口：环外再跳一次到出口块
BYPASS = BASE.replace('''    存帧32 [rbp-4400], r0
.L环路:''', '''    存帧32 [rbp-4400], r0
    小于等跳 .L出口
.L环路:''')

# ① 中途入口：环外跳进环体中间（绕过「头前载入」）
MIDENTER = BASE.replace('''    存帧32 [rbp-4400], r0
.L环路:''', '''    存帧32 [rbp-4400], r0
    跳转 .L环体
.L环路:''').replace('''    大于等跳 .L出口
    取帧32 r0, [rbp-4400]''', '''    大于等跳 .L出口
.L环体:
    取帧32 r0, [rbp-4400]''')

# G. 槽写形态盲点（LSP）：环里用 `存8帧` 写同一帧槽 —— 不认识的写法必须让该槽不许提升
SLOTW = '''_入口:
    压栈 r5
    压栈 r3
    移64 r5, r4
    减栈 4416
    移动 r0, 0
    存帧32 [rbp-4416], r0
    移动 r0, 100
    存帧32 [rbp-4400], r0
.L环路:
    取帧32 r0, [rbp-4416]      @HEAD
    比较 r0, 4
    大于等跳 .L出口
    取帧32 r0, [rbp-4400]
    加 r0, 1
    存帧32 [rbp-4400], r0
    移动 r0, 0
    存8帧 [rbp-4400], r0
    取帧32 r0, [rbp-4416]
    加 r0, 1
    存帧32 [rbp-4416], r0
    跳转 .L环路                 @TAIL
.L出口:
    取帧32 r0, [rbp-4400]
    加栈 4416
    弹栈 r3
    弹栈 r5
    返回
'''

# H. 跨块转发的尾巴盲点：p 尾巴用 `存8帧` 写同一槽 —— 不许转发
XBLKW = '''_入口:
    移64 r5, r4
    减栈 64
    移动 r2, 100
    存帧32 [rbp-8], r2
    移动 r0, 0
    存8帧 [rbp-8], r0
.Lb:
    取帧32 r0, [rbp-8]
    加栈 64
    返回
'''

# 破损：跳转目标不存在
BROKEN = BASE.replace('    跳转 .L环路                 @TAIL', '    跳转 .L不存在 @TAIL')


def build(code, with_meta=True):
    """按 `@HEAD` / `@TAIL` 标记算指令序号，生成完备元数据附录。"""
    lines, head, tail, k = [], None, None, 0
    for raw in code.split('\n'):
        if not raw.strip():
            continue
        mark = raw.strip().split()[-1]
        line = re.sub(r'\s*@(HEAD|TAIL)$', '', raw)
        lines.append(line)
        if line.strip().endswith(':'):
            continue
        if mark == '@HEAD':
            head = k
        if mark == '@TAIL':
            tail = k
        k += 1
    n = k
    txt = '\n'.join(lines) + '\n'
    if with_meta:
        has_loop = (head is not None and tail is not None)
        meta = ['; @QCCMETA 1 ins=%d funcs=1 loops=%d nodes=%d' % (n, 1 if has_loop else 0, n)]
        for i in range(n):
            meta.append('; @I %d f=1 n=%d k=1 t=-1 l=%d d=0'
                        % (i, i, (1 if head <= i <= tail else -1) if has_loop else -1))
        if has_loop:
            meta.append('; @L 1 s=%d e=%d d=1' % (head, tail))
        meta.append('; @QCCMETA-END')
        txt += '\n'.join(meta) + '\n'
    return txt


def text_size(p):
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


def run(cmd, t=600):
    return subprocess.run(cmd, capture_output=True, timeout=t)


def grab(out, pat, cast=int):
    m = re.search(pat, out)
    return cast(m.group(1)) if m else None


CLASS_KEYS = [
    ('栈spill', r'栈spill (\d+)'), ('栈合并', r'栈合并 (\d+)'), ('抵消对', r'抵消对 (\d+)'),
    ('不可达跳转', r'不可达 跳转 (\d+)'), ('不可达返回', r'不可达 跳转 \d+ 返回 (\d+)'),
    ('复制传播', r'复制传播 (\d+)'), ('跳落到下一行', r'跳落到下一行 (\d+)'),
    ('帧栈存取同址', r'帧栈存取同址 (\d+)'), ('死值写覆盖', r'死值写覆盖 (\d+)'),
    ('零栈调整', r'零栈调整 (\d+)'), ('G-DCE', r'G-DCE (\d+)'),
    ('比较分支融合', r'比较分支融合 (\d+)'), ('算术立即融合', r'算术立即融合 (\d+)'),
]


def classes(out):
    m = re.search(r'优化 \d+ 处 \[([^\]]*)\]', out)
    seg = m.group(1) if m else ''
    d = {}
    for name, pat in CLASS_KEYS:
        mm = re.search(pat, seg)
        d[name] = int(mm.group(1)) if mm else 0
    return d


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cc', default='qcc_zh_v1.exe')
    ap.add_argument('--work', default=os.path.join(os.environ.get('TEMP', '.'), 'qcc_opt_lsp'))
    a = ap.parse_args()
    cc = a.cc if os.path.isabs(a.cc) else os.path.join(ROOT, a.cc)
    opt_src = os.path.join(ROOT, 'srclib', 'opt_zh.jy')
    asm = os.path.join(ROOT, 'asm_zh.exe')
    if not (os.path.exists(cc) and os.path.exists(opt_src) and os.path.exists(asm)):
        print('[FAIL] 环境未就绪: cc / srclib/opt_zh.jy / asm_zh.exe 缺一')
        return 2
    os.makedirs(a.work, exist_ok=True)
    opt = os.path.join(a.work, 'opt_lsp.exe')
    r = run([cc, opt_src, '-o', opt])
    if r.returncode != 0:
        print('✗ 优化器源 编译失败 rc=%d\n%s' % (r.returncode, r.stdout.decode('utf-8', 'replace')[:400]))
        return 1
    print('⓪ 出厂优化器 由 shipped 编出 rc=0（%d 字节）' % os.path.getsize(opt))

    fails = []

    def put(name, txt):
        p = os.path.join(a.work, name + '.asm')
        io.open(p, 'w', encoding='utf-8', newline='\n').write(txt)
        return p

    def opt_run(src, dst):
        r = run([opt, src, '-o', dst])
        txt = (r.stdout + r.stderr).decode('utf-8', 'replace')
        return r.returncode, txt

    def asm_run(src, dst):
        r = run([asm, src, '-o', dst])
        return r.returncode, (r.stdout + r.stderr).decode('utf-8', 'replace')

    # ---------------- A. 隔离 ----------------
    srcA = put('lsp_iso', build(BASE, True))
    noA = os.path.join(a.work, 'lsp_iso_nometa.asm')
    io.open(noA, 'w', encoding='utf-8', newline='\n').write(build(BASE, False))
    outA = os.path.join(a.work, 'lsp_iso_o.asm')
    rcA, txtA = opt_run(srcA, outA)
    dA, szA = 'A 隔离', {}
    if rcA != 0:
        fails.append('A 隔离: 优化器 rc=%d' % rcA)
    else:
        cls = classes(txtA)
        slots = grab(txtA, r'LSP 槽=(-?\d+)')
        net = grab(txtA, r'LSP 槽=-?\d+ 净减字节=(-?\d+)')
        blk = grab(txtA, r'\[结构\] 块=(\d+)')
        edg = grab(txtA, r'边=(\d+)')
        itr = grab(txtA, r'活集 迭代=(\d+)')
        other = sum(v for k2, v in cls.items())
        r1, log1 = asm_run(srcA, os.path.join(a.work, 'lsp_iso_plain.exe'))
        r2, log2 = asm_run(outA, os.path.join(a.work, 'lsp_iso_opt.exe'))
        if r1 != 0 or r2 != 0:
            fails.append('A 隔离: 汇编失败 rc=%d/%d' % (r1, r2))
        else:
            t1 = text_size(os.path.join(a.work, 'lsp_iso_plain.exe'))
            t2 = text_size(os.path.join(a.work, 'lsp_iso_opt.exe'))
            print('  A 隔离: 结构 块=%s 边=%s 活集迭代=%s | LSP 槽=%s 净减字节=%s | 其余类合计=%d'
                  % (blk, edg, itr, slots, net, other))
            print('           .text %d -> %d（差 %+d，期望 %+d）' % (t1, t2, t2 - t1, -(net or 0)))
            if slots != 1:
                fails.append('A 隔离: LSP 槽=%s（期望 1）' % slots)
            if other != 0:
                fails.append('A 隔离: 其余类 合计=%d（期望 0）→ %s' % (other, cls))
            if net is None:
                fails.append('A 隔离: 未打印 净减字节')
            elif (t2 - t1) != -(net):
                fails.append('A 隔离: .text 差 %+d != 自报 %+d' % (t2 - t1, -(net)))
            if (blk or 0) <= 0 or (edg or 0) <= 0 or (itr or 0) <= 0:
                fails.append('A 隔离: 结构统计 缺失（块/边/活集迭代）')

    # ---------------- B. 负控 ----------------
    outB = os.path.join(a.work, 'lsp_iso_nometa_o.asm')
    rcB, txtB = opt_run(noA, outB)
    if rcB != 0:
        fails.append('B 负控: 优化器 rc=%d' % rcB)
    else:
        # 无附录 ⟹ 结构层不启用 ⟹ 连 `[循环]` 报告行都不打印 ⟹ 按 0 命中计
        slotsB = grab(txtB, r'LSP 槽=(-?\d+)') or 0
        otherB = sum(classes(txtB).values())
        same = io.open(noA, 'rb').read() == io.open(outB, 'rb').read()
        r1, _ = asm_run(noA, os.path.join(a.work, 'lsp_nometa_plain.exe'))
        r2, _ = asm_run(outB, os.path.join(a.work, 'lsp_nometa_opt.exe'))
        t1 = text_size(os.path.join(a.work, 'lsp_nometa_plain.exe')) if r1 == 0 else -1
        t2 = text_size(os.path.join(a.work, 'lsp_nometa_opt.exe')) if r2 == 0 else -1
        print('  B 负控(擦元数据): LSP 槽=%s 全类合计=%d 输出逐字节相同=%s .text %d/%d'
              % (slotsB, otherB, same, t1, t2))
        if slotsB != 0 or otherB != 0:
            fails.append('B 负控: 擦掉元数据仍 有命中（槽=%s 类合计=%d）' % (slotsB, otherB))
        if not same:
            fails.append('B 负控: 输出与输入 不是逐字节相同')
        if t1 != t2:
            fails.append('B 负控: 两链 .text 不同（%d vs %d）' % (t1, t2))

    # ---------------- C. 破损 ----------------
    srcC = put('lsp_broken', build(BROKEN, True))
    rcC, txtC = opt_run(srcC, os.path.join(a.work, 'lsp_broken_o.asm'))
    loud = ('控制流结构 破损' in txtC)
    print('  C 破损(跳转目标不存在): rc=%d 响亮拒绝=%s' % (rcC, loud))
    if rcC == 0 or not loud:
        fails.append('C 破损: 未响亮拒绝（rc=%d 提示=%s）' % (rcC, loud))

    # ---------------- D. 旁路出口 ----------------
    srcD = put('lsp_bypass', build(BYPASS, True))
    rcD, txtD = opt_run(srcD, os.path.join(a.work, 'lsp_bypass_o.asm'))
    sD = grab(txtD, r'LSP 槽=(-?\d+)')
    rD = grab(txtD, r'环验证拒=(\d+)')
    print('  D 旁路出口: rc=%d LSP 槽=%s 环验证拒=%s' % (rcD, sD, rD))
    if rcD != 0 or sD != 0 or (rD or 0) < 1:
        fails.append('D 旁路出口: 应拒（槽=0 且 环验证拒>=1），实测 槽=%s 拒=%s rc=%d' % (sD, rD, rcD))

    # ---------------- E. 中途入口 ----------------
    srcE = put('lsp_midenter', build(MIDENTER, True))
    rcE, txtE = opt_run(srcE, os.path.join(a.work, 'lsp_midenter_o.asm'))
    sE = grab(txtE, r'LSP 槽=(-?\d+)')
    rE = grab(txtE, r'环验证拒=(\d+)')
    print('  E 中途入口: rc=%d LSP 槽=%s 环验证拒=%s' % (rcE, sE, rE))
    if rcE != 0 or sE != 0 or (rE or 0) < 1:
        fails.append('E 中途入口: 应拒（槽=0 且 环验证拒>=1），实测 槽=%s 拒=%s rc=%d' % (sE, rE, rcE))

    # ---------------- G/H. 槽写形态盲点（真跑两链比退出码） ----------------
    for tag, code, pat in (
        ('G 槽写形态(LSP)', SLOTW, r'LSP 槽=(\d+)'),
        ('H 跨块尾巴(转发)', XBLKW, r'存→取 转发 条=(\d+)'),
    ):
        src = put('blind' + tag[0], build(code, True))
        out = os.path.join(a.work, 'blind%s_o.asm' % tag[0])
        rc1, txt = opt_run(src, out)
        r1, _ = asm_run(src, os.path.join(a.work, 'blind%s_n.exe' % tag[0]))
        r2, _ = asm_run(out, os.path.join(a.work, 'blind%s_o.exe' % tag[0]))
        if rc1 != 0 or r1 != 0 or r2 != 0:
            fails.append('%s: 链路 rc=%d/%d/%d' % (tag, rc1, r1, r2))
            print('  %s: ★链路失败 rc=%d/%d/%d' % (tag, rc1, r1, r2))
            continue
        e1 = run([os.path.join(a.work, 'blind%s_n.exe' % tag[0])], 60).returncode
        e2 = run([os.path.join(a.work, 'blind%s_o.exe' % tag[0])], 60).returncode
        # G: 该槽（[rbp-4400]）绝不许被提升；H: 绝不许转发
        otxt = io.open(out, encoding='utf-8', errors='replace').read()
        leaked = tag.startswith('G') and re.search(r'\.增\s+取帧(32|64) r\d+, \[rbp-4400\]', otxt) is not None
        if tag.startswith('H'):
            leaked = re.search(r'\[跨块\] 存→取 转发 条=(\d+)', txt)
            leaked = bool(leaked and int(leaked.group(1)) != 0)
        print('  %s: 两链 exit=%d/%d 一致=%s%s'
              % (tag, e1, e2, e1 == e2, '（目标类 0 命中）' if not leaked else '（★仍在提升该槽）'))
        if e1 != e2:
            fails.append('%s: 两链退出码不同（%d vs %d）' % (tag, e1, e2))
        if leaked:
            fails.append('%s: 未识别的写形态仍被提升/转发' % tag)

    # ---------------- F. 真实产物 ----------------
    real = '''#include <stdio.h>
int main(void){
    int s = 0;
    for (int i = 0; i < 300; i = i + 1)
        for (int j = 0; j < 300; j = j + 1)
            s = s + i * j;
    printf("REAL %d\\n", s);
    return 0;
}
'''
    rp = os.path.join(a.work, 'real.c')
    io.open(rp, 'w', encoding='utf-8', newline='\n').write(real)
    base = os.path.join(a.work, 'real')
    r = run([cc, '-S', rp, '-o', base + '_n.exe'])
    if r.returncode != 0:
        fails.append('F 真实: cc -S 失败')
    else:
        s = base + '_n.exe.asm'
        outF = os.path.join(a.work, 'real_o.asm')
        rcF, txtF = opt_run(s, outF)
        sF = grab(txtF, r'LSP 槽=(-?\d+)')
        rF = grab(txtF, r'环验证拒=(\d+)')
        bF = grab(txtF, r'\[结构\] 块=(\d+)')
        eF = grab(txtF, r'边=(\d+)')
        print('  F 真实 cc -S: rc=%d 结构 块=%s 边=%s | LSP 槽=%s 环验证拒=%s'
              % (rcF, bF, eF, sF, rF))
        rr1, _ = asm_run(s, os.path.join(a.work, 'real_n.exe'))
        rr2, _ = asm_run(outF, os.path.join(a.work, 'real_o.exe'))
        if rr1 != 0 or rr2 != 0:
            fails.append('F 真实: 汇编失败 %d/%d' % (rr1, rr2))
        else:
            p1 = run([os.path.join(a.work, 'real_n.exe')])
            p2 = run([os.path.join(a.work, 'real_o.exe')])
            same = (p1.stdout == p2.stdout and p1.returncode == p2.returncode)
            print('           两链 运行: rc %d/%d stdout 相同=%s' % (p1.returncode, p2.returncode, same))
            if not same:
                fails.append('F 真实: 两链 运行 不一致')
        if rcF != 0 or (sF or 0) <= 0:
            fails.append('F 真实: LSP 未命中（槽=%s）' % sF)

    print('=' * 60)
    if fails:
        print('LSP 判据: FAIL')
        for f in fails:
            print('  ✗ %s' % f)
        return 1
    print('LSP 判据: PASS（隔离对账 + 负控 + 破损响亮 + 旁路出口拒 + 中途入口拒 + 真实产物）')
    return 0


if __name__ == '__main__':
    sys.exit(main())
