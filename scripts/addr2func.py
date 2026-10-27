#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""addr2func.py —— 「地址 → 函数」量具（D54 的操作面：崩溃只有裸地址时能立刻定位）

用法:
  python scripts/addr2func.py --exe a.exe --addr 0x401248 [--addr ...] [--json]

原理（不新增任何编译器改动，复用 -g 已产出的 DWARF 4）:
  PE 头 → 节表（含 COFF 长名 /N → 字符串表）
      → .debug_abbrev / .debug_info / .debug_line
      → 遍历 compile_unit 的 DW_TAG_subprogram（名 / low_pc / high_pc / decl_line / decl_file）
      → 地址落在 [low_pc, low_pc+high_pc) ⟹ 输出 函数名 + 段内偏移 + 源行

地址口径（三种都接受，逐个尝试后报命中）:
  绝对虚拟地址 0x401248 ｜ RVA 0x1248 ｜ .text 节内偏移 0x248
"""
import argparse
import io
import json
import struct
import sys

# ---------------- DWARF 常量 ----------------
DW_TAG_compile_unit = 0x11
DW_TAG_subprogram = 0x2E

TAG_NAMES = {DW_TAG_compile_unit: "compile_unit", DW_TAG_subprogram: "subprogram"}
AT_NAMES = {0x03: "name", 0x11: "low_pc", 0x12: "high_pc", 0x3A: "decl_file",
            0x3B: "decl_line", 0x1B: "comp_dir", 0x25: "producer", 0x13: "language",
            0x10: "stmt_list"}
# form → 编码方式
F_ADDR = 0x01
F_DATA2 = 0x05
F_DATA4 = 0x06
F_DATA8 = 0x07
F_STRING = 0x08
F_UDATA = 0x0F
F_REF4 = 0x13


class Reader:
    def __init__(self, buf, pos=0):
        self.b = buf
        self.p = pos

    def u8(self):
        v = self.b[self.p]
        self.p += 1
        return v

    def u16(self):
        v = struct.unpack_from("<H", self.b, self.p)[0]
        self.p += 2
        return v

    def u32(self):
        v = struct.unpack_from("<I", self.b, self.p)[0]
        self.p += 4
        return v

    def u64(self):
        v = struct.unpack_from("<Q", self.b, self.p)[0]
        self.p += 8
        return v

    def uleb(self):
        r = 0
        s = 0
        while True:
            b = self.u8()
            r |= (b & 0x7F) << s
            if not (b & 0x80):
                return r
            s += 7

    def cstr(self):
        e = self.b.index(b"\0", self.p)
        s = self.b[self.p:e]
        self.p = e + 1
        return s.decode("utf-8", "replace")


def read_form(r, form):
    if form == F_ADDR:
        return r.u64()
    if form == F_DATA2:
        return r.u16()
    if form == F_DATA4:
        return r.u32()
    if form == F_DATA8:
        return r.u64()
    if form == F_STRING:
        return r.cstr()
    if form == F_UDATA:
        return r.uleb()
    if form == F_REF4:
        return r.u32()
    raise ValueError("unsupported DWARF form %#x" % form)


def parse_abbrev(buf, offset):
    """返回 {code: (tag, has_children, [(at, form), ...])}"""
    r = Reader(buf, offset)
    table = {}
    while r.p < len(buf):
        code = r.uleb()
        if code == 0:
            break
        tag = r.uleb()
        children = r.u8()
        specs = []
        while True:
            at = r.uleb()
            form = r.uleb()
            if at == 0 and form == 0:
                break
            specs.append((at, form))
        table[code] = (tag, children, specs)
    return table


def parse_info(abbrev_buf, info_buf):
    """返回 (subprogram 列表, compile_unit 列表)"""
    subs = []
    cus = []
    r = Reader(info_buf, 0)
    while r.p + 11 <= len(info_buf):
        unit_start = r.p
        unit_len = r.u32()
        if unit_len == 0 or unit_start + 4 + unit_len > len(info_buf):
            break
        unit_end = unit_start + 4 + unit_len
        version = r.u16()
        if version != 4:
            # 只认 v4（当前编译器产物）；其它版本按长度跳过，避免误读
            r.p = unit_end
            continue
        abbrev_off = r.u32()
        addr_size = r.u8()
        if addr_size != 8:
            r.p = unit_end
            continue
        abbrevs = parse_abbrev(abbrev_buf, abbrev_off)
        cu = {"unit_offset": unit_start, "version": version}
        while r.p < unit_end:
            code = r.uleb()
            if code == 0:
                continue
            if code not in abbrevs:
                raise ValueError("abbrev code %d 未定义 (info off %#x)" % (code, r.p))
            tag, children, specs = abbrevs[code]
            attrs = {}
            for at, form in specs:
                attrs[at] = read_form(r, form)
            if tag == DW_TAG_compile_unit:
                cu["name"] = attrs.get(0x03, "")
                cu["comp_dir"] = attrs.get(0x1B, "")
                cu["producer"] = attrs.get(0x25, "")
                cu["low_pc"] = attrs.get(0x11, 0)
                cus.append(cu)
            elif tag == DW_TAG_subprogram:
                low = attrs.get(0x11, 0)
                high = attrs.get(0x12, 0)
                subs.append({
                    "name": attrs.get(0x03, "?"),
                    "low": low,
                    "high": high,          # DWARF4 起 high_pc 是长度
                    "decl_line": attrs.get(0x3B, 0),
                    "decl_file": attrs.get(0x3A, 0),
                    "cu": cu.get("name", ""),
                })
        r.p = unit_end
    subs.sort(key=lambda s: s["low"])
    return subs, cus


def parse_pe(buf):
    pe = struct.unpack_from("<I", buf, 0x3C)[0]
    if buf[pe:pe + 4] != b"PE\0\0":
        raise ValueError("不是 PE 文件")
    nsec = struct.unpack_from("<H", buf, pe + 6)[0]
    opt_size = struct.unpack_from("<H", buf, pe + 20)[0]
    opt = pe + 24
    magic = struct.unpack_from("<H", buf, opt)[0]
    if magic == 0x20B:
        image_base = struct.unpack_from("<Q", buf, opt + 24)[0]
        base_of_code = struct.unpack_from("<I", buf, opt + 20)[0]
    else:
        image_base = struct.unpack_from("<I", buf, opt + 28)[0]
        base_of_code = struct.unpack_from("<I", buf, opt + 20)[0]
    sec_base = opt + opt_size
    secs = []
    raw_secs = []
    for i in range(nsec):
        off = sec_base + 40 * i
        raw_name = buf[off:off + 8]
        vsize, va, raw_size, raw_ptr = struct.unpack_from("<IIII", buf, off + 8)
        secs.append({"name": raw_name, "vsize": vsize, "va": va,
                     "raw_size": raw_size, "raw_ptr": raw_ptr})
        raw_secs.append((off, vsize, va, raw_size, raw_ptr))
    # COFF 符号表（长名 /N 需要）
    sym_off, nsym = struct.unpack_from("<II", buf, pe + 12)
    strtab = b""
    if sym_off and nsym:
        strtab = buf[sym_off + 18 * nsym:]
    for s in secs:
        nm = s["name"].rstrip(b"\0")
        if nm[:1] == b"/":
            idx = int(nm[1:])
            sec_str = strtab[idx:]
            end = sec_str.find(b"\0")
            s["name"] = sec_str[:end].decode("latin1") if end >= 0 else ""
        else:
            s["name"] = nm.decode("latin1")
    return image_base, base_of_code, secs


def section_data(buf, sec):
    if sec["raw_size"] == 0:
        return b""
    return buf[sec["raw_ptr"]:sec["raw_ptr"] + sec["raw_size"]]


def fmt_addr(v):
    return "0x%X" % v


def lookup(subs, addr):
    lo, hi = 0, len(subs) - 1
    best = None
    while lo <= hi:
        mid = (lo + hi) // 2
        s = subs[mid]
        if addr < s["low"]:
            hi = mid - 1
        elif addr >= s["low"] + s["high"]:
            lo = mid + 1
        else:
            best = s
            break
    if best is None:
        # 允许命中「末尾之后 0 字节」的边界（high_pc 缺失）
        for s in subs:
            if s["low"] <= addr < s["low"] + max(s["high"], 1):
                best = s
                break
    return best


def main():
    ap = argparse.ArgumentParser(description="地址 → 函数（PE + DWARF 4）")
    ap.add_argument("--exe", required=True, help="用 qcc -g 产出的 PE")
    ap.add_argument("--addr", action="append", default=[], help="地址，可多次；支持 0x401248 / 0x1248 / 0x248")
    ap.add_argument("--as", dest="kind", default="va", choices=["va", "rva", "off"],
                    help="地址口径: va=绝对虚拟地址(默认) rva=节虚拟地址+偏 rva off=.text 节内偏移；"
                         "不指定时**不做**任何静默改读（未命中就报未命中）")
    ap.add_argument("--list", action="store_true", help="列出全部函数区间（按地址升序）")
    ap.add_argument("--json", action="store_true", help="机器可读输出")
    args = ap.parse_args()

    buf = io.open(args.exe, "rb").read()
    image_base, base_of_code, secs = parse_pe(buf)
    by_name = {s["name"]: s for s in secs}
    need = [".debug_abbrev", ".debug_info"]
    missing = [n for n in need if n not in by_name]
    if missing:
        sys.stderr.write(
            "[ERR] %s 缺调试节 %s —— 该产物不是 -g 编出的（qcc -g -o x.exe x.c）\n"
            % (args.exe, ", ".join(missing)))
        return 2
    abbrev = section_data(buf, by_name[".debug_abbrev"])
    info = section_data(buf, by_name[".debug_info"])
    subs, cus = parse_info(abbrev, info)

    text = by_name.get(".text")
    text_va = text["va"] if text else base_of_code
    text_end = text_va + (text["vsize"] if text else 0)

    if args.list:
        out = []
        for s in subs:
            out.append({"name": s["name"], "low": s["low"], "len": s["high"],
                        "line": s["decl_line"], "file": s["decl_file"]})
        if args.json:
            print(json.dumps({"exe": args.exe, "image_base": image_base,
                              "cu": cus, "functions": out},
                             ensure_ascii=False, indent=1))
        else:
            print("ImageBase=%s  .text VA=%s..%s  CU=%s  函数=%d"
                  % (fmt_addr(image_base), fmt_addr(text_va), fmt_addr(text_end),
                     cus[0]["name"] if cus else "?", len(subs)))
            for s in subs:
                print("  %-32s %s +%-7d (len %-6d) line %d file %d"
                      % (s["name"], fmt_addr(s["low"]), s["high"], s["high"],
                         s["decl_line"], s["decl_file"]))
        return 0

    if not args.addr:
        sys.stderr.write("[ERR] 需要 --addr 或 --list\n")
        return 2

    results = []
    worst = 0
    for a in args.addr:
        v = int(a, 16) if a.lower().startswith("0x") else int(a, 0)
        if args.kind == "va":
            kind, va = "VA", v
        elif args.kind == "rva":
            kind, va = "RVA", image_base + v
        else:
            kind, va = ".text偏移", image_base + text_va + v
        s = lookup(subs, va)
        if s is None:
            # 只给提示，不冒充答案：口径选错时明确说出来
            alt = []
            if args.kind != "va":
                alt.append(("VA", v))
            if args.kind != "rva":
                alt.append(("RVA", image_base + v))
            if args.kind != "off":
                alt.append((".text偏移", image_base + text_va + v))
            hints = []
            for ak, av in alt:
                hs = lookup(subs, av)
                if hs:
                    hints.append("按 %s 口径 (%s) 会落在 %s + %d"
                                 % (ak, fmt_addr(av), hs["name"], av - hs["low"]))
            hint = ("；提示：" + "；".join(hints)) if hints else ""
            results.append({"query": a, "found": False, "as": kind, "hint": hint})
            worst = 1
            continue
        rec = {"query": a, "found": True, "as": kind, "va": fmt_addr(va),
               "func": s["name"], "func_low": fmt_addr(s["low"]),
               "offset": va - s["low"], "len": s["high"],
               "decl_line": s["decl_line"], "decl_file": s["decl_file"],
               "cu": s["cu"]}
        results.append(rec)
    if args.json:
        print(json.dumps({"exe": args.exe, "image_base": image_base,
                          "results": results}, ensure_ascii=False, indent=1))
    else:
        for rec in results:
            if not rec["found"]:
                print("%s -> 未命中任何函数（%s 口径；不在 subprogram 区间内）%s"
                      % (rec["query"], rec["as"], rec.get("hint", "")))
                continue
            print("%s -> %s + %d  [%s 口径, %s, 函数区间 %s +%d, decl %d:%d]"
                  % (rec["query"], rec["func"], rec["offset"], rec["as"], rec["va"],
                     rec["func_low"], rec["len"], rec["decl_file"], rec["decl_line"]))
    return 0 if worst == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
