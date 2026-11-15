# -*- coding: utf-8 -*-
"""中文汇编端到端 判据器 (R599c · plan 验收 3 的 gating 观测)。

断言链: 终态编译器 `-S` → 中文汇编器 `asm_zh` → 运行 ⟹ **输出内容与 编译器直产 逐字一致**,
并 附带 **指令级** 同源断言 (两 PE 的 .text 代码区 指令序列 完全相同)。

用法: python scripts/check_asm_e2e.py [--cc <编译器 exe>]
退出码: 0 全过 ; 1 有 断言 失败。
"""
import io, os, re, subprocess, sys, hashlib
sys.stdout.reconfigure(encoding='utf-8', errors='replace')

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
os.chdir(ROOT)
CC = 'qcc_zh_v2.exe'
if '--cc' in sys.argv:
    CC = sys.argv[sys.argv.index('--cc') + 1]

WORK = os.path.join('build', 'asm_e2e')
os.makedirs(WORK, exist_ok=True)

# 中文汇编器：不再假定它已在 cwd/PATH（原写法 `asm_zh.exe` 裸名 ⟹ 环境里没有就 WinError 2，
# 判据恒 FAIL 但原因与代码无关）。按仓库内的既有构建产物顺序解析。
_REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ASM = None


def _ensure_asm():
    """取一个**与当前编译器 -S 输出兼容**的 asm_zh：
    仓库里现成的两个构建（probes/S_text 10-16、build/asm_zh_c_build 11-06）都落后于
    编译器新加的指令（`存8` / `符号扩展64`）⟹ 以 `srclib/asm_zh.jy` 的新鲜度为准则，
    过期就用 `CC` 重建到 `build/asm_zh.exe`（构建产物，不入库）。"""
    src = os.path.join(_REPO, 'srclib', 'asm_zh.jy')
    out = os.path.join(_REPO, 'build', 'asm_zh.exe')
    try:
        fresh = os.path.exists(out) and os.path.exists(src) and os.path.getmtime(out) >= os.path.getmtime(src)
    except OSError:
        fresh = False
    if not fresh and os.path.exists(src):
        try:
            r = subprocess.run([CC, src, '-o', out], capture_output=True, timeout=1800)
            if r.returncode == 0 and os.path.exists(out):
                return out
        except Exception:
            pass
    for c in (out,
              os.path.join(_REPO, 'asm_zh.exe'),
              os.path.join(_REPO, 'build', 'asm_zh_c_build_20261106.exe'),
              os.path.join(_REPO, 'probes', 'S_text', 'asm_zh.exe')):
        if os.path.exists(c):
            return c
    return None


ASM = _ensure_asm()

CASES = [
    ('t_min.c', '        int main(void){return 42;}\n', 42, ''),
    ('t_prt.c', '        #include <stdio.h>\n        int main(void){ printf("CNE2E 42\\n"); return 0; }\n', 0, 'CNE2E 42'),
]
# 上面 用 缩进 只是 为了 让 本 文件 里 的 字面量 不 以 开头 空白 干扰 — 写盘 时 去 掉
CASES = [(n, s.replace('        ', ''), rc, out) for n, s, rc, out in CASES]


def run(exe):
    try:
        r = subprocess.run([exe], capture_output=True, timeout=60)
        return r.returncode, (r.stdout + r.stderr).decode('utf-8', 'replace')
    except subprocess.TimeoutExpired:
        return None, '<timeout>'


def code_insns(p):
    import pefile, capstone
    pe = pefile.PE(p, fast_load=True)
    va = d = None
    for s in pe.sections:
        if s.Name.rstrip(b'\x00') == b'.text':
            va, d = s.VirtualAddress, s.get_data()
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64)
    out = []
    for i in md.disasm(d, va):
        if i.address >= va + 271717:      # 到 码终 之前 (排除 尾部 数据)
            break
        out.append(i.mnemonic + ' ' + re.sub(r'0x[0-9a-f]+|\b\d+\b', 'N', i.op_str))
    return out


fails = []
print('== 中文汇编端到端 判据 (cc=%s) ==' % CC)
for name, src, erc, eout in CASES:
    p = os.path.join(WORK, name)
    io.open(p, 'w', encoding='utf-8', newline='\n').write(src)
    base = os.path.join(WORK, name[:-2])
    r = subprocess.run([CC, '-S', p, '-o', base + '.exe'], capture_output=True, timeout=600)
    if r.returncode != 0 or not os.path.exists(base + '.exe.asm'):
        fails.append('%s: 编译器 -S 失败 rc=%d' % (name, r.returncode))
        print('  [FAIL] %s 编译器 -S rc=%d' % (name, r.returncode))
        continue
    if ASM is None:
        fails.append('asm_zh 未找到（仓库内 asm_zh.exe / build\\ / probes\\S_text\\ 均无）')
        print('  [FAIL] asm_zh 未找到')
        break
    ra = subprocess.run([ASM, base + '.exe.asm', '-o', base + '_a.exe'], capture_output=True, timeout=900)
    if ra.returncode != 0 or not os.path.exists(base + '_a.exe'):
        fails.append('%s: asm_zh 汇编失败 rc=%d' % (name, ra.returncode))
        print('  [FAIL] %s asm_zh rc=%d' % (name, ra.returncode))
        continue
    rc1, o1 = run(base + '.exe')
    rc2, o2 = run(base + '_a.exe')
    ok_run = (rc1 == rc2) and (rc1 == erc) and (eout in o2)
    ia, ib = code_insns(base + '.exe'), code_insns(base + '_a.exe')
    ok_same = (ia == ib)
    print('  %s  直产 rc=%s out=%r | 汇编 rc=%s out=%r | 指令同源 %s (%d/%d)' % (
        name, rc1, o1[:40], rc2, o2[:40], ok_same, len(ia), len(ib)))
    if not ok_run:
        fails.append('%s: 运行/输出 断言 失败 (直产 rc=%s out=%r vs 汇编 rc=%s out=%r, 期望 rc=%s 含 %r)' % (
            name, rc1, o1, rc2, o2, erc, eout))
    if not ok_same:
        fails.append('%s: 指令级 同源 断言 失败 (%d vs %d 条)' % (name, len(ia), len(ib)))

print('== 结果: %s ==' % ('全部 通过' if not fails else 'FAIL=%d' % len(fails)))
for f in fails:
    print('   -', f)
sys.exit(0 if not fails else 1)
