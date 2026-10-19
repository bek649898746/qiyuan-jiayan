# -*- coding: utf-8 -*-
"""启元编码自查工具 — 扫描源码文件编码健康度

用法: python scripts/check_encoding.py [root]
要求: .c/.h/.jy/.py 源文件必须是严格 UTF-8
非 UTF-8 → 退出码 1 (CI 可挂)

例外：**编码负例锚**（故意非 UTF-8，用来测编译器对坏字节的确定性拒绝 A6085）
必须跳过，否则判据误伤真代码。见 is_encoding_neg_anchor()。
(2026-10-20 CI 首次真正触发时抓到：feat_zh_utf8269_neg.c 被判非 UTF-8 而挂红)
"""
import os, sys

sys.stdout.reconfigure(encoding='utf-8')  # CI windows-latest locale=cp1252, 中文输出否则 UnicodeEncodeError (fix 2026-08-07)

EXTS = ('.c', '.h', '.jy', '.py')
SKIP_DIRS = ('.git', 'node_modules', 'target', 'quarantine', '_archived',
             '_retired', 'scratch', 'memory', '.openclaw', '__pycache__')


def is_encoding_neg_anchor(fn, path):
    """编码负例锚：**故意**非 UTF-8，用来测编译器对坏字节的确定性拒绝（A6085）。

    这类文件必须"坏"，否则测不出东西 —— 编码检查器应跳过，否则误伤。
    判据两条（任一命中即跳过）：
      1) 文件名含 utf8 且含 _neg
      2) 文件头含 @EXPECTED err:A6085
    """
    low = fn.lower()
    if 'utf8' in low and '_neg' in low:
        return True
    try:
        with open(path, 'rb') as f:
            head = f.read(4096)
        return b'@EXPECTED err:A6085' in head
    except Exception:
        return False


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
    skipped_neg = 0
    for dirpath, dirs, files in os.walk(root):
        # 剪枝跳过目录
        dirs[:] = [d for d in dirs if d not in SKIP_DIRS and not d.startswith('_')]
        for fn in files:
            if not fn.endswith(EXTS):
                continue
            if any(fn.endswith(e) for e in ('.bak', '.bak_gbk')) or '.bak' in fn:
                continue
            p = os.path.join(dirpath, fn)
            if is_encoding_neg_anchor(fn, p):
                skipped_neg += 1
                continue
            total += 1
            r = check_file(p)
            if r:
                bad.append((p, r))
    return total, bad, skipped_neg


def main():
    root = sys.argv[1] if len(sys.argv) > 1 else '.'
    root = os.path.abspath(root)
    total, bad, skipped_neg = scan(root)
    msg = f'编码检查: 扫描 {total} 个文件'
    if skipped_neg:
        msg += f'（跳过 {skipped_neg} 个编码负例锚 —— 它们故意非 UTF-8 以测 A6085）'
    print(msg)
    if not bad:
        print('✅ 全部严格 UTF-8')
        return 0
    print(f'❌ {len(bad)} 个非 UTF-8:')
    for p, r in sorted(bad):
        print(f'   [{r}] {p}')
    return 1


if __name__ == '__main__':
    sys.exit(main())
