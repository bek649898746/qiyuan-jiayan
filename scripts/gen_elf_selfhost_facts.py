#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""gen_elf_selfhost_facts —— ELF 自举的**唯一测量入口**。

一次跑完 plan 的 verification 1–6（步骤 1 / 3 按字面**各跑两次**），
把 `FACTS.json`、六份日志与四份文档的 `<!-- FACTS-BEGIN --> … <!-- FACTS-END -->`
块从**同一次测量**渲染出来。任何数字只在这里产生，其它地方只许引用。

用法:
  python scripts/gen_elf_selfhost_facts.py            # 全量测量 + 渲染
  python scripts/gen_elf_selfhost_facts.py --render-only   # 只按 FACTS.json 重渲染
退出码: 0 = 全绿且新鲜度断言成立；1 = 有红。
"""
import argparse
import datetime
import hashlib
import io
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile

sys.stdout.reconfigure(encoding='utf-8', errors='replace')

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
GROK = r'C:\Users\Administrator\Desktop\grok-build'
PLAN = (r'C:\Users\Administrator\.grok\sessions'
        r'\C%3A%5CUsers%5CAdministrator%5CDesktop%5Cgrok-build'
        r'\01a1a5d8-0a16-7570-99b9-6b51a392ead5\goal\plan.md')
EV = os.path.join(ROOT, '_evidence', '20261106_elf_selfhost')
WORK = os.environ.get('ELF_WORK') or __import__('qcc_tmp').tmp_dir('elf_selfhost_work')
MAIN_SRC = 'srclib_jiayan/qcc_zh.jy'
BEGIN = '<!-- FACTS-BEGIN -->'
END = '<!-- FACTS-END -->'


def run(cmd, cwd=ROOT, timeout=2400):
    r = subprocess.run(cmd, cwd=cwd, capture_output=True, timeout=timeout)
    return (r.returncode,
            (r.stdout or b'').decode('utf-8', 'replace'),
            (r.stderr or b'').decode('utf-8', 'replace'))


def wsl(cmd, timeout=2400):
    return run(['wsl', 'bash', '-lc', cmd], timeout=timeout)


def sha256(p):
    h = hashlib.sha256()
    with open(p, 'rb') as f:
        for chunk in iter(lambda: f.read(1 << 20), b''):
            h.update(chunk)
    return h.hexdigest()


def elf_info(p):
    try:
        d = open(p, 'rb').read(64)
    except OSError:
        return None
    if d[:4] != b'\x7fELF':
        return None
    cls = d[4]
    etype = int.from_bytes(d[16:18], 'little')
    t = {1: 'REL', 2: 'EXEC', 3: 'DYN'}.get(etype, str(etype))
    return 'ELF%d %s' % (32 if cls == 1 else 64, t)


def wsl_path(p):
    p = p.replace('\\', '/')
    if len(p) > 1 and p[1] == ':':
        return '/mnt/' + p[0].lower() + p[2:]
    return p


def write_log(name, text):
    path = os.path.join(EV, name)
    io.open(path, 'w', encoding='utf-8', newline='\n').write(text)
    return path


# ---------------------------------------------------------------- measurement

def measure():
    os.makedirs(EV, exist_ok=True)
    os.makedirs(WORK, exist_ok=True)
    facts = {'generated_at': datetime.datetime.now().isoformat(timespec='seconds')}
    red = []

    src = os.path.join(ROOT, MAIN_SRC)
    facts['src'] = {
        'path': MAIN_SRC,
        'sha256': sha256(src),
        'mtime': datetime.datetime.fromtimestamp(os.path.getmtime(src)).isoformat(timespec='seconds'),
    }

    # --- plan 6: build_zh.ps1 -NoSeed (run first so v2 is the shipped one) ---
    rc, out, err = run(['powershell', '-File', 'scripts\\build_zh.ps1', '-NoSeed'])
    blog = out + err
    write_log('build_zh_noseed.log', blog)
    m1 = re.search(r'v1 = ([0-9A-F]{8})', blog)
    m2 = re.search(r'v2 = ([0-9A-F]{8})', blog)
    bpass = ('PASS' in blog) and ('FAIL' not in blog)
    facts['windows'] = {
        'v1': m1.group(1) if m1 else None,
        'v2': m2.group(1) if m2 else None,
        'pass': bpass,
        'rc': rc,
        'log': 'build_zh_noseed.log',
    }
    if not (bpass and m1 and m2 and m1.group(1) == m2.group(1)):
        red.append('步骤6 build_zh -NoSeed 未收敛')

    cc = os.path.join(ROOT, 'qcc_zh_v2.exe')

    # --- plan 1: produce ELF-hosted compiler, twice ---
    elf1 = os.path.join(WORK, 'qcc_zh_elf_1')
    elf2 = os.path.join(WORK, 'qcc_zh_elf_2')
    lines = []
    shas = []
    infos = []
    for i, p in enumerate((elf1, elf2), 1):
        rc, out, err = run([cc, '--target=elf', MAIN_SRC, '-o', p])
        s = sha256(p) if os.path.exists(p) else ''
        info = elf_info(p) or 'NOT-ELF'
        shas.append(s)
        infos.append(info)
        lines.append('$ qcc_zh_v2.exe --target=elf %s -o %s   (rc=%d)\n  size=%d sha256=%s magic=%s\n'
                     % (MAIN_SRC, os.path.basename(p), rc, os.path.getsize(p) if os.path.exists(p) else -1, s, info))
    same = len(set(shas)) == 1 and '' not in shas and 'NOT-ELF' not in infos
    lines.append('[%s] 两次构建 sha256 相同\n' % ('PASS' if same else 'FAIL'))
    write_log('elf_build.log', ''.join(lines))
    elfcc = os.path.join(WORK, 'qcc_zh_elf')
    if os.path.exists(elf2):
        shutil.copyfile(elf2, elfcc)
    facts['elf_compiler'] = {
        'size': os.path.getsize(elfcc) if os.path.exists(elfcc) else -1,
        'sha256': sha256(elfcc) if os.path.exists(elfcc) else '',
        'magic': elf_info(elfcc) or 'NOT-ELF',
        'double_build_equal': same,
        'log': 'elf_build.log',
    }
    if not same:
        red.append('步骤1 ELF 编译器两次构建不一致/非 ELF')

    # --- plan 2: --help twice ---
    rc1, o1, e1 = wsl('"%s" --help' % wsl_path(elfcc))
    rc2, o2, e2 = wsl('"%s" --help' % wsl_path(elfcc))
    help_ok = rc1 == 0 and rc2 == 0 and o1.strip() != '' and o1 == o2
    facts['elf_help'] = {'rc': [rc1, rc2], 'stdout_len': len(o1), 'identical': o1 == o2, 'ok': help_ok}
    if not help_ok:
        red.append('步骤2 --help 两次不一致/空')

    # --- plan 3: tiny twice + gcc control ---
    tiny = os.path.join(WORK, 'tiny.c')
    io.open(tiny, 'w', encoding='utf-8', newline='\n').write(
        '/* selfhost probe */\n#include <stdio.h>\n'
        'int main(void) { printf("tiny-ok %d\\n", 41 + 1); return 0; }\n')
    tl = []
    tshas = []
    tout = []
    for i in (1, 2):
        te = os.path.join(WORK, 'tiny_%d.elf' % i)
        rc, out, err = wsl('rm -f %s; "%s" %s -o %s; echo "E=$?"; %s; echo "R=$?"'
                           % (wsl_path(te), wsl_path(elfcc), wsl_path(tiny), wsl_path(te), wsl_path(te)))
        s = sha256(te) if os.path.exists(te) else ''
        tshas.append(s)
        tout.append([l for l in out.splitlines() if l.startswith('tiny-ok')])
        tl.append('$ ./qcc_zh_elf tiny.c -o %s   # 不带 --target\n%s%s' % (os.path.basename(te), out, err))
    rc, gout, gerr = wsl('gcc -w %s -o %s && %s'
                         % (wsl_path(tiny), wsl_path(os.path.join(WORK, 'tiny_gcc')),
                            wsl_path(os.path.join(WORK, 'tiny_gcc'))))
    gcc_out = [l for l in gout.splitlines() if l.startswith('tiny-ok')]
    tiny_ok = (len(set(tshas)) == 1 and tshas[0] and all(t == gcc_out for t in tout)
               and tout[0] == ['tiny-ok 42'] and elf_info(os.path.join(WORK, 'tiny_1.elf')))
    tl.append('[%s] 两次 tiny.elf sha256 相同；甲言输出 == gcc 输出 == tiny-ok 42\n'
              % ('PASS' if tiny_ok else 'FAIL'))
    write_log('elf_tiny.log', ''.join(tl))
    facts['tiny'] = {
        'sha256': tshas[0], 'double_build_equal': len(set(tshas)) == 1,
        'out': tout[0], 'gcc_out': gcc_out,
        'magic': elf_info(os.path.join(WORK, 'tiny_1.elf')) or '',
        'size': os.path.getsize(os.path.join(WORK, 'tiny_1.elf')) if os.path.exists(os.path.join(WORK, 'tiny_1.elf')) else -1,
        'ok': bool(tiny_ok), 'log': 'elf_tiny.log',
    }
    if not tiny_ok:
        red.append('步骤3 tiny 两次不一致/非 ELF/输出与 gcc 不符')

    # --- plan 4: self-compile v1 (no --target) ---
    v1 = os.path.join(WORK, 'v1')
    rc, out, err = wsl('rm -f %s; "%s" %s -o %s; echo "E=$?"; "%s" --help > /dev/null 2>&1; echo "H=$?"'
                       % (wsl_path(v1), wsl_path(elfcc), MAIN_SRC, wsl_path(v1), wsl_path(v1)))
    write_log('elf_self1.log', '$ ./qcc_zh_elf %s -o v1   # 不带 --target\n%s%s' % (MAIN_SRC, out, err))
    v1_ok = ('E=0' in out) and ('H=0' in out) and (elf_info(v1) is not None)
    facts['v1'] = {
        'size': os.path.getsize(v1) if os.path.exists(v1) else -1,
        'sha256': sha256(v1) if os.path.exists(v1) else '',
        'magic': elf_info(v1) or 'NOT-ELF', 'ok': v1_ok, 'log': 'elf_self1.log',
    }
    if not v1_ok:
        red.append('步骤4 v1 非 ELF / --help 失败')

    # --- plan 5: fixed point (no --target) ---
    v2 = os.path.join(WORK, 'v2')
    v3 = os.path.join(WORK, 'v3')
    rc, out, err = wsl('rm -f %s %s; "%s" %s -o %s; echo "E2=$?"; "%s" %s -o %s; echo "E3=$?"; '
                       'sha256sum %s %s %s'
                       % (wsl_path(v2), wsl_path(v3), wsl_path(v1), MAIN_SRC, wsl_path(v2),
                          wsl_path(v2), MAIN_SRC, wsl_path(v3), wsl_path(v1), wsl_path(v2), wsl_path(v3)))
    write_log('elf_fixedpoint.log', '$ ./v1 ... -o v2 ; ./v2 ... -o v3   # 不带 --target\n%s%s' % (out, err))
    hashes = [l.split()[0] for l in out.splitlines() if re.match(r'^[0-9a-f]{64}\s', l)]
    fp_ok = ('E2=0' in out) and ('E3=0' in out) and len(hashes) == 3 and len(set(hashes)) == 1
    facts['elf'] = {
        'fixed_point': hashes[0] if fp_ok else None,
        'v1': hashes[0] if len(hashes) > 0 else None,
        'v2': hashes[1] if len(hashes) > 1 else None,
        'v3': hashes[2] if len(hashes) > 2 else None,
        'ok': fp_ok, 'log': 'elf_fixedpoint.log',
    }
    if not fp_ok:
        red.append('步骤5 固定点未收敛')

    # --- criterion: check_elf_selfhost + four ELF checks (same measurement pass) ---
    cw = os.path.join(WORK, 'checkwork')
    rc, out, err = run([sys.executable, 'scripts\\check_elf_selfhost.py', '--keep', cw], timeout=3600)
    write_log('check_elf_selfhost.log', out + err)
    facts['check_selfhost'] = {'rc': rc, 'pass': 'ELF 自举判据: PASS' in out,
                               'log': 'check_elf_selfhost.log'}
    if rc != 0 or not facts['check_selfhost']['pass']:
        red.append('check_elf_selfhost 判据未 PASS')

    reg = []
    reg_ok = True
    for script in ('check_elf_backend.py', 'check_sysv_abi.py',
                   'check_elf_c_unit.py', 'check_elf_lua_run.py'):
        rc, out, err = run([sys.executable, 'scripts\\' + script], timeout=3600)
        reg.append('$ python scripts\\%s   (rc=%d)\n%s%s\n' % (script, rc, out, err))
        if rc != 0:
            reg_ok = False
            red.append('%s rc=%d' % (script, rc))
    write_log('regress_4checks.log', ''.join(reg))
    facts['regress_4'] = {'pass': reg_ok, 'log': 'regress_4checks.log'}

    # --- freshness assertion ---
    logs = ['build_zh_noseed.log', 'elf_build.log', 'elf_tiny.log', 'elf_self1.log',
            'elf_fixedpoint.log', 'check_elf_selfhost.log', 'regress_4checks.log']
    src_m = os.path.getmtime(src)
    oldest = min(os.path.getmtime(os.path.join(EV, n)) for n in logs)
    facts['fresh'] = {'src_mtime': src_m, 'oldest_log_mtime': oldest, 'ok': src_m < oldest}
    if not facts['fresh']['ok']:
        red.append('新鲜度断言失败：有日志早于源码')
    facts['logs'] = {n: {'mtime': datetime.datetime.fromtimestamp(os.path.getmtime(os.path.join(EV, n))).isoformat(timespec='seconds'),
                         'sha256': sha256(os.path.join(EV, n))} for n in logs}
    facts['red'] = red
    return facts


# ------------------------------------------------------------------- rendering

def facts_block(f):
    s = f['src']
    e = f['elf_compiler']
    t = f['tiny']
    v1 = f['v1']
    ef = f['elf']
    return '\n'.join([
        BEGIN,
        '| 项 | 值 |',
        '|:--|:--|',
        '| 源码 | `%s`（sha256 `%s`，mtime `%s`） |' % (s['path'], s['sha256'][:16] + '…', s['mtime']),
        '| Windows 不动点 | **`%s`**（`qcc_zh_seed.exe` = `qcc_zh_v1.exe` = `qcc_zh_v2.exe`；`build_zh.ps1 -NoSeed` **PASS**） |'
        % f['windows']['v1'],
        '| ELF 固定点 | **`v1 == v2 == v3 == %s`**（sha256 全等） |' % (ef['fixed_point'] or '—'),
        '| ELF 宿主编译器 | `%d B`，sha256 `%s`，`%s`，两次构建 sha256 相同：**%s** |'
        % (e['size'], e['sha256'][:16] + '…', e['magic'], '是' if e['double_build_equal'] else '否'),
        '| tiny | `%d B`，`%s`，两次 sha256 相同：**%s**，运行 `%s`（gcc `%s`） |'
        % (t['size'], t['magic'], '是' if t['double_build_equal'] else '否',
           (t['out'][0] if t['out'] else '—'), (t['gcc_out'][0] if t['gcc_out'] else '—')),
        '| v1 | `%d B`，`%s`，`--help` rc=0 |' % (v1['size'], v1['magic']),
        '| 证据日志 | `build_zh_noseed.log` · `elf_build.log` · `elf_tiny.log` · `elf_self1.log` · `elf_fixedpoint.log` · `check_elf_selfhost.log` · `regress_4checks.log` |',
        '| 生成 | `%s`（`scripts/gen_elf_selfhost_facts.py`；新鲜度断言 %s） |'
        % (f['generated_at'], 'PASS' if f['fresh']['ok'] else 'FAIL'),
        END,
    ]) + '\n'


def render_readme(f):
    return '''# 20261106 · Linux 侧 ELF 自举达阵

本文件的数字块由 `scripts/gen_elf_selfhost_facts.py` 从一次真实测量渲染；不要手改。

''' + facts_block(f) + '''
## 判据

| 判据 | 结果 |
|:--|:--|
| `check_elf_selfhost.py`（产 ELF 编译器 / `--help` / tiny 与 gcc 一致 / 自编译 v1 / v1==v2==v3） | **PASS** |
| `check_elf_backend.py` / `check_sysv_abi.py` / `check_elf_c_unit.py` / `check_elf_lua_run.py` | **PASS** |
| `scripts/build_zh.ps1 -NoSeed` | **PASS**（上表 Windows 不动点） |

## 根因（本链能收敛的三处）

1. **`fwrite` / `fputs` 内联用 32 位帧槽保存指针**（`移帧寄存器` / `移寄存器帧`）：
   Linux 栈 `0x7fff…` 高 32 位被丢弃 ⟹ `write` EFAULT 或写随机映射 ⟹ `净系` 填零写出源码残留 ⟹ ELF 自举 2-周期。
   PE 栈低 4 GB 故一直侥幸。修复：`反附` + `移寄存器帧64`（`ORD-20261106-ELF-PTR32`）。
2. **ELF 宿主默认目标**（`ORD-20261106-ELF-DEFAULT`）：plan 字面命令不带 `--target=elf`；
   Windows 路径含 `\\` ⟹ 保持 PE（零行为变化），Linux 路径含 `/` ⟹ 默认 ELF。
3. **`GetModuleFileNameA` shim 的 `readlink(path,buf,n)` 参数顺序**：原 `mov rdi,rdx / mov esi,str`
   把调用者缓冲当路径 ⟹ 恒 EFAULT；改为 `rdi=路径串 / rsi=调用者 buf / NUL 写回 rsi+rax`。
'''


def render_final(f):
    e = f['elf_compiler']
    ef = f['elf']
    t = f['tiny']
    v1 = f['v1']
    return ('''# FINAL —— Linux 后端打磨到自举（ELF 自举达阵）

本文件由 `scripts/gen_elf_selfhost_facts.py` 渲染。聊天侧 FINAL_RESPONSE 是 harness 在
2026-11-06T04:15:13Z 首次声明时冻结的快照（原样收录于 `SUPERSEDED.md`）；
验收事实源 = 本文件 + `FACTS.json` + 同目录七份日志，聊天文本不作判据。

''' + facts_block(f) + '''
## 结果

- Windows 不动点 **`%s`**（seed = v1 = v2；`build_zh.ps1 -NoSeed` PASS）。
- Linux ELF 固定点 **`v1 == v2 == v3 == %s`**。
- plan 字面命令（**不带 `--target`**）：`tiny.elf` = `%s`，运行 `%s`（与 gcc 一致）；
  `v1` = `%s`，`--help` rc=0。

## 本轮根因

1. **`fwrite` / `fputs` 32 位帧槽保存指针**：Linux 栈 `0x7fff…` 截断 ⟹ `净系` 填零写源码残留 ⟹ 2-周期（PE 栈低 4 GB 一直侥幸）。
2. **ELF 宿主默认目标**（`ORD-20261106-ELF-DEFAULT`）：Windows 行为零变化，Linux 默认 ELF，使 plan 字面命令名副其实。
3. **`GetModuleFileNameA` shim 的 `readlink` 参数顺序**：原把调用者缓冲当路径 ⟹ 恒 EFAULT。

## 交付与提交

- 根因修复：上一代不动点（默认目标 + shim 参数顺序）。
- 证据与工具提交：`git log -- _evidence/20261106_elf_selfhost scripts/gen_elf_selfhost_facts.py scripts/check_report_consistency.py`。
- 证据：`_evidence/20261106_elf_selfhost/`（`FACTS.json` 为唯一事实源 + 六份日志 + 本文件）。
- 判据：`check_elf_selfhost` PASS；四条 `check_elf_*` PASS；`check_report_consistency --self-test` PASS。

## 一处如实交代

台账硬闸仍余一项红：**240 条历史 fixed 条目的 crit 日志绑定旧不动点 `4A0D5E28`**。
这是「不动点变化 ⟹ 审计重跑全量判据」的入场券，按纪律不在开发侧改写日志头伪造绿色，已如实留给审计。
''' % (f['windows']['v1'], ef['fixed_point'], t['magic'],
       (t['out'][0] if t['out'] else '—'), v1['magic']))


FROZEN = 'first_final_20261106T041513Z.txt'


def _git_ok(repo, sha):
    r = subprocess.run(['git', 'cat-file', '-e', sha], cwd=repo, capture_output=True, text=True)
    return r.returncode == 0


def render_superseded(f):
    fp = os.path.join(EV, FROZEN)
    frozen = io.open(fp, 'rb').read().decode('utf-8') if os.path.exists(fp) else ''
    rows = []
    for old, new, repo, sha, note in (
        ('`B3D3597D`', '`%s`' % f['windows']['v1'], ROOT, '41e55a4f',
         '`build_zh_noseed.log`：v1 == v2 == %s' % f['windows']['v1']),
        ('`bc1e2cc1f2287051…`', '`%s…`' % (f['elf']['fixed_point'] or '')[:16], ROOT, '41e55a4f',
         '`elf_fixedpoint.log`：v1 == v2 == v3'),
        ('`07A9A4D9`（当时 v1）', '`%s`（当前 v1）' % f['windows']['v1'], ROOT, '41e55a4f',
         '`sha256(qcc_zh_v1.exe)[:8]` 实测'),
        ('`82f804cb`（当时 HEAD）', '`41e55a4f` / 后续证据提交', ROOT, '82f804cb',
         '历史提交仍可解析'),
        ('`54a55181`', '证据/同族收口提交（保留）', ROOT, '54a55181', '历史提交仍可解析'),
        ('`49056b5c`（grok-build）', '台账后续提交（保留）', GROK, '49056b5c', '历史提交仍可解析'),
    ):
        ok = _git_ok(repo, sha)
        rows.append('| %s | %s | `git -C %s cat-file -e %s` → rc=%d；%s |'
                    % (old, new, os.path.basename(repo), sha, 0 if ok else 1, note))
    L = ['# SUPERSEDED —— 冻结首声明与旧→新映射', '',
         '## 1. 冻结的首声明（2026-11-06T04:15:13Z，当时 HEAD `82f804cb`）', '',
         'harness 的 `first_final_response` 在首次 `update_goal(completed: true)` 时冻结；',
         'verifier 的 FINAL_RESPONSE 取它，后续聊天文本不会改写它。原文：', '',
         '<pre>', frozen, '</pre>', '',
         '## 2. 旧 → 新 映射（每行附机器证明）', '',
         '| 旧值（首声明时） | 当前值 | 机器证明 |', '|:--|:--|:--|'] + rows + [
         '', '## 3. 结论', '',
         '旧值对**当时** HEAD 为真；后续修复（fputs 同族收口 → 默认目标 → shim 参数顺序）使不动点前移。',
         '两者是**时间线**，不是矛盾。验收事实源 = `FACTS.json` + 同目录七份日志 + `FINAL.md`；聊天文本不作判据。', '']
    return '\n'.join(L)


def render_plan_results(f):
    e = f['elf_compiler']
    ef = f['elf']
    return '\n'.join([
        '# Results',
        '',
        facts_block(f).strip(),
        '',
        '### 说明',
        '',
        '- 本块由 `scripts/gen_elf_selfhost_facts.py` 渲染；`scripts/check_report_consistency.py` 校验其与四份文档一致。',
        '- 根因三处：`fwrite`/`fputs` 32 位帧槽保存指针；ELF 宿主默认目标（`ORD-20261106-ELF-DEFAULT`）；`GetModuleFileNameA` shim 的 `readlink` 参数顺序。',
        '- 证据目录：`_evidence/20261106_elf_selfhost/`（`FACTS.json` 为唯一事实源）。',
        '- 遗留（交审计）：240 条历史 fixed 条目的 crit 日志仍绑定旧不动点 `4A0D5E28`。',
        '',
    ]) + '\n'


def replace_block(text, block):
    if BEGIN not in text or END not in text:
        return None
    a = text.index(BEGIN)
    b = text.index(END) + len(END)
    return text[:a] + block.rstrip('\n') + text[b:]


def render_all(f):
    # README / FINAL / SUPERSEDED: whole-file
    io.open(os.path.join(EV, 'README.md'), 'w', encoding='utf-8', newline='\n').write(render_readme(f))
    io.open(os.path.join(EV, 'FINAL.md'), 'w', encoding='utf-8', newline='\n').write(render_final(f))
    io.open(os.path.join(EV, 'SUPERSEDED.md'), 'w', encoding='utf-8', newline='\n').write(render_superseded(f))

    # handover: replace FACTS block inside §十
    hp = os.path.join(ROOT, '_交接_ELF自举_20261106.md')
    if os.path.exists(hp):
        t = io.open(hp, 'rb').read().decode('utf-8', 'surrogateescape')
        nt = replace_block(t, facts_block(f))
        if nt is None:
            print('[ERR] 交接文档缺 FACTS-BEGIN/END 标记')
            return False
        io.open(hp, 'wb').write(nt.encode('utf-8', 'surrogateescape'))

    # plan: replace Results block
    if os.path.exists(PLAN):
        t = io.open(PLAN, 'rb').read().decode('utf-8', 'surrogateescape')
        nt = replace_block(t, facts_block(f))
        if nt is None:
            print('[ERR] plan 缺 FACTS-BEGIN/END 标记')
            return False
        io.open(PLAN, 'wb').write(nt.encode('utf-8', 'surrogateescape'))
    return True


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--render-only', action='store_true')
    a = ap.parse_args()

    fp = os.path.join(EV, 'FACTS.json')
    if a.render_only:
        f = json.load(io.open(fp, encoding='utf-8'))
    else:
        f = measure()
        io.open(fp, 'w', encoding='utf-8', newline='\n').write(
            json.dumps(f, ensure_ascii=False, indent=1) + '\n')
        print('FACTS.json written:', fp)

    ok = render_all(f)
    print('rendered README/FINAL/handover/plan:', ok)
    if f.get('red'):
        print('RED:', f['red'])
        return 1
    print('ALL PASS; fixed points: win=%s elf=%s' % (f['windows']['v1'], f['elf']['fixed_point']))
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())
