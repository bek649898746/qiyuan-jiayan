# -*- coding: utf-8 -*-
# tests/unit/test_asm_zh.py — asm_zh 指令编码单元测试
# 用法: python tests/unit/test_asm_zh.py  （需先编译 asm_zh.exe，见 build.ps1 / README_DEV）
# 覆盖: 通用 x86-64 指令 + LL 64 位指令的中文助记符 → 机器码逐字节断言
import subprocess, io, os, sys, struct, tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
ASMZH = os.path.join(ROOT, 'asm_zh.exe')
if not os.path.exists(ASMZH):
    # 尝试就地编译
    r = subprocess.run(['gcc', '-O2', '-Wall', '-Werror', os.path.join(ROOT, 'srclib', 'asm_zh.c'), '-o', ASMZH], capture_output=True)
    if r.returncode != 0:
        print('FATAL: asm_zh.exe 缺失且编译失败', r.stderr.decode('utf-8', 'replace')[:300])
        sys.exit(1)

# (助记符行, 期望 .text 字节 hex)
CASES = [
    # --- 已有基础指令 ---
    ('压栈 r0', '50'),
    ('弹栈 r0', '58'),
    ('返回', 'c3'),
    ('扩字', '4898'),
    ('扩八字', '4899'),
    ('移动 r0, 5', 'b805000000'),
    ('移64 r0, r3', '4889d8'),
    ('加 r0, r3', '40 01 d8'.replace(' ', '')),
    ('减 r0, r3', '4029d8'),
    ('比较 r0, r3', '4039d8'),
    ('取反 r0', 'f7d8'),  # r<8 无 REX 前缀
    ('按位反 r0', 'f7d0'),  # r<8 无 REX 前缀
    # --- LL 64 位指令 ---
    ('加64 r0, r3', '4801d8'),
    ('减64 r0, r3', '4829d8'),
    ('乘64 r0, r3', '480fafc3'),
    ('比较64 r0, r3', '4839d8'),
    ('除64 r3', '489948f7fb'),
    ('除余64 r3', '489948f7fb'),
    ('移动64 r0, 1410065408, 2', '48b800e40b5402000000'),
    ('左移64 r0, cl', '48d3e0'),
    ('算术右移64 r0, cl', '48d3f8'),
    ('逻辑右移64 r0, cl', '48d3e8'),
    ('与64 r0, r3', '4821d8'),
    ('或64 r0, r3', '4809d8'),
    ('异或64 r0, r3', '4831d8'),
    # --- P0 通用指令 ---
    ('加带进 r0, r3', '4011d8'),
    ('加带进64 r0, r3', '4811d8'),
    ('减带借 r0, r3', '4019d8'),
    ('减带借64 r0, r3', '4819d8'),
    ('加带进即 r0, 5', '4083d005'),
    ('加带进即64 r0, 5', '4883d005'),
    ('减带借即 r0, 300', '4081d82c010000'),
    ('减带借即64 r0, 300', '4881d82c010000'),
    ('自增64 r0', '48ffc0'),
    ('自减64 r3', '48ffcb'),
    ('取反64 r0', '48f7d8'),
    ('乘无64 r3', '48f7e3'),
    ('乘即 r0, r3, 5', '486bc305'),
    ('乘即 r0, r3, 300', '4869c32c010000'),
    ('双左移 r0, r3, 4', '400fa4d804'),
    ('双左移64 r0, r3, 4', '480fa4d804'),
    ('双右移 r0, r3, 4', '400facd804'),
    ('双右移64 r0, r3, 4', '480facd804'),
    ('循环左移 r0, 4', '40c1c004'),
    ('循环左移cl r0', '40d3c0'),
    ('循环右移 r0, 4', '40c1c804'),
    ('循环右移cl r0', '40d3c8'),
    ('带进左移 r0, 4', '40c1d004'),
    ('带进左移cl r0', '40d3d0'),
    ('带进右移 r0, 4', '40c1d804'),
    ('带进右移cl r0', '40d3d8'),
    ('循环左移64 r0, 4', '48c1c004'),
    ('循环左移64cl r0', '48d3c0'),  # rol r64,cl = D3 /0
    ('循环右移64 r0, 4', '48c1c804'),
    ('循环右移64cl r0', '48d3c8'),
    ('带进左移64 r0, 4', '48c1d004'),
    ('带进左移64cl r0', '48d3d0'),
    ('带进右移64 r0, 4', '48c1d804'),
    ('带进右移64cl r0', '48d3d8'),
    # --- 位操作 ---
    ('测试位 r0, 3', '480fba e003'.replace(' ', '')),
    ('测试位64 r0, r3', '480fa3d8'),
    ('位测置 r0, r3', '400fabd8'),
    ('位测置64 r0, r3', '480fabd8'),
    ('位测清 r0, r3', '400fb3d8'),
    ('位测清64 r0, r3', '480fb3d8'),
    ('位测翻 r0, r3', '400fbbd8'),
    ('位测翻64 r0, r3', '480fbbd8'),
    ('扫零位 r0, r3', '400fbcc3'),
    ('扫零位64 r0, r3', '480fbcc3'),
    ('扫置位 r0, r3', '400fbdc3'),
    ('扫置位64 r0, r3', '480fbdc3'),
    # --- 传输 ---
    ('取符号字节 r0, r3', '400fbec3'),
    ('取符号字 r0, r3', '400fbfc3'),
    ('交换 r0, r3', '4087c3'),  # xchg r/m=r3, reg=r0
    ('交换64 r0, r3', '4887c3'),
    # --- cmovcc 补充 ---
    ('条移负 r0, r3', '400f48c3'),
    ('条移非负 r0, r3', '400f49c3'),
    ('条移溢 r0, r3', '400f40c3'),
    ('条移不溢 r0, r3', '400f41c3'),
    ('条移奇 r0, r3', '400f4ac3'),
    ('条移偶 r0, r3', '400f4bc3'),
    # --- 间接跳转 / nop ---
    ('间跳 r0', '48ffe0'),
    ('无操作', '90'),
    ('无操作3', '0f1f00'),
    ('无操作4', '0f1f4000'),
    # --- 标志 / 系统 ---
    ('清进位', 'f8'),
    ('置进位', 'f9'),
    ('翻进位', 'f5'),
    ('清方向', 'fc'),
    ('置方向', 'fd'),
    ('取标志', '9f'),
    ('存标志', '9e'),
    ('压标志', '489c'),
    ('弹标志', '489d'),
    ('中断 3', 'cd03'),
    ('中断返', '48cf'),
    ('系统调用', '0f05'),
    ('取Cpu', '0fa2'),
    ('取时钟', '0f31'),
    # --- 字符串 ---
    ('串拷', 'f3a4'),
    ('串拷双', 'f3a5'),
    ('串比', 'a6'),
    ('串比双', 'a7'),
    ('串扫', 'ae'),
    ('串扫双', 'af'),
    ('串载', 'ac'),
    ('串载双', 'ad'),
    ('串存', 'aa'),
    ('串存双', 'ab'),
]

def asm_text_hex(src_lines):
    asm = '_入口:\n' + '\n'.join(src_lines) + '\n返回\n'
    fd, path = tempfile.mkstemp(suffix='.asm')
    os.close(fd)
    io.open(path, 'w', encoding='utf-8', newline='').write(asm)
    out = path + '.exe'
    r = subprocess.run([ASMZH, path, '-o', out], capture_output=True, text=True, encoding='utf-8', errors='replace')
    if r.returncode != 0:
        return None, (r.stderr or '')
    a = open(out, 'rb').read()
    for p in (path, out):
        try:
            os.unlink(p)
        except OSError:
            pass
    pe = struct.unpack_from('<I', a, 0x3C)[0]
    ns = struct.unpack_from('<H', a, pe + 4 + 2)[0]
    osz = struct.unpack_from('<H', a, pe + 4 + 16)[0]
    sec = pe + 4 + 20 + osz
    for i in range(ns):
        name = a[sec + i * 40: sec + i * 40 + 8].rstrip(b'\x00')
        rs, ra = struct.unpack_from('<II', a, sec + i * 40 + 16)
        if name == b'.text':
            return a[ra:ra + rs].hex(), ''
    return None, 'no .text'

def main():
    # 单指令模式：每条独立汇编对比
    passed = failed = 0
    fails = []
    for line, exp in CASES:
        got, err = asm_text_hex([line])
        if got is None:
            failed += 1
            fails.append('%-24s ERROR: %s' % (line, err.strip()[:60]))
        elif got.startswith(exp):
            passed += 1
        else:
            failed += 1
            fails.append('%-24s exp=%s got=%s' % (line, exp, got[:len(exp) + 2]))
    # 组合模式：批量指令一次汇编（覆盖 rel8/跳转回填）
    combo = [
        'L1:', '跳转短 L2', '环跳 L1', '负跳 L2', '大跳 L2',
        'L2:', '偶跳 L1',
    ]
    got, err = asm_text_hex(combo)
    if got is None:
        failed += 1
        fails.append('combo ERROR: %s' % err.strip()[:80])
    else:
        passed += 1
    print('asm_zh 指令测试: 通过 %d, 失败 %d' % (passed, failed))
    for f in fails[:20]:
        print('  [FAIL]', f)
    if fails:
        print('共 %d 条失败' % len(fails))
    sys.exit(1 if failed else 0)

if __name__ == '__main__':
    main()
