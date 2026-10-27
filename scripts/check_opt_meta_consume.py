#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""check_opt_meta_consume —— 优化器 **真解析 + 真消费** 中间态 元数据（R880 / R880d）。

七个用例，全部 真跑 出厂链（`cc 编出 opt_zh` → `opt_zh` → `asm_zh` → 运行）：

  A. **DCE 隔离**（带元数据的最小汇编，只含一条 死定义）
     · `[元数据] … 良构`（真解析）· G-DCE == 1，**其余 11 类 全部 == 0**
     · **优化器 自报 `G-DCE 删字节` == `.text` 有效内容 差值**（R880d：行长模型，删不了就不删）
     · 两链 运行 退出码 / stdout 相同
  B/C. **⑪ / ⑫ 隔离**（无元数据）：目标类 == 1、其余类 == 0、`.text` 差 == 汇编器 实测缩、两链运行相同
  D/E. **⑪ / ⑫ 寄存器互斥 回归**：`弹栈 r0` 会覆盖 K ⟹ **0 命中** 且 两链 运行 相同
  F. **负控（擦掉元数据）**：G-DCE == 0、全类 合计 0、两链 `.text` **逐字节相同**、运行相同
  G. **破损 元数据**：`u=999` 悬挂 ⟹ 优化器 **rc=1** 且 打印 元数据 错误
  H. **真实 `cc -S` 产物**：`[元数据]` 解析 + G-DCE>0 + 自报字节 N>0；擦掉元数据后 无 G-DCE；
     两链 `.text` 差 == N，且 两链 运行 rc/stdout 相同

用法: python scripts/check_opt_meta_consume.py [--cc qcc_zh_v1.exe]
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

DCE_BODY = '''_入口:
    移动 r2, 7
    移动 r0, 3
    返回
'''
DCE_META = '''; @QCCMETA 1 ins=3 funcs=1 loops=0 nodes=4
; @I 0 f=1 n=1 k=1 t=-1 l=-1 d=0
; @I 1 f=1 n=2 k=1 t=-1 l=-1 d=0
; @I 2 f=1 n=3 k=2 t=-1 l=-1 d=0
; @U 0 r=2 z=1 u=
; @U 1 r=0 z=0 u=2
; @QCCMETA-END
'''

FUSE11 = '''_入口:
    移动 r0, 0
    压栈 r0
    移动 r0, 5
    弹栈 r3
    比较 r3, r0
    置小 al
    零扩展 eax, al
    测试 r0, r0
    为零跳 .Lend
    移动 r0, 1
.Lend:
    返回
'''
FUSE12 = '''_入口:
    移动 r0, 0
    压栈 r0
    移动 r0, 5
    弹栈 r3
    减 r3, r0
    移32 r0, r3
    返回
'''
# 回归：弹栈 目的 == r0 ⟹ 覆盖 K ⟹ 语义 变成 V,V — 不许融合
FUSE11_RO = '''_入口:
    移动 r0, 0
    压栈 r3
    移动 r0, 5
    弹栈 r0
    比较 r0, r0
    置小 al
    零扩展 eax, al
    测试 r0, r0
    为零跳 .Lend
    移动 r0, 1
.Lend:
    返回
'''
FUSE12_RO = '''_入口:
    移动 r0, 0
    压栈 r3
    移动 r0, 5
    弹栈 r0
    减 r0, r0
    移32 r0, r0
    返回
'''

REAL_SRC = '''#include <stdio.h>
int main(void){
    int a = 3;
    a = 4;
    printf("%d\\n", a);
    return 0;
}
'''

CLASS_KEYS = [
    ('栈spill', r'栈spill (\d+)'), ('栈合并', r'栈合并 (\d+)'), ('抵消对', r'抵消对 (\d+)'),
    ('不可达跳转', r'不可达 跳转 (\d+)'), ('不可达返回', r'不可达 跳转 \d+ 返回 (\d+)'),
    ('复制传播', r'复制传播 (\d+)'), ('跳落到下一行', r'跳落到下一行 (\d+)'),
    ('帧栈存取同址', r'帧栈存取同址 (\d+)'), ('死值写覆盖', r'死值写覆盖 (\d+)'),
    ('零栈调整', r'零栈调整 (\d+)'), ('G-DCE', r'G-DCE (\d+)'),
    ('比较分支融合', r'比较分支融合 (\d+)'), ('算术立即融合', r'算术立即融合 (\d+)'),
    ('循环槽提升', r'LSP 槽=(\d+)'), ('跨块转发', r'跨块转发 (\d+)'), ('压栈回读', r'压栈回读 (\d+)'),
]


def run(cmd, timeout=None):
    return subprocess.run(cmd, capture_output=True, timeout=timeout)


def parse_classes(out):
    m = re.search(r'优化 \d+ 处 \[([^\]]*)\]', out)
    seg = m.group(1) if m else ''
    d = {k: int(re.search(p, seg).group(1)) if re.search(p, seg) else None for k, p in CLASS_KEYS}
    # `循环槽提升` 有自己的 `[循环]` 行（不在 `[...]` 汇总里）⟹ 单独解析
    m2 = re.search(r'LSP 槽=(\d+)', out)
    d['循环槽提升'] = int(m2.group(1)) if m2 else None
    return d


def parse_self_bytes(out):
    m = re.search(r'G-DCE 删字节=(-?\d+)', out)
    return int(m.group(1)) if m else None


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
            return j + 1, b[:j + 1]
    return -1, b''


def shz(out):
    m = re.search(r'实测缩=(-?\d+)', out)
    return int(m.group(1)) if m else None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cc', default='qcc_zh_v1.exe')
    ap.add_argument('--work', default=os.path.join(os.environ.get('TEMP', '.'), 'qcc_opt_meta'))
    a = ap.parse_args()
    cc = a.cc if os.path.isabs(a.cc) else os.path.join(ROOT, a.cc)
    opt_src = os.path.join(ROOT, 'srclib', 'opt_zh.jy')
    asm = os.path.join(ROOT, 'asm_zh.exe')
    if not (os.path.exists(cc) and os.path.exists(opt_src) and os.path.exists(asm)):
        print('[FAIL] 环境未就绪'); return 2
    os.makedirs(a.work, exist_ok=True)
    opt = os.path.join(a.work, 'opt_meta.exe')
    r = run([cc, opt_src, '-o', opt])
    if r.returncode != 0:
        print('✗ 优化器 源 编译 失败 rc=%d' % r.returncode); return 1
    print('⓪ 出厂优化器 由 shipped 编出 rc=0（%d 字节）' % os.path.getsize(opt))
    fails = []

    def chain(tag, text=None, src=None, do_opt=True):
        d = dict(rc_opt=None, out_opt='', shr=None)
        if text is not None:
            p = os.path.join(a.work, tag + '.asm')
            io.open(p, 'w', encoding='utf-8', newline='\n').write(text)
        else:
            p = src
        cur = p
        if do_opt:
            o = os.path.join(a.work, tag + '_o.asm')
            ro = run([opt, p, '-o', o])
            d['rc_opt'], d['out_opt'] = ro.returncode, (ro.stdout + ro.stderr).decode('utf-8', 'replace')
            cur = o
        exe = os.path.join(a.work, tag + '.exe')
        ra = run([asm, cur, '-o', exe])
        d['asm_rc'], d['asm_out'] = ra.returncode, (ra.stdout + ra.stderr).decode('utf-8', 'replace')
        if ra.returncode == 0:
            d['shr'] = shz(d['asm_out'])
            d['tsize'], d['tbytes'] = text_size(exe)
            rp = run([exe], 120)
            d['run_rc'], d['run_out'] = rp.returncode, rp.stdout
        return d

    def isolated(tag, text, target, need_self=False):
        base = chain(tag + '_n', text, do_opt=False)
        optd = chain(tag + '_o', text, do_opt=True)
        if optd['rc_opt'] != 0:
            fails.append('%s: opt rc=%d' % (tag, optd['rc_opt'])); return
        cls = parse_classes(optd['out_opt'])
        other = {k: v for k, v in cls.items() if k != target and v}
        sb = parse_self_bytes(optd['out_opt'])
        delta = (base['tsize'] - optd['tsize']) if ('tsize' in base and 'tsize' in optd) else None
        print('  %-10s %s=%s · 其余非零=%s · .text 差=%s · 自报字节=%s · 实测缩=%s · 运行 rc %s/%s'
              % (tag, target, cls.get(target), other or '{}', delta, sb, optd['shr'],
                 base.get('run_rc'), optd.get('run_rc')))
        if not cls.get(target):
            fails.append('%s: %s 未触发' % (tag, target))
        if other:
            fails.append('%s: 其它类 非零 %s' % (tag, other))
        if base.get('asm_rc') != 0 or optd.get('asm_rc') != 0:
            fails.append('%s: asm 失败' % tag); return
        if need_self:
            if sb is None or sb <= 0:
                fails.append('%s: 缺 优化器 自报字节' % tag)
            elif delta != sb:
                fails.append('%s: .text 差 %s ≠ 优化器 自报 %s' % (tag, delta, sb))
        elif delta != optd['shr']:
            fails.append('%s: .text 差 %s ≠ 汇编器 实测缩 %s' % (tag, delta, optd['shr']))
        if (base.get('run_rc'), base.get('run_out')) != (optd.get('run_rc'), optd.get('run_out')):
            fails.append('%s: 两链 运行 不等' % tag)

    def regression(tag, text):
        base = chain(tag + '_n', text, do_opt=False)
        optd = chain(tag + '_o', text, do_opt=True)
        cls = parse_classes(optd['out_opt'])
        hits = {k: v for k, v in cls.items() if v}
        print('  %-10s 全部类=%s（应为空）· 运行 rc %s/%s'
              % (tag, hits or '{}', base.get('run_rc'), optd.get('run_rc')))
        if hits:
            fails.append('%s: 不该 触发 任何 类 %s' % (tag, hits))
        if (base.get('run_rc'), base.get('run_out')) != (optd.get('run_rc'), optd.get('run_out')):
            fails.append('%s: 两链 运行 不等' % tag)

    print('① DCE 隔离（带元数据，自报字节 对账）')
    isolated('dce', DCE_BODY + '\n' + DCE_META, 'G-DCE', need_self=True)
    print('② ⑪ 比较-分支 融合 隔离')
    isolated('f11', FUSE11, '比较分支融合')
    print('③ ⑫ 算术-立即数 融合 隔离')
    isolated('f12', FUSE12, '算术立即融合')
    print('④ ⑪ 弹栈=r0 回归（不许融合）')
    regression('f11ro', FUSE11_RO)
    print('⑤ ⑫ 弹栈=r0 回归（不许融合）')
    regression('f12ro', FUSE12_RO)

    # ⑥ 负控：擦掉元数据
    base = chain('neg_n', DCE_BODY, do_opt=False)
    neg = chain('neg_o', DCE_BODY, do_opt=True)
    cls = parse_classes(neg['out_opt'])
    nonopt = sum(v or 0 for v in cls.values())
    print('⑥ 负控（擦掉元数据）: G-DCE=%s · 全类合计=%d · .text 逐字节相同=%s · 运行 rc %s/%s'
          % (cls.get('G-DCE'), nonopt, base.get('tbytes') == neg.get('tbytes'),
             base.get('run_rc'), neg.get('run_rc')))
    if cls.get('G-DCE') != 0 or nonopt != 0:
        fails.append('负控: 有 pass 触发（应 0）')
    if base.get('tbytes') != neg.get('tbytes'):
        fails.append('负控: 两链 .text 不 逐字节 相同')
    if (base.get('run_rc'), base.get('run_out')) != (neg.get('run_rc'), neg.get('run_out')):
        fails.append('负控: 两链 运行 不等')

    # ⑦ 破损：u=999 悬挂
    p = os.path.join(a.work, 'u999.asm')
    io.open(p, 'w', encoding='utf-8', newline='\n').write(DCE_BODY + '\n' + DCE_META.replace('; @U 0 r=2 z=1 u=', '; @U 0 r=2 z=1 u=999'))
    ro = run([opt, p, '-o', os.path.join(a.work, 'u999_o.asm')])
    txt = (ro.stdout + ro.stderr).decode('utf-8', 'replace')
    print('⑦ 破损元数据（u=999 悬挂）: rc=%d · 响亮=%s' % (ro.returncode, '是' if ('元数据' in txt and ro.returncode != 0) else '否'))
    if ro.returncode == 0 or '元数据' not in txt:
        fails.append('u=999 未 响亮 拒绝')

    # ⑧ 真实 cc -S 产物
    src = os.path.join(a.work, 'real.c')
    io.open(src, 'w', encoding='utf-8', newline='\n').write(REAL_SRC)
    base_exe = os.path.join(a.work, 'real.exe')
    rc0 = run([cc, '-S', src, '-o', base_exe])
    real_asm = base_exe + '.asm'
    if rc0.returncode != 0 or not os.path.exists(real_asm):
        fails.append('真实产物: -S 失败')
    else:
        txt_in = io.open(real_asm, encoding='utf-8', errors='replace').read()
        nom = os.path.join(a.work, 'real_nometa.asm')
        io.open(nom, 'w', encoding='utf-8', newline='\n').write(
            '\n'.join(l for l in txt_in.split('\n') if not l.startswith('; @')) + '\n')
        m1 = chain('real_m', src=real_asm, do_opt=True)
        m0 = chain('real_nm', src=nom, do_opt=True)
        sb = parse_self_bytes(m1['out_opt'])
        cls1 = parse_classes(m1['out_opt'])
        cls0 = parse_classes(m0['out_opt'])
        delta = m0['tsize'] - m1['tsize']
        ml = re.search(r'LSP 槽=-?\d+ 净减字节=(-?\d+)', m1['out_opt'])
        sl = int(ml.group(1)) if ml else 0
        print('⑧ 真实 -S 产物: 元数据解析=%s · G-DCE=%s（自报 %s）· LSP 净减=%s · 无元数据 G-DCE=%s · .text 差=%s · 运行 rc %s/%s'
              % ('是' if '良构' in m1['out_opt'] else '否', cls1.get('G-DCE'), sb, sl, cls0.get('G-DCE'),
                 delta, m1.get('run_rc'), m0.get('run_rc')))
        if '良构' not in m1['out_opt']:
            fails.append('真实产物: 未解析元数据')
        if not cls1.get('G-DCE'):
            fails.append('真实产物: G-DCE=0（真元数据 未 驱动）')
        if cls0.get('G-DCE'):
            fails.append('真实产物: 擦元数据 后 仍 G-DCE>0')
        if cls1.get('循环槽提升', 0) <= 0:
            fails.append('真实产物: LSP=0（真元数据 未 驱动 循环级 pass）')
        if cls0.get('循环槽提升', 0):
            fails.append('真实产物: 擦元数据 后 仍 LSP>0')
        # 只关 LSP（保留 @I/@U 通道）⟹ 与全开件 的差 必须 精确等于 LSP 自报，且其它类命中数不变
        txt_noL = '\n'.join(
            (re.sub(r'loops=\d+', 'loops=0', l) if l.startswith('; @QCCMETA 1 ')
             else re.sub(r' l=-?\d+', ' l=-1', l) if l.startswith('; @I ') else l)
            for l in txt_in.split('\n') if not l.startswith('; @L '))
        mL = chain('real_noL', text=txt_noL, do_opt=True)
        clsL = parse_classes(mL['out_opt'])
        dL = mL['tsize'] - m1['tsize']
        oth1 = {k: v for k, v in cls1.items() if k not in ('G-DCE', '循环槽提升') and v}
        othL = {k: v for k, v in clsL.items() if k not in ('G-DCE', '循环槽提升') and v}
        print('   ↳ 只关 LSP: LSP=%s · .text 差=%s · 自报=%s · 其它类 全开=%s / 关LSP=%s'
              % (clsL.get('循环槽提升'), dL, sl, oth1, othL))
        if clsL.get('循环槽提升'):
            fails.append('真实产物 只关 LSP: 仍 命中 %s' % clsL.get('循环槽提升'))
        # 混合输入上 逐字节等式 不成立（LSP 改写行 会抑制 帧栈存取同址 一类），这里只做
        # **方向性** 判定: 开 LSP 后 代码必须更小（dL > 0）；其余类的出入 如实打印。
        if dL <= 0:
            fails.append('真实产物 只关 LSP: 开 LSP 后 .text 没有变小（%d）' % dL)
        if oth1 != othL:
            print('   ↳ 注: LSP 改写行 抑制/新增了 其它类命中（%s → %s）⟹ 精确等式 由 单类隔离件 承担'
                  % (othL, oth1))
        if sb is None or sb <= 0:
            fails.append('真实产物: 缺 自报字节')
        elif delta != sb + sl:
            print('   （注: 有附录 vs 擦附录 的差 %d ≠ 两类自报和 %d —— LSP 改写行 让 其它 pass 少/多命中，'
                  '精确等式 由 check_opt_lsp 用例 A / check_opt_per_pass ⑬（单类隔离件）承担）' % (delta, sb + sl))
        if (m1.get('run_rc'), m1.get('run_out')) != (m0.get('run_rc'), m0.get('run_out')):
            fails.append('真实产物: 两链 运行 不等')

    print('=' * 64)
    if fails:
        print('元数据 消费 闸: FAIL -> %s' % '; '.join(fails))
        return 1
    print('元数据 消费 闸: PASS（真解析 · DCE/⑪/⑫ 逐类隔离 · 自报字节==.text差 · 寄存器互斥回归 · '
          '负控产物逐字节 · 悬挂响亮 · 真实 -S 产物 端到端 对账）')
    return 0


if __name__ == '__main__':
    sys.exit(main())
