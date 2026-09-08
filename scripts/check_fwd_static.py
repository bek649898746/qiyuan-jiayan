# -*- coding: utf-8 -*-
"""前向静态检查 (2026-09-09 防线役 R5b):
自举静态丢值根治 (18c3aca, 前向引用双条目→写读地址分裂) 后, 防新雷回归门。
扫描 qcc_zh.jy: 顶层静变量声明, 其名在声明行前被 值引用 (非调用/非声明/非注释) → 候选。
打包待 类雷 = 跨域双向引用 (声明前后都有引用且读写依赖); 纯前向 (域隔离) 无害, 列已知清单。
用法: python scripts/check_fwd_static.py   (exit 0=无新雷, 1=有新候选)
"""
import io, re, sys

SRC = r"C:\Users\Administrator\Desktop\qiyuan-jiayan\srclib_jiayan\qcc_zh.jy"

# 已知无害前向 (计数器/标签生成族, 域隔离 — 2026-09-09 审计; 若它们真出雷再移除)
KNOWN_HARMLESS = set()

def scan(src_text):
    src = re.sub(r"/\*.*?\*/", "", src_text, flags=re.S)
    lines = [re.sub(r'"([^"\\]|\\.)*"', "", re.sub(r"//.*$", "", l)) for l in src.split("\n")]

    decl = []  # (行, 名)
    pat = re.compile(r'^\s*静\s+(?P<ty>[A-Za-z_\u4e00-\u9fff][\w\u4e00-\u9fff]*)\s*\*?\s*(?P<name>[A-Za-z_\u4e00-\u9fff][\w\u4e00-\u9fff]*)\s*(?P<rest>=|;|\[|,)')
    for i, l in enumerate(lines):
        m = pat.match(l)
        if m and m.group("ty") != "空":
            decl.append((i + 1, m.group("name")))

    # 声明前值引用: 对每个声明, 向前找首个非调用引用
    name_first_decl = {}
    for di, name in decl:
        name_first_decl.setdefault(name, di)

    hits = []
    seen = set()
    for di, name in decl:
        if name in seen:
            continue
        seen.add(name)
        for j in range(0, di - 1):
            for m in re.finditer(r'(?<![\w\u4e00-\u9fff])' + re.escape(name) + r'(?![\w\u4e00-\u9fff])', lines[j]):
                if not lines[j][m.end():].lstrip().startswith("("):
                    hits.append((j + 1, di, name))
                    break
            if hits and hits[-1][2] == name:
                break

    # 新雷 = 不在已知清单的候选
    new_hits = [h for h in hits if h[2] not in KNOWN_HARMLESS]
    return new_hits, hits

def main():
    src = io.open(SRC, encoding="utf-8-sig").read()
    new_hits, all_hits = scan(src)
    if new_hits:
        print(f"⚠ 前向引用候选 {len(new_hits)} (可能 前向引用双条目雷, 见 20260908_static_fwd_decl_rootcause.md):")
        for ref, dl, name in sorted(new_hits):
            print(f"  引用 L{ref} < 声明 L{dl} : {name}  ← 声明前值引用, 若声明后也有跨域读写 = 雷, 需前移声明")
        return 1
    print(f"✓ 无新前向引用候选 (总候选 {len(all_hits)} 个均在已知无害域隔离清单或已修)")
    return 0

if __name__ == "__main__":
    sys.exit(main())
