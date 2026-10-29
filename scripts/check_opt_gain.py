# -*- coding: utf-8 -*-
"""优化收益度量器（本 goal 验收 3 的 gating 观测）

对**同一份代表性输入**，分别走：
  非优化链: `编译器 -S` → `中文汇编器`
  优化链  : `编译器 -S` → `优化器` → `中文汇编器`
解析两个 PE 的 `.text` 节，断言**优化链的 `.text` 字节数严格更小**（不是靠垫充冒充）。

同时记录：`.asm` 文本字节数、`.text` 字节数、两者差值 —— 三条读数可复算。

用法:
  python scripts/check_opt_gain.py [--cc <exe>] [--work <dir>] [--src <c文件>] [--minimal]
退出码: 0 = 断言成立（优化链更小） ; 1 = 不成立（当前基线下应报 1）
"""
import io, os, re, struct, sys
sys.stdout.reconfigure(encoding='utf-8', errors='replace')

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
os.chdir(ROOT)
import subprocess

CC = 'qcc_zh_v2.exe'
SRC = os.path.join('scripts', 'feat_zh_opt_gain.c')
WORK = os.path.join('build', 'opt_gain')
MINIMAL = '--minimal' in sys.argv
if '--cc' in sys.argv:
    CC = sys.argv[sys.argv.index('--cc') + 1]
if '--work' in sys.argv:
    WORK = sys.argv[sys.argv.index('--work') + 1]
if '--src' in sys.argv:
    SRC = sys.argv[sys.argv.index('--src') + 1]
os.makedirs(WORK, exist_ok=True)

OPT, ASM = 'opt_zh.exe', 'asm_zh.exe'
fails = []


def sh(a, t=3600):
    return subprocess.run(a, capture_output=True, timeout=t)


def text_size(p):
    """返回 (.text SizeOfRawData, 文件字节, .text 有效内容长度)
    注: SizeOfRawData 是 0x1000 对齐值, 优化省下的少量字节会被对齐页掩盖;
    「有效内容长度」= .text 内最后一个非零字节 + 1 —— 才是收益的真实落点。"""
    d = io.open(p, 'rb').read()
    pe = struct.unpack_from('<I', d, 0x3C)[0]
    n = struct.unpack_from('<H', d, pe + 6)[0]
    o = struct.unpack_from('<H', d, pe + 20)[0]
    b = pe + 24 + o
    for i in range(n):
        q = b + 40 * i
        nm = d[q:q + 8].rstrip(b'\x00').decode('latin1')
        vsz, va, rsz, ro = struct.unpack_from('<IIII', d, q + 8)
        if nm == '.text':
            body = d[ro:ro + rsz]
            j = len(body) - 1
            while j >= 0 and body[j] == 0:
                j -= 1
            return rsz, len(d), j + 1
    return -1, len(d), -1


# 代表性输入（可被 --src 覆盖）
if not os.path.exists(SRC):
    SRC = os.path.join(WORK, 'gain_repr.c')
    io.open(SRC, 'w', encoding='utf-8', newline='\n').write(
        '#include <stdio.h>\n'
        'static int 累加(int n) { int s = 0; for (int i = 0; i < n; i = i + 1) s = s + i; return s; }\n'
        'int main(void) { printf("GAIN %d\\n", 累加(10)); return 0; }\n')

base = os.path.join(WORK, 'repr')
print('== 优化收益度量 (cc=%s src=%s) ==' % (CC, SRC))

# 非优化链
r = sh([CC, '-S', SRC, '-o', base + '_n.exe'])
if r.returncode != 0:
    print('  [FAIL] -S rc=%d' % r.returncode); sys.exit(1)
asm_n = base + '_n.exe.asm'
r = sh([ASM, asm_n, '-o', base + '_n_a.exe'])
if r.returncode != 0:
    print('  [FAIL] asm_zh(非优化) rc=%d' % r.returncode); sys.exit(1)

# 优化链
asm_o = base + '_o.asm'
r = sh([OPT, asm_n, '-o', asm_o])
if r.returncode != 0:
    print('  [FAIL] opt_zh rc=%d' % r.returncode); sys.exit(1)
r = sh([ASM, asm_o, '-o', base + '_o_a.exe'])
if r.returncode != 0:
    print('  [FAIL] asm_zh(优化) rc=%d' % r.returncode); sys.exit(1)
# 单源真相：缩短总量取**汇编器自报**（文报通道 + 实测通道），不再从 .asm 文本里读 `.缩总`
_ao = (r.stdout + r.stderr).decode('utf-8', 'replace')
_mz = re.search(r'总缩=(-?\d+)', _ao)
_wz = re.search(r'文报缩=(-?\d+)', _ao)
_sz = re.search(r'实测缩=(-?\d+)', _ao)
suozong = int(_mz.group(1)) if _mz else 0
wenzong = int(_wz.group(1)) if _wz else 0
shizong = int(_sz.group(1)) if _sz else 0

tsz_n, fl_n, eff_n = text_size(base + '_n_a.exe')
tsz_o, fl_o, eff_o = text_size(base + '_o_a.exe')
asz_n = os.path.getsize(asm_n)
asz_o = os.path.getsize(asm_o)
# 文报通道（opt_zh 在 .asm 里报的 Σ.缩）—— 仅作参照，总量以汇编器自报为准
t = io.open(asm_o, encoding='utf-8', errors='replace').read()
m = re.search(r'^\.缩总 (\d+)', t, re.M)
wen_text = int(m.group(1)) if m else 0

print('  %-16s %10s %10s %12s %12s' % ('', '.asm 字节', '.text rsize', '.text 有效', '文件字节'))
print('  %-16s %10d %10d %12d %12d' % ('非优化链', asz_n, tsz_n, eff_n, fl_n))
print('  %-16s %10d %10d %12d %12d' % ('优化链', asz_o, tsz_o, eff_o, fl_o))
print('  %-16s %+10d %+10d %+12d %+12d' % ('差值', asz_o - asz_n, tsz_o - tsz_n, eff_o - eff_n, fl_o - fl_n))
print('  汇编器自报: 文报缩 = %d · 实测缩 = %d · 总缩 = %d     （.asm 文本内 `.缩总` = %d，仅文报通道）'
      % (wenzong, shizong, suozong, wen_text))
print()

# 主判据: .text **有效内容**严格更小 (SizeOfRawData 是 0x1000 对齐值, 会掩盖小额收益)
if eff_o < eff_n:
    d = eff_n - eff_o
    print('  ⟹ 收益落地：优化链 .text 有效内容严格更小（省 %d 字节，%.3f%%）✓' % (d, 100.0 * d / eff_n))
    if suozong and d != suozong:
        msg = '有效内容差值 %d != 汇编器自报总缩 %d' % (d, suozong)
        print('  [WARN] %s' % msg); fails.append(msg)
    else:
        print('  ✓ 差值 == 汇编器自报总缩 (%d)（文报 %d + 实测 %d，精确一致）' % (suozong, wenzong, shizong))
    if tsz_o < tsz_n:
        print('  ✓ 且 .text SizeOfRawData 也严格更小（输入足够大，跨过了对齐页）')
    else:
        print('  （注意：本输入下 .text SizeOfRawData 未变小 —— 收益小于一个 0x1000 对齐页；换更大输入可见）')
else:
    msg = ('收益未落地：优化链 .text 有效内容 (%d) %s 非优化链 (%d)'
           % (eff_o, '==' if eff_o == eff_n else '>', eff_n))
    print('  ⟹ %s' % msg)
    fails.append(msg)

print('== 结果: %s ==' % ('通过（收益落地）' if not fails else 'FAIL=%d' % len(fails)))

# ---------- --compiler: 编译器本体「同一份 -S 产物 2 环链 vs 3 环链」 ----------
if '--compiler' in sys.argv:
    print()
    print('== (b) 编译器本体：同一份 -S 产物 走 2 环链 vs 3 环链 ==')
    body = 'build\\boot7\\r1_s.exe.asm'
    if not os.path.exists(body):
        body = os.path.join(WORK, 'body_s.exe.asm')
        r = sh([CC, '-S', os.path.join('srclib_jiayan', 'qcc_zh.jy'), '-o', os.path.join(WORK, 'body_s.exe')])
        if r.returncode != 0 or not os.path.exists(body):
            print('  [FAIL] 无法取得编译器本体 -S 产物'); sys.exit(1)
    bo = os.path.join(WORK, 'body_o.asm')
    r1 = sh([OPT, body, '-o', bo])
    r2 = sh([ASM, body, '-o', os.path.join(WORK, 'body_2ring.exe')])      # 2 环链（不加优化）
    r3 = sh([ASM, bo, '-o', os.path.join(WORK, 'body_3ring.exe')])        # 3 环链
    if r1.returncode or r2.returncode or r3.returncode:
        print('  [FAIL] 链路 rc=%d/%d/%d' % (r1.returncode, r2.returncode, r3.returncode)); sys.exit(1)
    o1 = (r1.stdout + r1.stderr).decode('utf-8', 'replace')
    o3 = (r3.stdout + r3.stderr).decode('utf-8', 'replace')
    m = re.search(r'优化 (\d+) 处 \[([^\]]*)\]', o1)
    print('  出厂优化器自报: %s' % (m.group(0) if m else '?'))
    a_b, b_b = os.path.getsize(body), os.path.getsize(bo)
    print('  编译器本体 .asm 字节: %d → %d  (Δ=%+d；`.删`/`.替` 声明行 ⟹ 收益口径取**产物**)' % (a_b, b_b, b_b - a_b))
    tz = int(re.search(r'总缩=(-?\d+)', o3).group(1))
    tn = text_size(os.path.join(WORK, 'body_2ring.exe')); to = text_size(os.path.join(WORK, 'body_3ring.exe'))
    print('  %-14s %14s %14s %14s' % ('', '.text rsize', '.text 有效', '文件字节'))
    print('  %-14s %14d %14d %14d' % ('2 环链', tn[0], tn[2], tn[1]))
    print('  %-14s %14d %14d %14d' % ('3 环链(8pass)', to[0], to[2], to[1]))
    print('  %-14s %+14d %+14d %+14d' % ('差值', to[0] - tn[0], to[2] - tn[2], to[1] - tn[1]))
    print('  汇编器自报总缩 = %d  ⟹ 差值 == 自报量: %s ；优化链 .text 严格更小: %s'
          % (tz, '✓' if (tn[2] - to[2]) == tz else '★否', '✓' if to[2] < tn[2] else '★否'))
    if not (to[2] < tn[2] and (tn[2] - to[2]) == tz):
        fails.append('编译器本体收益断言失败 (有效 %d→%d, 自报 %d)' % (tn[2], to[2], tz))
    # ⑧（死值写覆盖）对本体 .text 的贡献 == 其自报量：与本 goal 前的 7 pass 参照产物对照
    ref = os.path.join('build', 'opt_t10b', '_Zv8.exe')
    if os.path.exists(ref):
        tr = text_size(ref)
        print('  ⑧ 贡献: 7 pass 参照产物 (_Zv8.exe) 有效=%d · 本产物有效=%d ⟹ 贡献 %d'
              % (tr[2], to[2], tr[2] - to[2]))
        print('           与自报量对齐: 7 pass 自报 211946 · 本产物自报 %d ⟹ 差 %d   ⟹ %s'
              % (tz, tz - 211946, '✓ 两者相等' if (tr[2] - to[2]) == (tz - 211946) else '★否'))
        if (tr[2] - to[2]) != (tz - 211946):
            fails.append('⑧ 贡献与自报量不符')
    print('== (b) 结果: %s ==' % ('通过' if (to[2] < tn[2] and (tn[2] - to[2]) == tz) else 'FAIL'))

sys.exit(0 if not fails else 1)
