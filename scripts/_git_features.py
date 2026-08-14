# -*- coding: utf-8 -*-
"""Git 源码语法特征预分析: 统计 Phase 1 缺失/已支持特性的使用频率."""
import os, sys, re, io
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')
ROOT = r'C:\Users\Administrator\Desktop\git-2.45.2'

files = []
for dirpath, dirs, fs in os.walk(ROOT):
    if '.git' in dirpath: continue
    for f in fs:
        if f.endswith('.c'):
            files.append(os.path.join(dirpath, f))

# 特性统计
features = {
    'designated init (.x=)': re.compile(r'\.\w+\s*=\s*'),
    'compound literal ((S){)': re.compile(r'\(\s*(?:struct\s+\w+|typeof\s*\([^)]*\))\s*\)\s*\w*\s*\{'),
    'VLA 提示 (int a[n] / a[expr])': re.compile(r'\[\s*\w+\s*\](?!\s*\])'),
    '_Bool': re.compile(r'\b_Bool\b'),
    'inline': re.compile(r'\binline\b'),
    'typedef fnptr': re.compile(r'\(\s*\*\s*\w+\s*\)\s*\('),
    '变参 (... )': re.compile(r',\s*\.\.\.\s*\)'),
    '__attribute__': re.compile(r'__attribute__'),
    'goto': re.compile(r'\bgoto\b'),
    'union': re.compile(r'\bunion\b'),
    'enum': re.compile(r'\benum\b'),
    'static inline': re.compile(r'static\s+inline'),
    '宏函数式': re.compile(r'#define\s+\w+\s*\('),
    '字符串化 #': re.compile(r'#[#]|#[A-Za-z_]\w*#'),
    '变长参数宏': re.compile(r'#define\s+\w+\([^)]*\.\.\.'),
    'for 声明 (int i=)': re.compile(r'for\s*\(\s*(?:const\s+)?(?:int|char|unsigned|long|struct|size_t)\s+\w+\s*='),
    'long long': re.compile(r'\blong\s+long\b'),
    '位域': re.compile(r':\s*\d+\s*[;,}]'),
    '复合赋值指针': re.compile(r'[a-z]+\s*\[[^\]]*\]\s*(\+\+|--)'),
    'const': re.compile(r'\bconst\b'),
    'volatile': re.compile(r'\bvolatile\b'),
    '全局大数组': re.compile(r'static\s+\w+\s+\w+\[\s*\d+\s*\]\s*='),
}

print('文件数:', len(files))
print('=' * 60)
for name, pat in features.items():
    cnt = 0
    for f in files:
        try:
            d = open(f, 'rb').read()
            t = d.decode('utf-8', errors='replace')
        except Exception:
            continue
        cnt += len(pat.findall(t))
    print('%-38s %d' % (name, cnt))
