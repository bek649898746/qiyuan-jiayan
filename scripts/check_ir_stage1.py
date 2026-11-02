# -*- coding: utf-8 -*-
"""check_ir_stage1 — IR 阶段 1 的可执行判据（ORD-20261102-85）。

判据（全部驱动 真实编译器 + 真实产物）：
  A `--ir` 导出 **显式指令链**：`-- IR 指令链` + `指令[i] 起/长/型/槽/偏/值` + `[LINK] … 一致=YES`；
    并逐条核对：导出的 `值` == 产物 `.text` 在 `起+长-4` 处的 4 字节（回填确实写进了机器码）。
  B `QCC_DBG_RELOC=1`：自编（`srclib_jiayan/qcc_zh.jy`）`抬基址重跑=0` 且 回填==执行>0；
    小输入默认允许 ≤1 次有界抬基址（计数器可观测）；再用 `QCC_PREBASE=0x200000` 强制足够预基，
    要求同一小输入 `抬基址重跑=0` 且产物可运行 —— 证明正确性不依赖兜底循环。
  C `QCC_OPT=all`：`[OPT] 常量折叠: 改 N 处` 且 N>0；`QCC_OPT=none`：N==0。
  D 负锚：浮点 `x * 0.0` 的 `浮点拒绝 M 处` 且 M>0，运行结果 `0.000000`。
  E 等价：5 类代表输入在 QCC_OPT=all / none 下产物 SHA256 相同且运行正确。
  F `-S` 端到端：`check_asm_e2e.py`（中文汇编器 asm_zh 产物 运行/指令级 同源）全过。
  G 体积回归：>100KB 源不得被启发式抬到 8MB 基址（产物 < 2MB；改前 8.39MB）。
用法: python scripts/check_ir_stage1.py [--cc qcc_zh_v1.exe]
"""
import argparse
import hashlib
import io
import os
import re
import struct
import subprocess
import sys
import tempfile

sys.stdout.reconfigure(encoding='utf-8', errors='replace')
REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

CONSTS = '''#include <stdio.h>
函数 整 主(空){ 整 x = 1 + 2 * 3; 格式化 "OPT %d\\n", x; 返 0; }
'''
FLOAT = '''#include <stdio.h>
函数 整 主(空){ 双 x = 3.5; 双 y = x * 0.0; 格式化 "%f\\n", y; 返 0; }
'''
REPS = {
    'plain': ('P 7', '#include <stdio.h>\n函数 整 主(空){ 格式化 "P %d\\n", 7; 返 0; }\n'),
    'struct': ('3 4', '#include <stdio.h>\n构 S { 整 a; 整 b; };\n函数 整 主(空){ 构 S s; s.a=3; s.b=4; 格式化 "%d %d\\n", s.a, s.b; 返 0; }\n'),
    'float': ('3.750000', '#include <stdio.h>\n函数 整 主(空){ 双 x = 1.5; 双 y = 2.5; 格式化 "%f\\n", x*y; 返 0; }\n'),
    'pointer': ('2', '#include <stdio.h>\n函数 整 主(空){ 整 a[3]; a[0]=1; a[1]=2; a[2]=3; 整 *p = a; 格式化 "%d\\n", *(p+1); 返 0; }\n'),
    'channel': ('C 9', '#include <stdio.h>\n函数 整 主(空){ 整 q = 通道新建(2); 送 q, 9; 整 v=0; 收 q,v; 释放通道(q); 格式化 "C %d\\n", v; 返 0; }\n'),
}


def pe_text_raw(path):
    """读 PE 的 `.text` PointerToRawData（不在判据里写死文件偏移）。"""
    b = open(path, 'rb').read()
    e = struct.unpack_from('<I', b, 0x3C)[0]
    nsec = struct.unpack_from('<H', b, e + 6)[0]
    optsz = struct.unpack_from('<H', b, e + 20)[0]
    sec = e + 24 + optsz
    for i in range(nsec):
        o = sec + i * 40
        name = b[o:o + 8].rstrip(b'\0')
        if name == b'.text':
            return struct.unpack_from('<I', b, o + 20)[0]
    return None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cc', default=os.environ.get('QCC_TEST_BIN') or 'qcc_zh_v1.exe')
    a = ap.parse_args()
    cc = a.cc if os.path.isabs(a.cc) else os.path.join(REPO, a.cc)
    work = tempfile.mkdtemp(prefix='qcc_ir1_')
    bad = []

    def w(name, text):
        p = os.path.join(work, name)
        io.open(p, 'w', encoding='utf-8', newline='\n').write(text)
        return p

    def comp(src, out, env=None, timeout=600):
        e = dict(os.environ)
        if env:
            e.update(env)
        r = subprocess.run([cc, src, '-o', out], cwd=REPO, capture_output=True, text=True,
                           encoding='utf-8', errors='replace', timeout=timeout, env=e)
        return r.returncode, ((r.stdout or '') + (r.stderr or ''))

    # A：--ir 显式指令链 + 逐条字节核对
    h = subprocess.run([cc, '--help'], capture_output=True, text=True, encoding='utf-8', errors='replace', timeout=60)
    help_txt = (h.stdout or '') + (h.stderr or '')
    if '--ir' not in help_txt or '--dump-ir' not in help_txt:
        bad.append('--help 缺 --ir/--dump-ir')
    p = w('consts.c', CONSTS)
    dump_exe = os.path.join(work, 'consts_dump.exe')
    r2 = subprocess.run([cc, '--ir', p, '-o', dump_exe], cwd=REPO, capture_output=True, text=True,
                        encoding='utf-8', errors='replace', timeout=600, env=dict(os.environ, QCC_OPT='all'))
    dump = (r2.stdout or '') + (r2.stderr or '')
    if r2.returncode != 0:
        bad.append('--ir 编译失败 rc=%d' % r2.returncode)
    if '-- IR L1 抽象指令' not in dump:
        bad.append('--ir 未导出 L1 抽象指令')
    if '-- IR 指令链' not in dump:
        bad.append('--ir 未导出显式指令链')
    mlink = re.search(r'\[LINK\] 指令=(\d+) 回填=(\d+) 执行=(\d+) 一致=(\w+)', dump)
    if not mlink:
        bad.append('--ir 缺 [LINK] 链接核对行')
        ninsn = nrepl = nexec = -1
    else:
        ninsn, nrepl, nexec = int(mlink.group(1)), int(mlink.group(2)), int(mlink.group(3))
        if mlink.group(4) != 'YES' or ninsn != nrepl or nexec != ninsn or ninsn <= 0:
            bad.append('[LINK] 不一致: %s' % mlink.group(0))
    recs = [(int(i), int(s), int(l), int(t), int(sl), int(off), int(v))
            for i, s, l, t, sl, off, v in re.findall(
                r'指令\[(\d+)\] 起=(\d+) 长=(\d+) 型=(\d+) 槽=(\d+) 偏=(\d+) 值=(-?\d+)', dump)]
    if ninsn >= 0 and len(recs) != ninsn:
        bad.append('指令链 记录数=%d != LINK 指令=%d' % (len(recs), ninsn))
    slots = [r[4] for r in recs]
    if slots and sorted(slots) != list(range(len(slots))):
        bad.append('回填槽 不是 0..N-1 一一对应: %s…' % slots[:8])
    raw = pe_text_raw(dump_exe) if os.path.exists(dump_exe) else None
    if raw is None:
        bad.append('无法定位产物 .text（--ir 未产出/字节核对失败）')
    else:
        data = open(dump_exe, 'rb').read()
        mismatch = 0
        for i, s, l, t, sl, off, v in recs:
            pos = raw + s + l - 4
            if pos + 4 > len(data):
                mismatch += 1
                continue
            got = struct.unpack_from('<i', data, pos)[0]
            if got != v:
                mismatch += 1
        if mismatch:
            bad.append('指令链 值 ↔ 机器码 不一致 %d/%d 条' % (mismatch, len(recs)))

    # B：自编 抬基址重跑=0；小输入 计数器可观测（≤1 次有界兜底）
    self_src = os.path.join(REPO, 'srclib_jiayan', 'qcc_zh.jy')
    rc, out_self = comp(self_src, os.path.join(work, 'self_v1.exe'), {'QCC_DBG_RELOC': '1'}, timeout=1200)
    ms = re.search(r'\[RELOC\] 指令=(\d+) 回填=(\d+) 执行=(\d+) 抬基址重跑=(\d+)', out_self)
    if rc != 0 or not ms:
        bad.append('自编 RELOC 计数缺失 rc=%d: %r' % (rc, out_self.strip()[-160:]))
        n_self = -1
    else:
        n_self = int(ms.group(4))
        if int(ms.group(1)) <= 0 or int(ms.group(2)) != int(ms.group(3)) or n_self != 0:
            bad.append('自编 计数不符: %s' % ms.group(0))
    rc, out_small = comp(p, os.path.join(work, 'consts_reloc.exe'), {'QCC_DBG_RELOC': '1'})
    msm = re.search(r'\[RELOC\] 指令=(\d+) 回填=(\d+) 执行=(\d+) 抬基址重跑=(\d+)', out_small)
    if rc != 0 or not msm:
        bad.append('小输入 RELOC 计数缺失 rc=%d: %r' % (rc, out_small.strip()[-160:]))
        n_small = -1
    else:
        n_small = int(msm.group(4))
        if int(msm.group(1)) <= 0 or int(msm.group(2)) != int(msm.group(3)) or n_small > 1:
            bad.append('小输入 计数不符（应 有界 ≤1）: %s' % msm.group(0))

    # B2：强制足够预基 ⟹ 同一小输入 0 次抬基址且产物可运行（正确性不依赖兜底）
    force_exe = os.path.join(work, 'consts_force.exe')
    rc, out_force = comp(p, force_exe, {'QCC_DBG_RELOC': '1', 'QCC_PREBASE': '0x200000'})
    mf = re.search(r'\[RELOC\] 指令=(\d+) 回填=(\d+) 执行=(\d+) 抬基址重跑=(\d+)', out_force)
    n_force = int(mf.group(4)) if mf else -1
    if rc != 0 or not mf or int(mf.group(1)) <= 0 or int(mf.group(2)) != int(mf.group(3)) or n_force != 0:
        bad.append('强制预基 计数不符 rc=%d: %r' % (rc, out_force.strip()[-160:]))
    else:
        q = subprocess.run([force_exe], capture_output=True, text=True, encoding='utf-8',
                           errors='replace', timeout=60)
        if q.returncode != 0 or 'OPT 7' not in (q.stdout or ''):
            bad.append('强制预基 产物运行不符 rc=%d out=%r' % (q.returncode, (q.stdout or '').strip()[:60]))

    # C：OPT all/none
    rc, out_all = comp(p, os.path.join(work, 'consts_all.exe'), {'QCC_OPT': 'all'})
    m = re.search(r'\[OPT\] 常量折叠: 改 (\d+) 处', out_all)
    n_all = int(m.group(1)) if m else -1
    if rc != 0 or n_all <= 0:
        bad.append('QCC_OPT=all 计数不符 rc=%d n=%s' % (rc, n_all))
    rc, out_none = comp(p, os.path.join(work, 'consts_none.exe'), {'QCC_OPT': 'none'})
    m2 = re.search(r'\[OPT\] 常量折叠: 改 (\d+) 处', out_none)
    n_none = int(m2.group(1)) if m2 else -1
    if rc != 0 or n_none != 0:
        bad.append('QCC_OPT=none 计数不符 rc=%d n=%s' % (rc, n_none))

    # D：浮点负锚
    pf = w('float_zero.c', FLOAT)
    rc, out_f = comp(pf, os.path.join(work, 'float_all.exe'), {'QCC_OPT': 'all'})
    m = re.search(r'浮点拒绝 (\d+) 处', out_f)
    mfloat = int(m.group(1)) if m else -1
    if rc != 0 or mfloat <= 0:
        bad.append('浮点负锚不符 rc=%d 拒绝=%s' % (rc, mfloat))
    else:
        q = subprocess.run([os.path.join(work, 'float_all.exe')], capture_output=True, text=True,
                           encoding='utf-8', errors='replace', timeout=60)
        if q.returncode != 0 or '0.000000' not in (q.stdout or ''):
            bad.append('浮点负锚运行不符 rc=%d out=%r' % (q.returncode, (q.stdout or '').strip()[:60]))

    # E：5 类代表 产物等价 + 运行
    for name, (want, src) in REPS.items():
        p = w(name + '.c', src)
        e_all = os.path.join(work, name + '_all.exe')
        e_none = os.path.join(work, name + '_none.exe')
        r1, _ = comp(p, e_all, {'QCC_OPT': 'all'})
        r2, _ = comp(p, e_none, {'QCC_OPT': 'none'})
        if r1 != 0 or r2 != 0:
            bad.append('%s 编译失败 all=%d none=%d' % (name, r1, r2))
            continue
        h1 = hashlib.sha256(open(e_all, 'rb').read()).hexdigest()
        h2 = hashlib.sha256(open(e_none, 'rb').read()).hexdigest()
        if h1 != h2:
            bad.append('%s QCC_OPT all/none 产物不同 %s != %s' % (name, h1[:12], h2[:12]))
        q = subprocess.run([e_all], capture_output=True, text=True, encoding='utf-8', errors='replace', timeout=60)
        if q.returncode != 0 or want not in (q.stdout or ''):
            bad.append('%s 运行不符 rc=%d out=%r' % (name, q.returncode, (q.stdout or '').strip()[:60]))

    # F：-S 端到端（中文汇编器）
    rf = subprocess.run([sys.executable, os.path.join(REPO, 'scripts', 'check_asm_e2e.py'), '--cc', cc],
                        cwd=REPO, capture_output=True, text=True, encoding='utf-8', errors='replace', timeout=1800)
    fout = (rf.stdout or '') + (rf.stderr or '')
    ok_f = rf.returncode == 0 and '全部 通过' in fout
    if not ok_f:
        bad.append('-S 端到端 失败 rc=%d: %r' % (rf.returncode, fout.strip().splitlines()[-1][:120] if fout.strip() else ''))

    # G：>100KB 源 不得被 8MB 启发式 抬基址（体积回归）
    big = w('big.c', '#include <stdio.h>\n/*' + 'x' * 110000 + '*/\n'
            '函数 整 主(空){ 格式化 "big ok\\n"; 返 0; }\n')
    big_exe = os.path.join(work, 'big.exe')
    rc, out_big = comp(big, big_exe, {'QCC_DBG_RELOC': '1'})
    mb = re.search(r'\[RELOC\] 指令=(\d+) 回填=(\d+) 执行=(\d+) 抬基址重跑=(\d+)', out_big)
    big_sz = os.path.getsize(big_exe) if os.path.exists(big_exe) else -1
    if rc != 0 or not mb:
        bad.append('>100KB 源 编译/计数失败 rc=%d' % rc)
    elif big_sz >= 2 * 1024 * 1024:
        bad.append('>100KB 源 产物体积=%d ≥2MB（8MB 预基启发式 回归）' % big_sz)
    elif int(mb.group(4)) != 0:
        bad.append('>100KB 源 抬基址重跑=%s（4× 预基 已足够）' % mb.group(4))

    print('[A] --ir 指令链: %s（指令=%s 槽覆盖=%s 字节核对=%s）'
          % ('PASS' if not any(x.startswith('--ir') or x.startswith('指令链') or x.startswith('[LINK]') for x in bad) else 'FAIL',
             ninsn, 'YES' if slots and sorted(slots) == list(range(len(slots))) else 'NO',
             'PASS' if raw is not None and not any('机器码' in x for x in bad) else 'FAIL'))
    print('[B] RELOC 自编抬基址重跑=%s 小输入=%s 强制预基=%s' % (n_self, n_small, n_force))
    print('[C] OPT all=%d none=%d' % (n_all, n_none))
    print('[D] 浮点拒绝=%d' % mfloat)
    print('[E] 代表输入 all/none 等价: %s' % ('PASS' if not any(x.startswith(tuple(REPS)) for x in bad) else 'FAIL'))
    print('[F] -S 端到端: %s' % ('PASS' if ok_f else 'FAIL'))
    print('[G] >100KB 源 体积=%s 抬基址重跑=%s' % (big_sz, mb.group(4) if mb else '?'))
    for b in bad:
        print('[FAIL] ' + b)
    print('== IR 阶段1 判据: %s ==' % ('PASS' if not bad else 'FAIL (%d 处)' % len(bad)))
    return 0 if not bad else 1


if __name__ == '__main__':
    sys.exit(main())
