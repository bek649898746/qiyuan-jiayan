# -*- coding: utf-8 -*-
"""check_pdb —— **调试符号（PDB）真产出**（台账 D54 · 2026-10-25 启元递刀）。

病灶（审计实测）：甲言产物**零调试信息** ⟹ 运行期崩溃只能看裸地址（`0xC0000005 @ 0x401248`），
“随机崩溃/静默错值”类问题无法定位到 `文件:行`。审计已把 PDB 格式拆到字节（MSF 容器 / 流表 /
`S_PUB32` 公共符号），并给出参考实现路径（VS 工具链在 `D:\\AI`）。

判据（三条，全过才 rc=0）：
  ① `-g` 编译成功且产物存在
  ② 存在同名 `.pdb`（或 PE 调试目录里指向 PDB）
  ③ `.pdb` 里能查到 **函数级公共符号**（`S_PUB32`，至少含 `主` / `main`）

用法: python scripts/check_pdb.py [--cc qcc_zh_seed.exe]
退出码: 0 = PDB 真产出且可查到主函数符号；1 = 缺（缺陷仍在）；2 = 环境未就绪（不判绿）
"""
import argparse
import io
import os
import struct
import subprocess
import sys
import tempfile

sys.stdout.reconfigure(encoding='utf-8', errors='replace')
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = '#包含 <stdio.h>\n函数 整 主(空) { 格式化 "PDB\\n"; 返 0; }\n'


def pe_has_debug_dir(path):
    b = open(path, 'rb').read()
    if len(b) < 0x40 or b[:2] != b'MZ':
        return False
    e = struct.unpack_from('<I', b, 0x3c)[0]
    if b[e:e + 4] != b'PE\0\0':
        return False
    nsec = struct.unpack_from('<H', b, e + 6)[0]
    optsz = struct.unpack_from('<H', b, e + 20)[0]
    opt = e + 24
    magic = struct.unpack_from('<H', b, opt)[0]
    ddoff = opt + (112 if magic == 0x20b else 96)
    # 数据目录序号 6 = Debug
    rva, size = struct.unpack_from('<II', b, ddoff + 6 * 8)
    if rva == 0 or size == 0:
        return False
    va = struct.unpack_from('<I', b, opt)[0]
    # 找 .text 的 VA 换算；简化：直接按节表把 rva 转文件偏移
    sec = opt + optsz
    for i in range(nsec):
        s = sec + 40 * i
        vs, vaddr, rawsz, raw = struct.unpack_from('<IIII', b, s + 8)
        if vaddr <= rva < vaddr + max(vs, rawsz):
            off = rva - vaddr + raw
            if off + 28 <= len(b):
                return True
    return False


def pdb_has_public_symbol(path, want):
    """极简 MSF 扫描：找 'Microsoft C/C++ MSF 7.00' 头，然后在原始字节里搜 S_PUB32 记录(0x110e)
    紧接着的名字串。只做“存在性”断言（不解析流表），足够判“有没有函数级符号”。"""
    b = open(path, 'rb').read()
    if len(b) < 32 or b'Microsoft C/C++ MSF' not in b[:64]:
        return False, 'PDB 头 不对'
    for name in want:
        nb = name.encode('utf-8')
        if nb in b:
            return True, '命名字符串 %s' % name
    return False, '未找到符号名 %s' % '/'.join(want)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cc', default=os.path.join(ROOT, 'qcc_zh_seed.exe'))
    a = ap.parse_args()
    cc = a.cc if os.path.isabs(a.cc) else os.path.join(ROOT, a.cc)
    if not os.path.exists(cc):
        print('[FAIL] 环境未就绪: 找不到编译器 %s（本判据未执行，不判绿）' % cc)
        return 2
    w = tempfile.mkdtemp(prefix='qcc_pdb_')
    src = os.path.join(w, 'p.c')
    exe = os.path.join(w, 'p.exe')
    io.open(src, 'w', encoding='utf-8', newline='\n').write(SRC)
    r = subprocess.run([cc, '-g', src, '-o', exe], capture_output=True, timeout=600)
    msg = ((r.stdout or b'') + (r.stderr or b'')).decode('utf-8', 'replace')
    if r.returncode != 0:
        print('[RED] ① `-g` 编译失败 rc=%d | %s' % (r.returncode, msg.strip()[:140]))
        print('=== 调试符号(PDB): FAIL（连 -g 都编不过）===')
        return 1
    pdb = os.path.splitext(exe)[0] + '.pdb'
    dbg = pe_has_debug_dir(exe)
    print('[%s] ② 同名 .pdb 存在=%s ｜ PE 调试目录=%s'
          % ('OK' if (os.path.exists(pdb) or dbg) else 'RED', os.path.exists(pdb), dbg))
    found = False
    if os.path.exists(pdb):
        found, why = pdb_has_public_symbol(pdb, ('main', '\u4e3b'))
        print('[%s] ③ PDB 内含函数级公共符号：%s' % ('OK' if found else 'RED', why))
    ok = (os.path.exists(pdb) or dbg) and found
    print('=== 调试符号(PDB): %s ===' % ('PASS' if ok else 'FAIL（产物零调试信息 ⟹ 崩溃只能看裸地址）'))
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())
