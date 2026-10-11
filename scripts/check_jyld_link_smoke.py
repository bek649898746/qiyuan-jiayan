# -*- coding: utf-8 -*-
"""check_jyld_link_smoke —— **`-c` 产出物 + jyld 链接 冒烟**（闸⑤ 候选）。

背景（R543 · 2026-10-07 虾米）：
  `run_tests_zh.py` 的锚**全部**走「链接模式」（编译器 一步 产 PE）；闸② 只验 `-c` 能**编译出 .o**。
  **没有任何 自动检查 把 .o 交给 `jyld` 链接 并 运行** ⟹ 族12 #241「链接模型完整」/ #254「静态库归档」
  在 闸内 恒为 🟡（可 真链路 长期 坏 而 无人 知）。

  实测（本轮 现场）：`int main(void){ return 42; }` → `qcc -c` → `jyld x.o -o x.exe` ⟹ 链接 rc=0，
  但 **运行 返 0x2CE000 而 非 42**；直连 `qcc x.c -o x.exe` ⟹ 正确。根因 = `srclib/jyld.c` 的
  `k32_names[]` 前 8 项被 C10f 插入了 `LoadLibraryA`（索引 4）⟹ 后 8 项全体后移一 ⟹ CRT 桩
  硬编码 槽 6/7（ExitProcess / GetCommandLineA）**错位** ⟹ 进程 在 argv 解析处 call 到 ExitProcess
  **提前退出**（退码 = 垃圾地址、无输出）。已修（移出 前 8）。

规则：① 单 .o 链接 运行 断言 退码；② 跨 .o（a.o 提供符号 + b.o 调用）断言 stdout；
      ③ **归档往返**（.o → `.a`（本脚本 自写 GNU ar 格式）→ jyld 链接）断言 stdout。
      任一 失败 ⟹ 判红（这类 缺陷 = 「闸 够不着 即 盲区」）。
用法：python scripts/check_jyld_link_smoke.py [编译器]
退出码：0 = 全过（或 jyld 不可用 = SKIP）；1 = 有 失败
"""

# R715 仪器刀 (2026-10-11 虾米): 默认控制台 GBK 时 emoji(✅/❌) 会让 print 抛 UnicodeEncodeError
#   ⟹ 检查器 rc=1 假红 ⟹ 闸⑥/三套件 被中断。强推 UTF-8 输出（Python 3.7+，非 tty 管道下同样生效），
#   不改任何 print 文本；调用侧（verify.ps1 / run_tests_zh.py）另设 PYTHONIOENCODING=utf-8 作兜底。
import sys as _sys_utf8w
for _stream_utf8w in (getattr(_sys_utf8w, 'stdout', None), getattr(_sys_utf8w, 'stderr', None)):
    if _stream_utf8w is not None and hasattr(_stream_utf8w, 'reconfigure'):
        _stream_utf8w.reconfigure(encoding='utf-8', errors='replace')
import io, os, subprocess, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CC = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, 'qcc_zh_v1.exe')
JYLD = os.path.join(ROOT, 'srclib', 'jyld.exe')
OUT = os.path.join(ROOT, 'scratch_c1b2' if os.path.isdir(os.path.join(ROOT, 'scratch_c1b2')) else '.', 'jyldsmoke')
os.makedirs(OUT, exist_ok=True)


def ensure_jyld():
    if os.path.exists(JYLD):
        return True
    src = os.path.join(ROOT, 'srclib', 'jyld.c')
    if not os.path.exists(src):
        return False
    try:
        r = subprocess.run(['gcc', '-O2', '-w', src, '-o', JYLD], capture_output=True, timeout=600)
    except Exception:
        return False
    return r.returncode == 0 and os.path.exists(JYLD)


def w(name, text):
    p = os.path.join(OUT, name)
    io.open(p, 'w', encoding='utf-8-sig', newline='\n').write(text)
    return p


def comp(src, obj):
    r = subprocess.run([CC, src, '-c', '-o', obj], capture_output=True, timeout=300)
    return r.returncode == 0, (r.stdout + r.stderr).decode('utf-8', 'replace').strip()


def link(objs, exe):
    r = subprocess.run([JYLD] + objs + ['-o', exe], capture_output=True, timeout=300)
    return r.returncode == 0, (r.stdout + r.stderr).decode('utf-8', 'replace').strip()


def run(exe):
    try:
        r = subprocess.run([exe], capture_output=True, timeout=60)
        return r.returncode, (r.stdout or b'').decode('utf-8', 'replace').strip()
    except Exception as e:
        return 'ERR', str(e)


def make_ar(path, members, syms=None):
    """GNU ar 格式: !<arch>\\n + (可选 `/` 符号表成员) + 逐成员 60B 头 (name16 mtime12 uid6 gid6 mode8 size10 + `\\n).

    syms: [(成员序号, 符号名)] —— 有则 先写 `/` 符号表成员 (大端 count + N×大端 成员头偏移 + N×NUL 结尾名),
    否则 jyld 无法把 undefined 符号 映到成员 (实测: undefined symbol)。"""
    out = bytearray(b'!<arch>\n')
    n = len(members)
    if syms:
        symdata_len = 4 + 4 * len(syms) + sum(len(s.encode('utf-8')) + 1 for _, s in syms)
        pad = symdata_len & 1
        first_hdr = 8 + 60 + symdata_len + pad
        # 逐成员头偏移
        offs = []
        cur = first_hdr
        for nm, data in members:
            offs.append(cur)
            cur += 60 + len(data) + (len(data) & 1)
        sym = bytearray()
        sym += len(syms).to_bytes(4, 'big')
        for mi, _s in syms:
            sym += offs[mi].to_bytes(4, 'big')
        for _mi, s in syms:
            sym += s.encode('utf-8') + b'\x00'
        hdr = (b'/'.ljust(16) + b'0'.ljust(12) + b'0'.ljust(6) + b'0'.ljust(6)
               + b'100644'.ljust(8) + str(len(sym)).ljust(10).encode('ascii') + b'\x60\x0a')
        out += hdr + bytes(sym)
        if pad:
            out += b'\n'
    for nm, data in members:
        hdr = (nm + '/').ljust(16)[:16].encode('ascii')
        hdr += b'0'.ljust(12) + b'0'.ljust(6) + b'0'.ljust(6) + b'100644'.ljust(8)
        hdr += str(len(data)).ljust(10).encode('ascii') + b'\x60\x0a'
        assert len(hdr) == 60, len(hdr)
        out += hdr + data
        if len(data) & 1:
            out += b'\n'
    io.open(path, 'wb').write(bytes(out))


if not ensure_jyld():
    print('[check_jyld_link_smoke] ⚠️ jyld 不可用 (无 srclib/jyld.exe 且 无法 gcc 构建) —— SKIP (不判红)')
    sys.exit(0)

bad = []

# ① 单 .o：退码 断言 (无 printf ⟹ 不 依赖 导入表)
s = w('one.c', 'int main(void) { return 42; }\n')
o = os.path.join(OUT, 'one.o')
ok, msg = comp(s, o)
if not ok:
    print('%-10s -c FAIL | %s' % ('one', msg[:110]))
    bad.append('one(-c)')
else:
    ok, lm = link([o], os.path.join(OUT, 'one.exe'))
    rc, out = run(os.path.join(OUT, 'one.exe')) if ok else ('-', '')
    good = ok and rc == 42
    print('%-10s 单.o: %s (rc=%s, 期望 42)%s' % ('one', 'OK' if good else 'FAIL', rc, '' if good else '  | ' + lm[:90]))
    if not good:
        bad.append('one(link/run)')

# ② 跨 .o + printf
pa = w('prov.c', '整 加三(整 x) { 返 x + 3; }\n')
pb = w('use.c', '# 包含 <stdio.h>\n整 加三(整 x);\n函数 整 主(空) { 格式化 "CROSS %d\\n", 加三(4); 返 0; }\n')
oa, ob = os.path.join(OUT, 'prov.o'), os.path.join(OUT, 'use.o')
oka, ma = comp(pa, oa)
okb, mb = comp(pb, ob)
if not (oka and okb):
    print('%-10s -c FAIL | %s %s' % ('cross', ma[:60], mb[:60]))
    bad.append('cross(-c)')
else:
    ok, lm = link([oa, ob], os.path.join(OUT, 'cross.exe'))
    rc, out = run(os.path.join(OUT, 'cross.exe')) if ok else ('-', '')
    good = ok and rc == 0 and out == 'CROSS 7'
    print('%-10s 跨.o: %s (rc=%s out=%r, 期望 rc=0 out=CROSS 7)%s' % ('cross', 'OK' if good else 'FAIL', rc, out, '' if good else '  | ' + lm[:90]))
    if not good:
        bad.append('cross(link/run)')

# ③ 归档 往返 (.a 自写) + printf
if oka:
    lib = os.path.join(OUT, 'libprov.a')
    make_ar(lib, [('prov.o', open(oa, 'rb').read())], syms=[(0, u'加三')])
    ok, lm = link([ob, lib], os.path.join(OUT, 'arch.exe'))
    rc, out = run(os.path.join(OUT, 'arch.exe')) if ok else ('-', '')
    good = ok and rc == 0 and out == 'CROSS 7'
    print('%-10s 归档: %s (rc=%s out=%r, 期望 rc=0 out=CROSS 7)%s' % ('arch', 'OK' if good else 'FAIL', rc, out, '' if good else '  | ' + lm[:90]))
    if not good:
        bad.append('arch(link/run)')

print()
print('[check_jyld_link_smoke] 3 用例 · 失败 %d' % len(bad))
if bad:
    print('  ❌ 失败: %s' % ' '.join(bad))
    sys.exit(1)
print('  → 全过')
sys.exit(0)
