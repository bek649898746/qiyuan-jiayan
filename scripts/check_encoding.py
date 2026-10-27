# -*- coding: utf-8 -*-
"""启元编码自查工具 — 扫描源码文件编码健康度
用法: python scripts/check_encoding.py [root]
要求: .c/.h/.jy/.py/.ps1 源文件必须是严格 UTF-8
非 UTF-8 → 退出码 1 (CI 可挂)
2026-10-27 (bootstrap P0): 纳入 .ps1，并显式识别 UTF-16 BOM 与 NUL 字节 ——
  原白名单没有 .ps1 ⟹ publish 上的 build_zh.ps1 编码事故 在 CI 第一道门 从未被拦。
2026-10-27 (skeptic 复核): 再加 **含非 ASCII 的 .ps1 必须带 UTF-8 BOM** ——
  只查 UTF-8 可解码会放过「无 BOM + 中文变量名」这种真实 P0 输入（PS5.1 按 ANSI 读 ⟹ 112 错）。
"""
import os, sys, subprocess

sys.stdout.reconfigure(encoding='utf-8')  # CI windows-latest locale=cp1252, 中文输出否则 UnicodeEncodeError (fix 2026-08-07)

EXTS = ('.c', '.h', '.jy', '.py', '.ps1')
SKIP_DIRS = ('.git', 'node_modules', 'target', 'quarantine', '_archived',
             '_retired', 'scratch', 'memory', '.openclaw', '__pycache__')
# 故意的非 UTF-8 输入：A6085 (字符串字面量 UTF-8 规范性校验) 的负锚 **必须** 内含 0xFF 0xFE，
# 否则它测不到“非法字节必红”。这不是待修项，是测试夹具。
ALLOW = ('scripts/feat_zh_utf8269_neg.c',)


def tracked_files(root):
    """只扫 git 已跟踪的文件：本地工作区常驻大量未跟踪的调试产物（UTF-16 日志等），
    全扫会把真信号埋进噪声里。git 不可用时退回全扫。"""
    try:
        r = subprocess.run(['git', '-C', root, 'ls-files', '-z'], capture_output=True)
        if r.returncode == 0:
            return set(x for x in r.stdout.decode('utf-8', 'replace').split('\x00') if x)
    except Exception:
        pass
    return None

def check_file(path):
    with open(path, 'rb') as f:
        data = f.read()
    # UTF-16 的 BOM 不是合法 UTF-8 起始字节，但纯 ASCII 的 UTF-16 LE 正文
    # （每个字符后跟 \x00）能被 UTF-8 解码成功 ⟹ 必须显式拦住，否则会静默放过。
    if data.startswith(b'\xff\xfe') or data.startswith(b'\xfe\xff'):
        return 'NOT_UTF8(UTF-16 BOM)'
    # 2026-10-27 (skeptic 复核): **含非 ASCII 的 .ps1 必须带 UTF-8 BOM** —— 只查
    # 「UTF-8 可解码」不够：publish 的 build_zh.ps1 是合法 UTF-8（无 BOM）+ 343 个
    # 中文变量名，PS 5.1 在英文 runner 按 ANSI 读 ⟹ 112 个 ParserError。原闸对这一
    # 真实 P0 输入是绿的 ⟹ 必须把 BOM 规则写进闸，才算根治。
    if path.lower().endswith('.ps1') and not data.startswith(b'\xef\xbb\xbf'):
        if any(b >= 0x80 for b in data):
            return 'NOT_BOM(non-ASCII .ps1 without UTF-8 BOM)'
    try:
        text = data.decode('utf-8')
    except UnicodeDecodeError:
        return 'NOT_UTF8'
    except Exception as e:
        return f'ERR:{e}'
    if '\x00' in text:
        return 'NOT_UTF8(NUL byte)'
    return None  # OK

def scan(root):
    bad = []
    total = 0
    tracked = tracked_files(root)
    for dirpath, dirs, files in os.walk(root):
        # 剪枝跳过目录
        dirs[:] = [d for d in dirs if d not in SKIP_DIRS and not d.startswith('_')]
        for fn in files:
            if not fn.endswith(EXTS):
                continue
            if any(fn.endswith(e) for e in ('.bak', '.bak_gbk')) or '.bak' in fn:
                continue
            p = os.path.join(dirpath, fn)
            rel = os.path.relpath(p, root).replace('\\', '/')
            if rel in ALLOW:
                continue
            if tracked is not None and rel not in tracked:
                continue
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
