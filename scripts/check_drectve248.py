# -*- coding: utf-8 -*-
"""check_drectve248 —— **族12 #248 导出表控制 收口 检查器**（闸⑥）。

背景（R597 · 2026-10-09 · 批C-AU）：
  R593 已 把 `导出` 前缀 做成 **语言面**（顶层 登记 导出名 + `导出名数()` 可 查询），但
  **导出名 从未 进 目标文件** ⟹ 族12 #248 一直 挂账（`_DEBT_INDEX` 的 下一刀 就是 本 段）。
  R597 补 **COFF `.drectve` 段**：把 每个 导出名 发成 链接器 指令 文本 `/EXPORT:名\\0`
  （与 MSVC 同款 通道）⟹ 链接器 读 该 段 即 为 名 生成 PE 导出目录 (.edata) ⟹
  `DataDirectory[0]` 不再 是 none。 本 检查器 逐字节 读 `.obj` 验证 **编译器 侧 职责 终点**。

判据（两条, 一 正 一 负）：
  ① **正**: 源 含 `导出 函数 整 甲(…)` ⟹ `.obj` 有 名为 `.drectve` 的 节, 其 原始数据
     **含 ASCII 串 `/EXPORT:甲`**（且 NumberOfSections == 5）。
  ② **负（零回归）**: 源 **无** `导出` ⟹ `.obj` **无** `.drectve` 节 且 NumberOfSections == 4
     —— 钉死「不 用 导出 的 程序 产物 逐字节 与 修前 同构」。

用法: python scripts/check_drectve248.py [编译器]      (默认 qcc_zh_v1.exe)
退出码: 0 = 全过; 1 = 有 判据 不 成立
"""
import io, os, struct, subprocess, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CC = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, 'qcc_zh_v1.exe')
OUT = os.path.join(ROOT, 'scratch_c1b2' if os.path.isdir(os.path.join(ROOT, 'scratch_c1b2')) else '.', 'drectve248')
os.makedirs(OUT, exist_ok=True)

H = '# 包含 <stdio.h>\n'
POS = H + u'导出 函数 整 甲(整 x) { 返 x + 2; }\n函数 整 主(空) { 格式化 "%d\\n", 甲(5); 返 0; }\n'
NEG = H + u'函数 整 乙(整 x) { 返 x + 2; }\n函数 整 主(空) { 格式化 "%d\\n", 乙(5); 返 0; }\n'


def sections(obj):
    """返 [(名, 原始尺寸, 原始偏移, 原始数据), …] 与 NumberOfSections。"""
    b = io.open(obj, 'rb').read()
    n = struct.unpack_from('<H', b, 2)[0]
    out = []
    for i in range(n):
        off = 20 + 40 * i
        name = b[off:off + 8].rstrip(b'\x00').decode('ascii', 'replace')
        size = struct.unpack_from('<I', b, off + 16)[0]
        ptr = struct.unpack_from('<I', b, off + 20)[0]
        out.append((name, size, ptr, b[ptr:ptr + size]))
    return n, out, b


def build(tag, src):
    p = os.path.join(OUT, tag + '.c')
    io.open(p, 'w', encoding='utf-8-sig', newline='\r\n').write(src)
    obj = os.path.join(OUT, tag + '.obj')
    if os.path.exists(obj):
        os.remove(obj)
    r = subprocess.run([CC, p, '-c', '-o', obj], capture_output=True, timeout=300)
    msg = (r.stdout + r.stderr).decode('utf-8', 'replace').strip().replace('\n', ' ')
    return obj, r.returncode, msg


bad = []

# ① 正: 有 导出 ⟹ 有 .drectve 且 含 /EXPORT:甲
obj, rc, msg = build('pos', POS)
if rc != 0 or not os.path.exists(obj):
    print('[正] ❌ `-c` 编译失败 rc=%d %s' % (rc, msg[:160])); bad.append('pos-compile')
else:
    n, secs, raw = sections(obj)
    hit = [(nm, sz, dt) for nm, sz, ptr, dt in secs if nm == '.drectve']
    print('[正] 节数=%d 节名=%s' % (n, ' '.join(s[0] for s in secs)))
    if not hit:
        print('[正] ❌ 目标文件 无 `.drectve` 节'); bad.append('pos-nosec')
    else:
        data = hit[0][2]
        want = u'/EXPORT:甲'.encode('utf-8')
        print('[正] `.drectve` 尺寸=%d 内容=%r' % (hit[0][1], data[:120]))
        if want not in data:
            print('[正] ❌ `.drectve` 内 未 找到 %r' % want); bad.append('pos-noexport')
        if n != 5:
            print('[正] ❌ NumberOfSections=%d (期望 5)' % n); bad.append('pos-nsec')

# ② 负/零回归: 无 导出 ⟹ 无 .drectve, 节数 4
obj, rc, msg = build('neg', NEG)
if rc != 0 or not os.path.exists(obj):
    print('[负] ❌ `-c` 编译失败 rc=%d %s' % (rc, msg[:160])); bad.append('neg-compile')
else:
    n, secs, raw = sections(obj)
    names = [s[0] for s in secs]
    print('[负] 节数=%d 节名=%s' % (n, ' '.join(names)))
    if '.drectve' in names:
        print('[负] ❌ 无 导出的 程序 也 生成 了 `.drectve` (零回归 破坏)'); bad.append('neg-hassec')
    if n != 4:
        print('[负] ❌ NumberOfSections=%d (期望 4)' % n); bad.append('neg-nsec')

print()
print('[check_drectve248] 判据 2 组 (正 导出 / 负 零回归) · 失败 %d' % len(bad))
if bad:
    print('  ❌ %s' % ' '.join(bad)); sys.exit(1)
print('  → 全过')
sys.exit(0)
