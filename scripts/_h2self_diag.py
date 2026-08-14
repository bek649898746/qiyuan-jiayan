# -*- coding: utf-8 -*-
"""自宿主 H2 诊断: 对比宿主 qcc -S 文本 vs v4 -S 文本, 找 v4 缺/多/异常指令行."""
import subprocess, os, sys, difflib, re, io
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')
os.chdir(r'C:\Users\Administrator\Desktop\qiyuan-jiayan')
src = sys.argv[1]
base = 'scratch_test/_td'
for p in ('_td.asm', '_td.asm.asm', '_tdc.asm', '_tdc.asm.asm'):
    try: os.remove(base + p[1:])
    except OSError: pass
subprocess.run(['qcc_x86.exe', '-S', src, '-o', base + '.asm'], capture_output=True)
subprocess.run(['build/conv/v4.exe', '-S', src, '-o', base + 'c.asm'], capture_output=True)
h = open(base + '.asm.asm', 'rb').read().decode('utf-8', errors='replace').splitlines()
v = open(base + 'c.asm.asm', 'rb').read().decode('utf-8', errors='replace').splitlines()

def norm(s):
    s = re.sub(r'\[rsp\+(\d+)\]', lambda m: '[rsp%s]' % m.group(1), s)
    s = re.sub(r'\.L\d+', '.L#', s)
    return s.strip()

def is_inst(l):
    s = l.strip()
    return bool(s) and not s.startswith('.') and not s.startswith(';') and not s.endswith(':')

hh = [norm(x) for x in h]
vv = [norm(x) for x in v]
sm = difflib.SequenceMatcher(None, hh, vv)
print('=== v4 缺的指令行 ===')
cnt = 0
for tag, i1, i2, j1, j2 in sm.get_opcodes():
    if tag == 'delete':
        for k in range(i1, i2):
            if is_inst(h[k]):
                print('  %d: %s' % (k, h[k][:100])); cnt += 1
                if cnt > 15: break
    if cnt > 15: break
print('=== v4 多的指令行 ===')
cnt = 0
for tag, i1, i2, j1, j2 in sm.get_opcodes():
    if tag == 'insert':
        for k in range(j1, j2):
            if is_inst(v[k]):
                print('  %d: %s' % (k, v[k][:100])); cnt += 1
                if cnt > 15: break
    if cnt > 15: break
print('=== 非格式差异 (归一化后仍不同) ===')
cnt = 0
for tag, i1, i2, j1, j2 in sm.get_opcodes():
    if tag == 'replace':
        for k in range(max(i2 - i1, j2 - j1)):
            hl = h[i1 + k] if i1 + k < i2 else None
            vl = v[j1 + k] if j1 + k < j2 else None
            if hl is None or vl is None: continue
            if is_inst(hl) or is_inst(vl):
                print('  H: %s' % hl[:100])
                print('  V: %s' % vl[:100])
                cnt += 1
                if cnt > 12: break
    if cnt > 12: break
