# -*- coding: utf-8 -*-
"""check_doc_examples —— 按施工线 README 抄命令**真能跑**（ORD-30 验收；2026-10-29）。

从 `README.md` 抽出 hello 示例的代码块与命令块，在干净临时目录原样执行；
并断言 README 里**不再出现** `qcc_x86.exe` 作为可执行入口 / 构建目标。

修前腿: 设 `QCC_DOC_README=<旧 README 路径>`（例如 `git show <commit>:README.md > old.md`）。
用法: python scripts/check_doc_examples.py [--cc <exe>]
"""
import io, os, re, shutil, subprocess, sys, tempfile
sys.stdout.reconfigure(encoding='utf-8', errors='replace')

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CC = os.path.join(REPO, 'qcc_zh_seed.exe')
S = os.environ.get('QCC_SCRATCH') or tempfile.mkdtemp(prefix='qcc_doc_')
ENV = dict(os.environ, TEMP=r'D:\qcc_tmp', TMP=r'D:\qcc_tmp')
readme = io.open(os.environ.get('QCC_DOC_README') or os.path.join(REPO, 'README.md'), encoding='utf-8').read()

fails, lines = [], []
lines.append('=== 文档示例真跑（README 施工线）===')

# ① README 不得把 qcc_x86.exe 当可执行入口（只允许出现在警示/说明里）
bad_cmd = [l for l in readme.split('\n')
           if re.search(r'^\s*\./?\s*qcc_x86\.exe\s', l) or re.search(r'gcc .*-o qcc_x86\.exe', l)]
lines.append('① qcc_x86.exe 作命令/构建目标 的行: %d %s' % (len(bad_cmd), bad_cmd[:3]))
if bad_cmd:
    fails.append('README 仍把 qcc_x86.exe 当入口')

# ② 抽出「步骤 3」的 C 代码块（hello.jy）与命令块，原样执行
blocks = re.findall(r'```(\w*)\n(.*?)```', readme, re.S)
c_blocks = [b for t, b in blocks if t == 'c' and 'jiayan hello 123' in b]
ps_blocks = [b for t, b in blocks if t in ('powershell', 'bash') and 'hello.jy -o hello.exe' in b]
if not c_blocks or not ps_blocks:
    fails.append('README 里找不到 hello 示例的代码块/命令块')
else:
    w = tempfile.mkdtemp(prefix='doc_hello_')
    src = os.path.join(w, 'hello.jy')
    io.open(src, 'w', encoding='utf-8', newline='\n').write(c_blocks[0])
    cmds = [l.strip() for l in ps_blocks[0].split('\n') if l.strip() and not l.strip().startswith('#')]
    lines.append('② 抽到 hello.jy %d B，命令 %s' % (len(c_blocks[0]), cmds))
    r = subprocess.run([CC, src, '-o', os.path.join(w, 'hello.exe')], cwd=w,
                       capture_output=True, text=True, encoding='utf-8', errors='replace', env=ENV)
    lines.append('   编译 rc=%d %s' % (r.returncode, (r.stdout + r.stderr).strip()[-120:]))
    if r.returncode != 0:
        fails.append('README 的 hello 命令编不过')
    else:
        q = subprocess.run([os.path.join(w, 'hello.exe')], cwd=w, capture_output=True, text=True,
                           encoding='utf-8', errors='replace', timeout=30)
        lines.append('   运行 rc=%d out=%r' % (q.returncode, (q.stdout or '').strip()))
        if (q.stdout or '').strip() != 'jiayan hello 123':
            fails.append('hello 输出与文档不符')

# ③ 中文关键字 hello（步骤 4）
zh_c = [b for t, b in blocks if t == 'c' and '你好，甲言' in b]
zh_ps = [b for t, b in blocks if t in ('powershell', 'bash') and 'hello_zh.jy' in b]
if zh_c and zh_ps:
    w = tempfile.mkdtemp(prefix='doc_hellozh_')
    src = os.path.join(w, 'hello_zh.jy')
    io.open(src, 'w', encoding='utf-8', newline='\n').write(zh_c[0])
    r = subprocess.run([CC, src, '-o', os.path.join(w, 'hello_zh.exe')], cwd=w, capture_output=True,
                       text=True, encoding='utf-8', errors='replace', env=ENV)
    ok = r.returncode == 0
    out = ''
    if ok:
        q = subprocess.run([os.path.join(w, 'hello_zh.exe')], cwd=w, capture_output=True, text=True,
                           encoding='utf-8', errors='replace', timeout=30)
        out = (q.stdout or '').strip()
    lines.append('③ 中文 hello 编译 rc=%d out=%r' % (r.returncode, out))
    if not ok or out != '你好，甲言！':
        fails.append('中文 hello 与文档不符')

# ④ 仓库自带的 examples/hello.jy
r = subprocess.run([CC, os.path.join(REPO, 'examples', 'hello.jy'), '-o', os.path.join(S, 'ex_hello.exe')],
                   capture_output=True, text=True, encoding='utf-8', errors='replace', env=ENV)
ok = r.returncode == 0
out = ''
if ok:
    q = subprocess.run([os.path.join(S, 'ex_hello.exe')], capture_output=True, text=True,
                       encoding='utf-8', errors='replace', timeout=30)
    out = (q.stdout or '').strip()
lines.append('④ examples/hello.jy 编译 rc=%d out=%r' % (r.returncode, out))
if not ok:
    fails.append('examples/hello.jy 编不过')

lines.append('结论: %s%s' % ('PASS' if not fails else 'FAIL ', '' if not fails else str(fails)))
txt = '\n'.join(lines)
print(txt)
io.open(os.path.join(S, 'doc_examples.log'), 'w', encoding='utf-8', newline='\n').write(txt + '\n')
sys.exit(0 if not fails else 1)
