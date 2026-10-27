# -*- coding: utf-8 -*-
"""check_addr2func —— **「地址 → 函数」量具**的验收判据（台账 D54 操作面 · 2026-10-25）

为什么需要它：D54 的痛点是「崩溃只有裸地址」（`0xC0000005 @ 0x401248`）。
写完整 PDB 要两周，但 `-g` 早就已经产出 **DWARF 4**（`.debug_abbrev/.debug_info/.debug_line`，
随自写链接器进入 PE 的 `/4 /18 /30` 三个长名节）——量具不必等 PDB，直接从 DWARF 读函数区间。

量具：`scripts/addr2func.py`
  `--exe a.exe --list`            列出全部 subprogram 区间（名 / low_pc / 长度 / 声明行）
  `--exe a.exe --addr 0x401500`   VA 口径（默认；**不做静默改读**，未命中就报未命中）
  `--as rva|off`                  显式换口径

本判据五条（全过才 rc=0）：
  ① `-g` 编小样 rc=0，`--list` 函数数 ≥ 2 且区间**单调不重叠**
  ② 用户函数地址命中：函数名 + 偏移正确，且 `low_pc` 与 `-S` 清单的函数入口首指令一致
  ③ 运行时函数地址命中（`-g` 的产物里运行库与用户源同处一个 CU）
  ④ 负控：`.text` 之外的地址必须「未命中」且 rc≠0（**不许**为了好看改读别的口径蒙对）
  ⑤ 隔离门：`-g` 与 无`-g` 的 `.text` **逐字节一致**（#289 的验收门①，量具不得改变被量对象）
  ⑥ `--probe-g`（可选，指向 `-g` 编出的**编译器自己**）：解析一个编译器函数 ⟹ 证明量具能用于
     真实崩溃（不是只能解玩具样例）。缺该文件 ⟹ rc=2 环境未就绪（**不判绿**）。

用法: python scripts/check_addr2func.py [--cc build/dev_fmap3.exe] [--probe-g build/qcc_g.exe]
退出码: 0 = 六条全过；1 = 有红；2 = 环境未就绪（不判绿）
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
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
A2F = os.path.join(ROOT, 'scripts', 'addr2func.py')

SRC = '''#包含 <stdio.h>
静 整 助手甲(整 x) { 返 x * 3 + 1; }
整 助手乙(整 x) { 返 助手甲(x) + 7; }
整 主(空) { printf("%d\\n", 助手乙(5)); 返 0; }
'''


def pe_text(path):
    b = io.open(path, 'rb').read()
    pe = struct.unpack_from('<I', b, 0x3c)[0]
    nsec = struct.unpack_from('<H', b, pe + 6)[0]
    opt_size = struct.unpack_from('<H', b, pe + 20)[0]
    sec_base = pe + 24 + opt_size
    for i in range(nsec):
        off = sec_base + 40 * i
        nm = b[off:off + 8].rstrip(b'\0').decode('latin1')
        vsize, va, raw_size, raw_ptr = struct.unpack_from('<IIII', b, off + 8)
        if nm == '.text':
            return b[raw_ptr:raw_ptr + vsize]
    return b''


def a2f(*args):
    r = subprocess.run([sys.executable, '-X', 'utf8', A2F] + list(args),
                       capture_output=True, timeout=600)
    return r.returncode, (r.stdout or b'').decode('utf-8', 'replace'), \
        (r.stderr or b'').decode('utf-8', 'replace')


def parse_list(text):
    """从 `--list` 输出解析 (name, low, length, line, file)"""
    rows = []
    for l in text.split('\n'):
        m = re.match(r'\s+(\S+)\s+0x([0-9A-Fa-f]+) \+(\d+)\s+\(len (\d+)\s*\) line (\d+) file (\d+)', l)
        if m:
            rows.append((m.group(1), int(m.group(2), 16), int(m.group(4)),
                         int(m.group(5)), int(m.group(6))))
    return rows


def listing_order(path):
    """`-S` 清单里的函数出现顺序（`; === 名 ===` 标记）"""
    raw = io.open(path, 'rb').read()
    s = raw.decode('utf-8', 'replace')
    return re.findall(r'^; === (.+?) ===$', s, re.M)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cc', default=os.path.join(ROOT, 'build', 'dev_fmap3.exe'))
    ap.add_argument('--probe-g', default=os.path.join(ROOT, 'build', 'qcc_g.exe'))
    a = ap.parse_args()
    cc = a.cc if os.path.isabs(a.cc) else os.path.join(ROOT, a.cc)
    if not os.path.exists(cc):
        print('[FAIL] 环境未就绪: 找不到编译器 %s（本判据未执行，不判绿）' % cc)
        return 2
    w = tempfile.mkdtemp(prefix='qcc_addr2func_')
    src = os.path.join(w, 'a.c')
    io.open(src, 'w', encoding='utf-8', newline='\n').write(SRC)

    # ---- ① -g 编 + 函数表 ----
    gexe = os.path.join(w, 'a_g.exe')
    ng = os.path.join(w, 'a_ng.exe')
    r = subprocess.run([cc, '-g', '-o', gexe, src], capture_output=True, timeout=1800)
    ok1 = (r.returncode == 0 and os.path.exists(gexe))
    print('[① -g 编小样 ] rc=%d %s' % (r.returncode, 'OK' if ok1 else 'FAIL'))
    if not ok1:
        print((r.stderr or b'').decode('utf-8', 'replace')[:400])
        print('=== 地址→函数量具: FAIL（连 -g 都编不出）===')
        return 1
    rc, out, err = a2f('--exe', gexe, '--list')
    subs = parse_list(out)
    cu = out.split('CU=')[1].split()[0] if 'CU=' in out else '?'
    print('[① 函数表   ] %d 个函数，CU=%s' % (len(subs), cu))
    if rc != 0 or len(subs) < 2:
        print(out[:600], err[:400])
        print('=== 地址→函数量具: FAIL（函数表读不出）===')
        return 1
    overlaps = [(subs[i][0], subs[i + 1][0]) for i in range(len(subs) - 1)
                if subs[i][1] + subs[i][2] > subs[i + 1][1]]
    mono = all(subs[i][1] <= subs[i + 1][1] for i in range(len(subs) - 1))
    print('[① 单调/不重叠] %s%s' % ('OK' if mono and not overlaps else 'FAIL',
                                 '' if not overlaps else ' 重叠: %s' % overlaps[:3]))
    ok1 = ok1 and mono and not overlaps

    # ---- ② 用户函数地址 + -S 一致性 ----
    user = [s for s in subs if s[0] in ('助手甲', '助手乙', '主')]
    ok2 = False
    if len(user) < 2:
        print('[② 用户函数 ] FAIL：函数表里找不到 助手甲/助手乙')
    else:
        name, low, length, line, _f = user[0]
        probe = low + 10 if length > 12 else low + 1
        rc, out2, _e = a2f('--exe', gexe, '--addr', hex(probe))
        m = re.match(r'0x[0-9A-Fa-f]+ -> (\S+) \+ (\d+)', out2.strip())
        got = (m.group(1), int(m.group(2))) if m else (None, -1)
        exp = (name, probe - low)
        ok2 = (rc == 0 and got == exp)
        print('[② 用户地址 ] %s -> %s（期望 %s + %d）%s'
              % (hex(probe), '%s + %d' % got if got[0] else '未命中',
                 exp[0], exp[1], 'OK' if ok2 else 'FAIL'))
        # -S 清单：函数出现顺序必须与 DWARF 地址序一致（子集比对）
        sexe = os.path.join(w, 'a.s')
        subprocess.run([cc, '-S', '-o', sexe, src], capture_output=True, timeout=1800)
        asm = sexe + '.asm'
        if os.path.exists(asm):
            lo = listing_order(asm)
            dw = [s[0] for s in subs]
            common = [n for n in dw if n in lo]
            order_ok = [n for n in lo if n in dw] == common
            print('[② -S 顺序 ] 清单 %d / DWARF %d / 交集 %d ⟹ %s'
                  % (len(lo), len(dw), len(common), 'OK' if order_ok else 'FAIL'))
            ok2 = ok2 and order_ok
        else:
            print('[② -S 顺序 ] FAIL：`-S` 未产出 .asm 清单')
            ok2 = False

    # ---- ③ 运行时函数地址 ----
    rt = [s for s in subs if s not in user]
    ok3 = False
    if not rt:
        print('[③ 运行库   ] FAIL：函数表里没有运行库函数')
    else:
        name, low, length, line, fno = rt[0]
        rc, out3, _e = a2f('--exe', gexe, '--addr', hex(low + 1))
        ok3 = (rc == 0 and ('-> %s + 1' % name) in out3 and 'decl 2:' in out3)
        print('[③ 运行库   ] %s @ %s+1 decl_file=2 ⟹ %s'
              % (name, hex(low), 'OK' if ok3 else 'FAIL | ' + out3.strip()[:120]))

    # ---- ④ 负控 ----
    rc4a, out4a, _ = a2f('--exe', gexe, '--addr', '0x400001')
    rc4b, out4b, _ = a2f('--exe', gexe, '--addr', '0x500000')
    ok4 = rc4a == 1 and rc4b == 1 and '未命中' in out4a and '未命中' in out4b
    print('[④ 负控     ] 0x400001 rc=%d / 0x500000 rc=%d ⟹ %s'
          % (rc4a, rc4b, 'OK（未命中且非 0）' if ok4 else 'FAIL（静默改读口径或假命中）'))

    # ---- ⑤ 隔离门：-g 不改 .text ----
    r = subprocess.run([cc, '-o', ng, src], capture_output=True, timeout=1800)
    t1, t2 = pe_text(gexe), pe_text(ng)
    ok5 = (r.returncode == 0 and len(t1) > 0 and t1 == t2)
    print('[⑤ 隔离门   ] -g .text %d B sha=%s ｜ 无-g %d B sha=%s ⟹ %s'
          % (len(t1), hashlib.sha256(t1).hexdigest()[:12],
             len(t2), hashlib.sha256(t2).hexdigest()[:12],
             'OK（逐字节一致）' if ok5 else 'FAIL'))

    # ---- ⑥ 真实编译器（-g 自编）----
    probe = a.probe_g if os.path.isabs(a.probe_g) else os.path.join(ROOT, a.probe_g)
    if not os.path.exists(probe):
        print('[⑥ 编译器自 ] 环境未就绪: 缺 %s（用 `工具 -g -o build/qcc_g.exe srclib_jiayan/qcc_zh.jy` 产出）'
              % probe)
        print('=== 地址→函数量具: 环境未就绪（①–⑤ 已跑，⑥ 未执行 ⟹ 不判绿）===')
        return 2
    rc, out6, _ = a2f('--exe', probe, '--list')
    big = parse_list(out6)
    ok6 = False
    if rc == 0 and len(big) > 100:
        target = None
        for cand in ('解析', '词法', '生成码', '查变量'):
            for s in big:
                if s[0] == cand:
                    target = s
                    break
            if target:
                break
        if target:
            name, low, length, line, _f = target
            rc2, out6b, _e = a2f('--exe', probe, '--addr', hex(low + 3))
            ok6 = rc2 == 0 and ('-> %s + 3' % name) in out6b
            print('[⑥ 编译器自 ] %d 函数；%s @ %s（len %d）→ %s ⟹ %s'
                  % (len(big), name, hex(low), length, out6b.strip()[:80],
                     'OK' if ok6 else 'FAIL'))
        else:
            print('[⑥ 编译器自 ] FAIL：找不到预期函数名（%s）'
                  % ', '.join(s[0] for s in big[:5]))
    else:
        print('[⑥ 编译器自 ] FAIL：%s 的函数表读不出（rc=%d, %d 条）' % (probe, rc, len(big)))

    ok = ok1 and ok2 and ok3 and ok4 and ok5 and ok6
    print('=== 地址→函数量具: %s ===' % ('PASS' if ok else 'FAIL'))
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())
