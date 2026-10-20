# -*- coding: utf-8 -*-
"""check_objmode_unit — 对象模式判据（2026-10-21 虾米）

两类模式：
  A. **真实激励**（能让「修复被回退」当场变红）：
     --reloc-big   生成 >65,535 条重定位的源 ⟹ `.text` 节头写 0xFFFF（逃逸码），
                   且首条重定位 VirtualAddress 存**真实条数**（>0xFFFF）   [D-OBJ16]
     --runs-big    生成 40 个 4KB 静态块 ⟹ `.data$N` **≥38 个 run**、VirtualAddress 严格递增
                   （R800 前 `节容` 定长 40、按 `节数` 索引 ⟹ 40 run 就越界）      [R800]
     --sparse      ���成 64MB 静态数组 ⟹ 对象**远小于** 64MB（稀疏 run 流式写）
                   （D-OBJ 前按 `静态数*4` 全量分配/写出）                        [D-OBJ]
  B. 小输入守卫（快，但不单独作为上面三项的判据）：
     --basic / --addr / --repeat / --reloc / --runs / --symbols
     --symbols-big 34k 自身源 `-c` 符号表：同名/同址 重复定义 = 0        [R801]

用法: python scripts/check_objmode_unit.py [--cc <exe>] <mode>
"""
import argparse
import os
import struct
import subprocess
import sys
import tempfile

sys.stdout.reconfigure(encoding='utf-8', errors='replace')
REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
os.chdir(REPO)

SMALL = '整 主(空) {\n    整 甲 = 3;\n    整 乙 = 甲 * 7;\n    printf("OBJ %d\\n", 乙);\n    返 0;\n}\n'
ADDR = '整 主(空) {\n    整 (*p)(常 字 *, ...) = printf;\n    p("ADDR %d\\n", 7);\n    返 0;\n}\n'


def gen_reloc_big(nwrap=1000, calls=70):
    """真(整) 被 nwrap*calls 次调用 ⟹ 每次调用 1 条 REL32 重定位（70,000 条 > 65,535）。
    ⚠️ 函数个数受 `fn_param`(4096) / `fn_static`(1024) 两个资源护栏限制 ⟹ 用「少函数、多调用」凑重定位数。"""
    L = ['整 真(整 a) { 返 a + 1; }']
    for i in range(nwrap):
        body = ' + '.join('真(a + %d)' % (j + 1) for j in range(calls))
        L.append('整 包%d(整 a) { 返 %s; }' % (i, body))
    L.append('整 主(空) { 返 包0(1) + 包%d(2); }' % (nwrap - 1))
    return '\n'.join(L)


def gen_runs_big(n=40, gap=12288):
    """n 组「12KB 填充 + 1 个字符串指针数组」⟹ n 个**互不相邻**的 `.data` run。
    实测 5 组 → 5 个 run（`.data` + `.data$1..$4`，VirtualAddress 递增）。
    R800 前 `标叁/下布一/首重/…` 按 `数段容=40` 定长、却按 `节数=3+数段数` 索引 ⟹ 40 run（43 节）就越界。"""
    L = []
    for i in range(n):
        L.append('静 字 填%02d[%d]; 静 字 *针%02d[1] = {"x"};' % (i, gap, i))
    L.append('整 主(空) { 返 (整)针00[0][0]; }')
    return '\n'.join(L)


def gen_sparse(mb=64):
    return '静 字 大[%d] = "x";\n整 主(空) { 返 (整)大[0]; }\n' % (mb * 1024 * 1024)


def run(cc, args, timeout=900):
    return subprocess.run([cc] + args, capture_output=True, text=True,
                          encoding='utf-8', errors='replace', timeout=timeout)


def parse_sections(b):
    # COFF 目标文件（不是 PE）：Machine@0 / NumberOfSections@2 / SizeOfOptionalHeader@16 / 节表@20+optsz
    nsec = struct.unpack_from('<H', b, 2)[0]
    optsz = struct.unpack_from('<H', b, 16)[0]
    off = 20 + optsz
    out = []
    for i in range(nsec):
        base = off + 40 * i
        name = b[base:base + 8].rstrip(b'\0').decode('latin1')
        vsize, vaddr, rawsz, rawptr, relptr, lnptr = struct.unpack_from('<IIIIII', b, base + 8)
        nrel, nln = struct.unpack_from('<HH', b, base + 32)
        out.append(dict(name=name, vsize=vsize, vaddr=vaddr, rawsz=rawsz, relptr=relptr, nrel=nrel))
    return out, struct.unpack_from('<I', b, 8)[0], struct.unpack_from('<I', b, 12)[0]


def parse_syms(b, symptr, nsym):
    out, i = [], 0
    while i < nsym:
        base = symptr + 18 * i
        out.append(dict(name=b[base:base + 8], value=struct.unpack_from('<I', b, base + 8)[0],
                        sec=struct.unpack_from('<h', b, base + 12)[0],
                        type=struct.unpack_from('<H', b, base + 14)[0], cls=b[base + 16]))
        i += 1 + b[base + 17]
    return out


def compile_obj(cc, src, obj, timeout=900):
    r = run(cc, ['-c', src, '-o', obj], timeout=timeout)
    if r.returncode != 0:
        print('[FAIL] -c rc=%d\n%s' % (r.returncode, ((r.stdout or '') + (r.stderr or ''))[-500:]))
        return None
    return open(obj, 'rb').read()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cc', default='qcc_zh_seed.exe')
    for m in ('basic', 'reloc', 'runs', 'symbols', 'symbols-big', 'addr', 'repeat',
              'reloc-big', 'runs-big', 'sparse'):
        ap.add_argument('--' + m, action='store_true')
    a = ap.parse_args()
    modes = [m for m in ('basic', 'reloc', 'runs', 'symbols', 'symbols-big', 'addr', 'repeat',
                         'reloc-big', 'runs-big', 'sparse') if getattr(a, m.replace('-', '_'))]
    mode = modes[0] if modes else 'basic'
    d = tempfile.mkdtemp(prefix='objunit_')

    if mode == 'addr':
        src, exe = os.path.join(d, 'a.jy'), os.path.join(d, 'a.exe')
        open(src, 'w', encoding='utf-8-sig').write(ADDR)
        r = run(a.cc, [src, '-o', exe])
        if r.returncode != 0:
            print('[FAIL] 编译失败 rc=%d\n%s' % (r.returncode, ((r.stdout or '') + (r.stderr or ''))[-400:]))
            return 1
        rr = subprocess.run([exe], capture_output=True, text=True, encoding='utf-8', errors='replace')
        got = ((rr.stdout or '') + (rr.stderr or '')).strip()
        ok = (rr.returncode == 0 and 'ADDR 7' in got)
        print('[%s] &printf 经函数指针调用：rc=%d out=%r' % ('PASS' if ok else 'FAIL', rr.returncode, got[:40]))
        print('=== ADDR-BUILTIN 判据: %s ===' % ('PASS' if ok else 'FAIL'))
        return 0 if ok else 1

    if mode == 'repeat':
        src, exe = os.path.join(d, 'r.jy'), os.path.join(d, 'r.exe')
        open(src, 'w', encoding='utf-8-sig').write(SMALL)
        r = run(a.cc, ['--repeat', '2', src, '-o', exe], timeout=600)
        out = ((r.stdout or '') + (r.stderr or ''))
        ok = (r.returncode == 0 and 'REPEAT-FAIL' not in out)
        print('[%s] --repeat 2 rc=%d %s' % ('PASS' if ok else 'FAIL', r.returncode, out.strip()[-120:]))
        print('=== D38 判据（重定位复位）: %s ===' % ('PASS' if ok else 'FAIL'))
        return 0 if ok else 1

    # 其余模式：先按模式生成源
    if mode == 'reloc-big':
        text = gen_reloc_big()
    elif mode == 'runs-big':
        text = gen_runs_big()
    elif mode == 'sparse':
        text = gen_sparse()
    else:
        text = SMALL
    src, obj = os.path.join(d, 's.jy'), os.path.join(d, 's.o')
    if mode == 'symbols-big':
        src = os.path.join('srclib_jiayan', 'qcc_zh.jy')   # 34k 自身源（不复制，直接编）
        print('# 输入 34k 自身源 %s (%s)' % (src, mode))
    else:
        open(src, 'w', encoding='utf-8-sig').write(text)
        print('# 输入 %d 字节 (%s)' % (len(text), mode))
    b = compile_obj(a.cc, src, obj)
    if b is None:
        return 1
    secs, symptr, nsym = parse_sections(b)
    names = [s['name'] for s in secs]

    if mode == 'basic':
        ok = (len(secs) >= 4 and '.text' in names and secs[0]['rawsz'] > 0 and len(b) > 512)
        print('[%s] -c 产物 %d 字节 / %d 节 / .text=%s' % ('PASS' if ok else 'FAIL', len(b), len(secs),
                                                          secs[0]['rawsz'] if secs else 0))
        print('=== 小对象冒烟: %s ===' % ('PASS' if ok else 'FAIL'))
        return 0 if ok else 1

    if mode in ('reloc', 'reloc-big'):
        text_sec = next((s for s in secs if s['name'] == '.text'), None)
        sent = None
        if text_sec and text_sec['nrel'] == 0xFFFF and text_sec['relptr']:
            sent = struct.unpack_from('<I', b, text_sec['relptr'])[0]
        if mode == 'reloc-big':
            ok = (text_sec is not None and text_sec['nrel'] == 0xFFFF and sent is not None and sent > 0xFFFF)
            print('[%s] .text nrel=0x%X 哨兵=%s（需 0xFFFF 且 哨兵 >65535）'
                  % ('PASS' if ok else 'FAIL', text_sec['nrel'] if text_sec else -1, sent))
            print('=== D-OBJ16 判据（>65535 重定位 逃逸码）: %s ===' % ('PASS' if ok else 'FAIL'))
        else:
            ok = (text_sec is not None and text_sec['nrel'] <= 0xFFFF)
            print('[%s] 小输入 .text nrel=0x%X（冒烟，不作 D-OBJ16 判据）'
                  % ('PASS' if ok else 'FAIL', text_sec['nrel'] if text_sec else -1))
            print('=== 小对象冒烟: %s ===' % ('PASS' if ok else 'FAIL'))
        return 0 if ok else 1

    if mode in ('runs', 'runs-big'):
        runs = [s for s in secs if s['name'].startswith('.data$')]
        data = [s for s in secs if s['name'] == '.data']
        asc = all(runs[i]['vaddr'] < runs[i + 1]['vaddr'] for i in range(len(runs) - 1))
        if mode == 'runs-big':
            ok = (len(runs) >= 38 and asc and bool(data) and data[0]['vsize'] >= runs[-1]['vaddr'])
            print('[%s] .data$N=%d（需 ≥38）递增=%s .data vsize=%s 末 run VA=%s'
                  % ('PASS' if ok else 'FAIL', len(runs), asc,
                     data[0]['vsize'] if data else None, runs[-1]['vaddr'] if runs else None))
            print('=== R800 判据（≥38 run 不越界/顺序正确）: %s ===' % ('PASS' if ok else 'FAIL'))
        else:
            ok = bool(data) and asc
            print('[%s] .data vsize=%s runs=%d 递增=%s（冒烟，不作 R800 判据）'
                  % ('PASS' if ok else 'FAIL', data[0]['vsize'] if data else None, len(runs), asc))
            print('=== 小对象冒烟: %s ===' % ('PASS' if ok else 'FAIL'))
        return 0 if ok else 1

    if mode == 'sparse':
        data = next((s for s in secs if s['name'] == '.data'), None)
        want = 64 * 1024 * 1024
        ok = (data is not None and data['vsize'] >= want and len(b) < 30 * 1024 * 1024)
        print('[%s] .data vsize=%.1fMB（需 ≥64MB）· 对象 %.1fMB（需 <30MB，稀疏流式写）'
              % ('PASS' if ok else 'FAIL', (data['vsize'] if data else 0) / 1048576,
                 len(b) / 1048576))
        print('=== D-OBJ 判据（64MB 稀疏静态 ⟹ 对象不膨胀）: %s ===' % ('PASS' if ok else 'FAIL'))
        return 0 if ok else 1

    if mode == 'symbols':
        syms = parse_syms(b, symptr, nsym) if symptr and nsym else []
        funcs = [s for s in syms if s['cls'] == 2 and (s['type'] & 0x20) and s['sec'] > 0]
        seen, dup = {}, []
        for s in funcs:
            if s['value'] in seen:
                dup.append((seen[s['value']], s['name']))
            seen[s['value']] = s['name']
        ok = not dup
        print('[%s] 已定义函数符号 %d 个；重复地址 %d 对' % ('PASS' if ok else 'FAIL', len(funcs), len(dup)))
        print('=== 小对象冒烟（符号去重）: %s ===' % ('PASS' if ok else 'FAIL'))
        return 0 if ok else 1

    if mode == 'symbols-big':
        # R801（真激励）：34k 自身源 `-c` 的符号表 —— 同一 **名字** 被 重复 定义
        #   （历史 351 条：重载条目 在 4 千函数级 时 被 二次登记 ⟹ 链接器 报 duplicate symbol）。
        #   小输入 只 1 个函数符号 ⟹ 冒烟 不能 证伪，必须 用 34k 源。
        syms = parse_syms(b, symptr, nsym) if symptr and nsym else []
        # 长名（>8 字节）在 COFF 里是 4 个 0 + 4 字节 字符串表偏移 —— 必须解析字符串表，
        # 否则 所有长名 都退化成同一串 垃圾 ⟹ 假「重名」（首版 判据 就这么 假红过 3 条）。
        strtab = b[symptr + 18 * nsym:] if symptr and nsym else b''

        def sym_name(raw):
            if raw[:4] == b'\x00\x00\x00\x00':
                off = struct.unpack_from('<I', raw, 4)[0]
                if off < len(strtab):
                    end = strtab.find(b'\x00', off)
                    return strtab[off:end if end >= 0 else len(strtab)].decode('utf-8', 'replace')
                return ''
            return raw.rstrip(b'\x00').decode('utf-8', 'replace')

        defined = [s for s in syms if s['cls'] in (2, 3) and s['sec'] > 0 and s['name'][:1] != b'.']
        names, dups = {}, []
        for s in defined:
            nm = sym_name(s['name'])
            nm = nm.split('\x00')[0]
            if not nm or nm.startswith('.') or nm.startswith('$'):
                continue
            key = (s['cls'], nm)
            if key in names:
                dups.append('%s(cls=%d)' % (nm, s['cls']))
            names[key] = s['value']
        addrs, dupaddr = set(), 0
        for s in defined:
            # 同址 必须 **同节** 才算 重复：不同 节 的 偏移 各 从 0 起，天然 同号。
            key = (s['sec'], s['value'])
            if key in addrs:
                dupaddr += 1
            addrs.add(key)
        ok = (not dups) and dupaddr == 0
        print('[%s] 34k 对象符号表: 已定义符号 %d 个 · 重名 %d 条 · 同址 %d 条'
              % ('PASS' if ok else 'FAIL', len(defined), len(dups), dupaddr))
        for nm in dups[:10]:
            print('   重名: %s' % nm)
        print('=== R801 判据（34k `-c` 符号去重）: %s ===' % ('PASS' if ok else 'FAIL'))
        return 0 if ok else 1

    return 0


if __name__ == '__main__':
    sys.exit(main())
