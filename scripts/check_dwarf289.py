# -*- coding: utf-8 -*-
"""闸④ #289 调试信息 (DWARF) 独立验证 —— 族15 缺口「本体真但零独立验证」的落地检查器。

判据 (对齐 DWARF v4 规范):
  1. `-g` 产物 必须 含 .debug_abbrev/.debug_info/.debug_line 三节 (COFF 长名 `/N` 经字符串表解析)。
  2. CU 的 DW_AT_name = 用户源文件名 (不含路径)。
  3. .debug_line 的 file_names 真含 两个条目: 用户源 + 前插运行时源 (qcc_rt.c)。
  4. 用户函数 (甲/乙/主) 的 DIE: DW_AT_decl_file = 1 (用户文件), DW_AT_decl_line = **真源行号**
     (检查器 现场 反查 锚文件, 不写死任何数字)。
  5. 运行时函数 DIE: DW_AT_decl_file = 2, 行号 落在 运行时源 的 合法 行区间。
  6. 行号程序 可解码, 且 `主` 内 `格式化` 语句行 有 对应 行表行 (地址 落在 `主` 的 [low_pc, +size) 内)。
  7. 无 任何 file=1 的行 超出 用户源 行数 (= 行号 已 扣除 前插运行时 偏移)。

修复前 (不动点 BDFC5F8B): 用户函数 decl_line = 1158/1162/1166 (远大于 锚文件 20 行),
且 file_names 只有 1 个条目 ⟹ 断言 3/4/5/7 必红。
"""
import io
import os
import subprocess
import sys

try:
    sys.stdout.reconfigure(encoding='utf-8')
except Exception:
    pass

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ANCHOR = os.path.join('scripts', 'feat_zh_dwarf289_ok.c')
RT = os.path.join('srclib', 'qcc_rt.c')

AT_NAME, AT_LOW_PC, AT_HIGH_PC, AT_DECL_LINE, AT_DECL_FILE = 3, 0x11, 0x12, 0x3b, 0x3a


def find_qcc():
    env = os.environ.get('QCC_TEST_BIN')
    if env and os.path.exists(env):
        return env
    for n in range(7, 0, -1):
        p = os.path.join(ROOT, 'qcc_zh_v%d.exe' % n)
        if os.path.exists(p):
            return p
    for nm in ('qcc_zh_seed.exe', 'qcc_x86.exe'):
        p = os.path.join(ROOT, nm)
        if os.path.exists(p):
            return p
    return 'qcc_x86.exe'


# ---------- PE / COFF ----------
def u(b, o, n):
    return int.from_bytes(b[o:o + n], 'little')


def parse_pe(b):
    pe = u(b, 0x3C, 4)
    if b[pe:pe + 4] != b'PE\0\0':
        raise ValueError('PE 签名缺失')
    nsec = u(b, pe + 6, 2)
    optsz = u(b, pe + 20, 2)
    symptr = u(b, pe + 12, 4)
    nsym = u(b, pe + 16, 4)
    opt = pe + 24
    magic = u(b, opt, 2)
    baseofcode = u(b, opt + 20, 4)
    imagebase = u(b, opt + 24, 8) if magic == 0x20b else u(b, opt + 28, 4)
    st = symptr + 18 * nsym
    strsz = u(b, st, 4) if 0 < st < len(b) - 4 else 0
    strtab = b[st:st + strsz]

    def longname(nm8):
        if nm8.startswith(b'/'):
            try:
                off = int(nm8[1:].split(b'\0')[0].decode('latin1'))
            except ValueError:
                return nm8.split(b'\0')[0].decode('latin1')
            e2 = strtab.index(b'\0', off) if off < len(strtab) else off
            return strtab[off:e2].decode('latin1')
        return nm8.split(b'\0')[0].decode('latin1')

    secs = {}
    so = opt + optsz
    for i in range(nsec):
        e = so + i * 40
        secs[longname(b[e:e + 8])] = dict(va=u(b, e + 12, 4), vsize=u(b, e + 8, 4),
                                          rawptr=u(b, e + 20, 4), rawsize=u(b, e + 16, 4))
    return dict(magic=magic, baseofcode=baseofcode, imagebase=imagebase, secs=secs)


def sec_data(b, pe, name):
    s = pe['secs'][name]
    return b[s['rawptr']:s['rawptr'] + s['rawsize']]


# ---------- DWARF 基本解码 ----------
def read_uleb(d, i):
    v, sh = 0, 0
    while True:
        x = d[i]; i += 1
        v |= (x & 0x7f) << sh
        if not (x & 0x80):
            break
        sh += 7
    return v, i


def read_sleb(d, i):
    v, sh, x = 0, 0, 0
    while True:
        x = d[i]; i += 1
        v |= (x & 0x7f) << sh
        sh += 7
        if not (x & 0x80):
            break
    if sh < 64 and (x & 0x40):
        v -= (1 << sh)
    return v, i


def parse_abbrev(d):
    out, i = {}, 0
    while i < len(d):
        code = d[i]; i += 1
        if code == 0:
            break
        tag = d[i]; i += 1
        ch = d[i]; i += 1
        attrs = []
        while True:
            an = d[i]; i += 1
            af = d[i]; i += 1
            if an == 0 and af == 0:
                break
            attrs.append((an, af))
        out[code] = (tag, ch, attrs)
    return out


def read_form(d, i, form, asz):
    if form == 0x01:
        return u(d, i, asz), i + asz
    if form == 0x05:
        return u(d, i, 2), i + 2
    if form == 0x06:
        return u(d, i, 4), i + 4
    if form == 0x07:
        return u(d, i, 8), i + 8
    if form in (0x0b, 0x0c):
        return d[i], i + 1
    if form == 0x0d:
        return u(d, i, 2), i + 2
    if form == 0x0e:
        return u(d, i, 4), i + 4
    if form == 0x0f:
        return read_uleb(d, i)
    if form == 0x08:
        j = d.index(b'\0', i)
        return d[i:j].decode('utf-8', 'replace'), j + 1
    raise ValueError('未支持 form %#x' % form)


def parse_info(d, ab):
    cu, dies, i = None, [], 0
    while i + 11 <= len(d):
        ulen = u(d, i, 4)
        if ulen == 0:
            break
        end = i + 4 + ulen
        cu = dict(off=i, end=end, unit_length=ulen, version=u(d, i + 4, 2),
                  abbrev_offset=u(d, i + 6, 4), addr_size=d[i + 10])
        j = i + 11
        depth = 0
        while j < end:
            code, j = read_uleb(d, j)
            if code == 0:
                depth -= 1
                if depth < 0:
                    break
                continue
            if code not in ab:
                break
            tag, ch, attrs = ab[code]
            die = dict(code=code, tag=tag, attrs={})
            for an, af in attrs:
                v, j = read_form(d, j, af, cu['addr_size'])
                die['attrs'][an] = v
            dies.append(die)
            if ch:
                depth += 1
        i = end
    if cu is not None and dies and dies[0]['tag'] == 0x11:
        cu['attrs'] = dies[0]['attrs']            # CU 头 + 紧随的 compile_unit DIE
    return cu, dies


def line_header(dline):
    """返回 (prog_off, file_names)。规范布局: 0 unit_length / 4 version / 6 header_length /
    10 min_inst / 11 max_ops / 12 default_is_stmt / 13 line_base / 14 line_range / 15 opcode_base /
    16.. standard_opcode_lengths / include_dirs / file_names。"""
    hl = u(dline, 6, 4)
    prog = 10 + hl
    opcode_base = dline[15]
    i = 16 + (opcode_base - 1)
    while True:                                    # include_directories
        j = dline.index(b'\0', i)
        if j == i:
            i += 1
            break
        i = j + 1
    names = []
    while True:                                    # file_names
        j = dline.index(b'\0', i)
        if j == i:
            i += 1
            break
        names.append(dline[i:j].decode('utf-8', 'replace'))
        i = j + 1
        for _ in range(3):
            _, i = read_uleb(dline, i)
    return prog, names


def decode_lines(dline, addr_size):
    """DWARF v4 行号状态机 -> [(address, file, line)]。"""
    prog, _ = line_header(dline)
    # R575 (2026-10-08 虾米 · 批C-AB) 修正: DWARF 的 unit_length **不 含** 其 自身 4 字节, 故 单元 结束 偏移
    #   = 4 + unit_length（原 写 10 + unit_length ⟹ 多 走 6 字节 ⟹ 当 .debug_line 程序 尺寸 距 0x200 边界
    #   < 6 字节 时 直接 走进 填充 区 后 越界, 抛 IndexError = **假红**（本批 现场 命中: 程序 5117B, 段 5120B）。
    end = 4 + u(dline, 0, 4)
    opcode_base = dline[15]
    line_base = dline[13] - 256
    line_range = dline[14]
    min_inst = dline[10]
    std_len = list(dline[16:16 + opcode_base - 1])
    q = prog
    rows = []
    addr, fileno, line = 0, 1, 1
    while q < end:
        op = dline[q]; q += 1
        if op == 0:
            ln, q = read_uleb(dline, q)
            sub = dline[q]; q += 1
            rest = q + ln - 1
            if sub == 1:
                rows.append((addr, fileno, line))
                addr, fileno, line = 0, 1, 1
            elif sub == 2:
                addr = u(dline, q, addr_size)
            q = rest
        elif op < opcode_base:
            if op == 1:
                rows.append((addr, fileno, line))
            elif op == 2:
                v, q = read_uleb(dline, q)
                addr += v
            elif op == 3:
                v, q = read_sleb(dline, q)
                line += v
            elif op == 4:
                fileno, q = read_uleb(dline, q)
            elif op == 5:
                _, q = read_uleb(dline, q)
            elif op == 8:
                addr += (255 - opcode_base) // line_range * min_inst
            elif op == 9:
                adv = u(dline, q, 2); q += 2
                addr += adv
            else:
                for _ in range(std_len[op - 1] if op - 1 < len(std_len) else 0):
                    _, q = read_uleb(dline, q)
        else:
            adj = op - opcode_base
            addr += (adj // line_range) * min_inst
            line += line_base + (adj % line_range)
            rows.append((addr, fileno, line))
    return rows


# ---------- 断言 ----------
FAILS = []


def ck(cond, msg):
    print(('  [PASS] ' if cond else '  [FAIL] ') + msg)
    if not cond:
        FAILS.append(msg)


def main():
    os.chdir(ROOT)
    qcc = find_qcc()
    outdir = os.path.join(ROOT, 'scratch_test')
    os.makedirs(outdir, exist_ok=True)
    exe = os.path.join(outdir, 'dwarf289_check.exe')
    if os.path.exists(exe):
        os.remove(exe)
    r = subprocess.run([qcc, '-g', ANCHOR, '-o', exe], capture_output=True, text=True,
                       encoding='utf-8', errors='replace')
    if r.returncode != 0 or not os.path.exists(exe):
        print('[闸④ #289 DWARF] ❌ 编译失败 rc=%d 编译器=%s\n%s'
              % (r.returncode, qcc, (r.stdout or '') + (r.stderr or '')))
        return 1
    b = open(exe, 'rb').read()
    pe = parse_pe(b)

    ck(all(n in pe['secs'] for n in ('.debug_abbrev', '.debug_info', '.debug_line')),
       '三调试节齐备 (.debug_abbrev/.debug_info/.debug_line)')
    if FAILS:
        return 1
    ab = parse_abbrev(sec_data(b, pe, '.debug_abbrev'))
    cu, dies = parse_info(sec_data(b, pe, '.debug_info'), ab)
    dline = sec_data(b, pe, '.debug_line')

    src_lines = io.open(ANCHOR, encoding='utf-8-sig', newline='').read().split('\r\n')
    if len(src_lines) <= 1:
        src_lines = io.open(ANCHOR, encoding='utf-8-sig').read().split('\n')
    n_user = len(src_lines)
    n_rt = len(io.open(RT, encoding='utf-8', errors='replace').read().split('\n'))
    rt_lines = io.open(RT, encoding='utf-8', errors='replace').read().split('\n')

    def rt_line_of(needle):
        for i, l in enumerate(rt_lines, 1):
            if needle in l:
                return i
        return -1
    base = os.path.basename(ANCHOR)

    def line_of(needle):
        for i, l in enumerate(src_lines, 1):
            if needle in l:
                return i
        return -1

    exp = {nm: line_of('函数 整 %s(' % nm) for nm in ('甲', '乙', '主')}
    exp_stmt = line_of('格式化 "DW289')

    ck(cu is not None and cu['attrs'].get(AT_NAME) == base,
       'CU 名 = 用户源文件名 %r (实得 %r)' % (base, (cu or {}).get('attrs', {}).get(AT_NAME)))

    prog, files = line_header(dline)
    ck(base in files, 'file_names 含 用户源 (实得 %r)' % (files,))
    ck(any(f.endswith('qcc_rt.c') for f in files), 'file_names 含 前插运行时 qcc_rt.c (实得 %r)' % (files,))

    subs = {}
    for d in dies:
        if d['tag'] == 0x2e:
            subs.setdefault(d['attrs'].get(AT_NAME), d)

    for nm in ('甲', '乙', '主'):
        d = subs.get(nm)
        if d is None:
            ck(False, '子程序 DIE %s 存在' % nm)
            continue
        ck(d['attrs'].get(AT_DECL_FILE) == 1,
           '%s 的 decl_file == 1 (实得 %r)' % (nm, d['attrs'].get(AT_DECL_FILE)))
        ck(d['attrs'].get(AT_DECL_LINE) == exp[nm],
           '%s 的 decl_line == 真源第 %d 行 (实得 %r)' % (nm, exp[nm], d['attrs'].get(AT_DECL_LINE)))

    rt_dies = [d for d in dies if d['tag'] == 0x2e and d['attrs'].get(AT_DECL_FILE) == 2]
    ck(len(rt_dies) >= 20, '运行时函数 归 文件 2 (实得 %d 个)' % len(rt_dies))
    # 2026-10-18 审计 D32: 前插运行时的 `#include` 展开会给**所有**运行时行号一个**恒定**偏移
    #   (实测 +10: stdio/stdlib/string/time 展开的行)。这里不写死 10，而是用已知函数
    #   (`void *realloc(`) 现场求偏移，并要求「偏移后全部落回 qcc_rt.c 原区间」+ 偏移恒定。
    _rt_anchor = rt_line_of('void *realloc(')
    _rt0 = next((d for d in rt_dies if d['attrs'].get(AT_NAME) == 'realloc'), None)
    _off = None
    if _rt_anchor > 0 and _rt0 is not None:
        _off = _rt0['attrs'].get(AT_DECL_LINE, 0) - _rt_anchor
    ck(_off is not None and _off >= 0,
       '运行时 展开行偏 可现场求出 (realloc 源 %s 行, 实得偏移 %r)' % (_rt_anchor, _off))
    if _off is not None:
        _bad = [d for d in rt_dies if not (1 <= d['attrs'].get(AT_DECL_LINE, 0) - _off <= n_rt)]
        ck(not _bad,
           '运行时 decl_line 减 行偏 %d 后 落 1..%d (越界 %d 条 %r)' % (
               _off, n_rt, len(_bad),
               [(d['attrs'].get(AT_NAME), d['attrs'].get(AT_DECL_LINE)) for d in _bad[:3]]))
        _thr = next((d for d in rt_dies if d['attrs'].get(AT_NAME) == 'throw_ex'), None)
        if _thr is not None:
            ck(_thr['attrs'].get(AT_DECL_LINE, 0) - _off == rt_line_of('int throw_ex('),
               'throw_ex decl_line - 行偏 == 真源第 %d 行 (实得 %r - %d)' % (
                   rt_line_of('int throw_ex('), _thr['attrs'].get(AT_DECL_LINE), _off))
    usr_dies = [d for d in dies if d['tag'] == 0x2e and d['attrs'].get(AT_DECL_FILE) == 1]
    ck(all(1 <= d['attrs'].get(AT_DECL_LINE, 0) <= n_user for d in usr_dies),
       '所有 用户文件 子程序 行号 落在 1..%d (锚文件 行数)' % n_user)

    rows = decode_lines(dline, cu['addr_size'])
    max_user_line = max([l for (_a, f, l) in rows if f == 1] or [0])
    ck(max_user_line <= n_user, '行表 file=1 的 最大行号 %d <= 用户源 行数 %d' % (max_user_line, n_user))
    ck(len(rows) >= 20, '行号程序 可解码 (行数 %d)' % len(rows))

    def ranges(ds):
        return [(d['attrs'].get(AT_LOW_PC, 0), d['attrs'].get(AT_LOW_PC, 0) + max(d['attrs'].get(AT_HIGH_PC, 0), 1)) for d in ds]

    # 不变量 (第 453 站收紧): 落在某函数区间内的行, 其文件 必须 == 该函数 的文件。
    allsub = [(d, ranges([d])[0]) for d in dies if d['tag'] == 0x2e]
    bad = []
    for (a, f, l) in rows:
        owner = [d for d, (lo, hi) in allsub if lo <= a < hi]
        if owner and owner[0]['attrs'].get(AT_DECL_FILE) != f:
            bad.append((a, f, l, owner[0]['attrs'].get(AT_NAME)))
    ck(not bad, '每个 行表行 的 文件 == 其地址 所在 函数 的 文件 (跨文件违例 %d 条 %r)' % (len(bad), bad[:3]))

    lo_min = min([d['attrs'].get(AT_LOW_PC, 0) for d in usr_dies] or [0])
    low1 = [r for r in rows if r[1] == 1 and r[0] < lo_min]
    ck(not low1, '无 file=1 行 落在 首个 用户函数 (0x%x) 之前 (越界 %d 条 %r)' % (lo_min, len(low1), low1[:3]))

    main_die = subs.get('主')
    if main_die:
        lo = main_die['attrs'].get(AT_LOW_PC, 0)
        sz = main_die['attrs'].get(AT_HIGH_PC, 0)
        ok_addr = any(f == 1 and l == exp_stmt and lo <= a < lo + max(sz, 1) for (a, f, l) in rows)
        ck(ok_addr, '行表 有 (file=1, line=%d 格式化) 且 地址 落在 主 的 [low_pc, +size)' % exp_stmt)

    if FAILS:
        print('[闸④ #289 DWARF] ❌ %d 条 未通过 (编译器 %s)' % (len(FAILS), qcc))
        return 1
    print('[闸④ #289 DWARF] ✅ CU/文件表/声明行/行表 全部 指向 真源 '
          '(用户 %d 行 · 运行时 %d 行 · 行表 %d 行 · 运行时 DIE %d 个)'
          % (n_user, n_rt, len(rows), len(rt_dies)))
    return 0


if __name__ == '__main__':
    sys.exit(main())
