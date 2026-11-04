# -*- coding: utf-8 -*-
"""check_elf_format —— ELF64 产物**格式自洽**判据（E 段 · 本机无 Linux/WSL 时的 gating）

断言（任一条不成立 ⟹ rc=1）:
  1. 魔数 \\x7fELF；EI_CLASS=2（64 位）；EI_DATA=1（小端）；EI_VERSION=1。
  2. e_type=ET_EXEC(2)；e_machine=EM_X86_64(62)；e_version=1；
     e_ehsize=64；e_phentsize=56；e_shentsize=64；e_phnum>=2；e_shnum>=1。
  3. 每个 PT_LOAD：p_filesz <= p_memsz；p_offset % p_align == p_vaddr % p_align；
     文件区间 [p_offset, p_offset+p_filesz) 在文件内。
  4. e_entry 落在某个 PT_LOAD 的 [p_vaddr, p_vaddr+p_memsz) 内。
  5. 节头：名字经 .shstrtab 解析；必须含 .text 与 .data；
     .text 可执行（SHF_ALLOC|SHF_EXECINSTR）、.data 可写（SHF_ALLOC|SHF_WRITE）；
     所有节的文件区间在文件内；e_shoff + e_shnum*64 <= filesize。
  6. `.text` 节大小 > 0，且其文件内容不全为 0（不是空壳）。

用法: python scripts/check_elf_format.py <elf文件>
退出码: 0 = 全部成立；1 = 失败
"""
import io
import struct
import sys

sys.stdout.reconfigure(encoding='utf-8', errors='replace')


def main(path):
    d = io.open(path, 'rb').read()
    fails = []

    def chk(ok, msg):
        if not ok:
            fails.append(msg)
        return ok

    chk(len(d) >= 64, '文件 < 64 字节')
    if fails:
        return report(path, fails)
    chk(d[:4] == b'\x7fELF', '魔数不是 \\x7fELF')
    chk(d[4] == 2, 'EI_CLASS != 2（不是 ELF64）')
    chk(d[5] == 1, 'EI_DATA != 1（不是小端）')
    chk(d[6] == 1, 'EI_VERSION != 1')

    (e_type, e_machine, e_version, e_entry, e_phoff, e_shoff, e_flags,
     e_ehsize, e_phentsize, e_phnum, e_shentsize, e_shnum, e_shstrndx) = \
        struct.unpack_from('<HHIQQQIHHHHHH', d, 16)

    chk(e_type == 2, 'e_type=%d != ET_EXEC(2)' % e_type)
    chk(e_machine == 62, 'e_machine=%d != EM_X86_64(62)' % e_machine)
    chk(e_version == 1, 'e_version != 1')
    chk(e_ehsize == 64, 'e_ehsize=%d != 64' % e_ehsize)
    chk(e_phentsize == 56, 'e_phentsize=%d != 56' % e_phentsize)
    chk(e_shentsize == 64, 'e_shentsize=%d != 64' % e_shentsize)
    chk(e_phnum >= 2, 'e_phnum=%d < 2' % e_phnum)
    chk(e_shnum >= 1, 'e_shnum=%d < 1' % e_shnum)
    chk(e_shstrndx < e_shnum, 'e_shstrndx 越界')
    chk(e_phoff + e_phnum * e_phentsize <= len(d), '程序头表越界')
    chk(e_shoff + e_shnum * e_shentsize <= len(d), '节头表越界')

    loads = []
    for i in range(e_phnum):
        q = e_phoff + i * e_phentsize
        p_type, p_flags, p_off, p_vaddr, p_paddr, p_filesz, p_memsz, p_align = \
            struct.unpack_from('<IIQQQQQQ', d, q)
        if p_type == 1:
            loads.append((p_flags, p_off, p_vaddr, p_filesz, p_memsz, p_align))
            chk(p_filesz <= p_memsz, 'PT_LOAD[%d] filesz>memsz' % i)
            if p_align:
                chk(p_off % p_align == p_vaddr % p_align,
                    'PT_LOAD[%d] offset/vaddr 不同余 (align=%#x)' % (i, p_align))
            chk(p_off + p_filesz <= len(d), 'PT_LOAD[%d] 文件区间越界' % i)
    chk(len(loads) >= 2, 'PT_LOAD 少于 2 个')
    chk(any(v <= e_entry < v + m for (_, _, v, _, m, _) in loads),
        'e_entry=%#x 不在任何 PT_LOAD 内' % e_entry)

    # 节头
    secs = []
    for i in range(e_shnum):
        q = e_shoff + i * e_shentsize
        sh_name, sh_type, sh_flags, sh_addr, sh_off, sh_size, sh_link, sh_info, \
            sh_align, sh_entsize = struct.unpack_from('<IIQQQQIIQQ', d, q)
        secs.append((sh_name, sh_type, sh_flags, sh_addr, sh_off, sh_size))
        chk(sh_off + sh_size <= len(d), '节[%d] 文件区间越界' % i)

    strtab = secs[e_shstrndx]
    st_off, st_size = strtab[4], strtab[5]

    def name_of(off):
        b = d[st_off + off: st_off + st_size]
        z = b.find(b'\x00')
        return (b[:z] if z >= 0 else b).decode('latin1')

    names = {name_of(s[0]): s for s in secs}
    chk('.text' in names, '缺 .text 节')
    chk('.data' in names, '缺 .data 节')
    if '.text' in names:
        t = names['.text']
        chk(t[2] & 0x4 != 0, '.text 缺 SHF_EXECINSTR')
        chk(t[2] & 0x2 != 0, '.text 缺 SHF_ALLOC')
        chk(t[5] > 0, '.text 大小 0')
        body = d[t[4]:t[4] + t[5]]
        chk(any(b != 0 for b in body), '.text 内容全 0（空壳）')
    if '.data' in names:
        t = names['.data']
        chk(t[2] & 0x1 != 0, '.data 缺 SHF_WRITE')
        chk(t[2] & 0x2 != 0, '.data 缺 SHF_ALLOC')

    print('ELF64: type=%d machine=%d entry=%#x phnum=%d shnum=%d size=%d'
          % (e_type, e_machine, e_entry, e_phnum, e_shnum, len(d)))
    for (fl, off, va, fs, ms, al) in loads:
        print('  PT_LOAD flags=%d off=%#x vaddr=%#x filesz=%d memsz=%d align=%#x'
              % (fl, off, va, fs, ms, al))
    for nm in sorted(names):
        s = names[nm]
        print('  [%s] addr=%#x off=%#x size=%d flags=%#x' % (nm, s[3], s[4], s[5], s[2]))
    return report(path, fails)


def report(path, fails):
    if fails:
        print('== ELF 格式: FAIL (%d) ==' % len(fails))
        for f in fails:
            print('  - ' + f)
        return 1
    print('== ELF 格式: PASS（头/程序头/节头/段对齐 全部自洽）==')
    return 0


if __name__ == '__main__':
    if len(sys.argv) < 2:
        print('用法: python scripts/check_elf_format.py <elf文件>')
        sys.exit(2)
    sys.exit(main(sys.argv[1]))
