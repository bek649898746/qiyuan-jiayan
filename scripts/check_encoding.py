# -*- coding: utf-8 -*-
"""启元编码自查工具 — 扫描源码文件编码健康度
用法: python scripts/check_encoding.py [root]
要求: .c/.h/.jy/.py 源文件必须是严格 UTF-8
非 UTF-8 → 退出码 1 (CI 可挂)
"""
import os, sys

sys.stdout.reconfigure(encoding='utf-8')  # CI windows-latest locale=cp1252, 中文输出否则 UnicodeEncodeError (fix 2026-08-07)

EXTS = ('.c', '.h', '.jy', '.py')
SKIP_DIRS = ('.git', 'node_modules', 'target', 'quarantine', '_archived',
             '_retired', 'scratch', 'memory', '.openclaw', '__pycache__')

# 例外：**故意含非法 UTF-8 字节**的负例锚。
# 这类文件测的就是「编译器必须拒绝非法编码」，它本身**必须**非 UTF-8 ——
# 否则锚就失去了被测对象。白名单按文件名精确匹配，不做通配，避免误放。
ALLOW_NON_UTF8 = (
    'feat_zh_utf8269_neg.c',   # 族14 #269（R551）：字符串字面量含非法 UTF-8 ⟹ 期望 A6085 拒绝
)

def check_file(path):
    with open(path, 'rb') as f:
        data = f.read()
    try:
        data.decode('utf-8')
        return None  # OK
    except UnicodeDecodeError:
        return 'NOT_UTF8'
    except Exception as e:
        return f'ERR:{e}'

def scan(root):
    bad = []
    total = 0
    for dirpath, dirs, files in os.walk(root):
        # 剪枝跳过目录
        dirs[:] = [d for d in dirs if d not in SKIP_DIRS and not d.startswith('_')]
        for fn in files:
            if not fn.endswith(EXTS):
                continue
            if any(fn.endswith(e) for e in ('.bak', '.bak_gbk')) or '.bak' in fn:
                continue
            if fn in ALLOW_NON_UTF8:
                continue   # 故意的负例锚：必须非 UTF-8，见 ALLOW_NON_UTF8 注释
            p = os.path.join(dirpath, fn)
            total += 1
            r = check_file(p)
            if r:
                bad.append((p, r))
    return total, bad

def main():
    root = sys.argv[1] if len(sys.argv) > 1 else '.'
    root = os.path.abspath(root)
    total, bad = scan(root)
    print(f'编码检查: 扫描 {total} 个文件')
    if not bad:
        print('✅ 全部严格 UTF-8')
        return 0
    print(f'❌ {len(bad)} 个非 UTF-8:')
    for p, r in sorted(bad):
        print(f'   [{r}] {p}')
    return 1

if __name__ == '__main__':
    sys.exit(main())
