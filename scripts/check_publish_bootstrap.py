# -*- coding: utf-8 -*-
"""check_publish_bootstrap —— publish 分支 build_zh.ps1 的编码/解析永久闸。

为什么需要它（skeptic 复核）：
  `check_encoding.py .` 只扫**当前工作树**，抓不到 origin/publish 上的原文件；
  而 CI 真正 checkout 的是 publish。原缺陷（无 BOM + 343 中文变量名）是**合法 UTF-8**，
  旧闸对它是绿的。本闸直接 `git show <ref>:scripts/build_zh.ps1`，做三件事：
    ① 含非 ASCII ⟹ 必须带 UTF-8 BOM；
    ② 中文变量名死赋值行必须为 0；
    ③ PowerShell Parser::ParseFile 必须 0 错误。
  在修复合入 publish 之前，本闸对 `origin/publish` **应当红**（blocked 现场）。

用法: python scripts/check_publish_bootstrap.py [--ref origin/publish]
退出: 0 = 三项全过；1 = 有红（当前 origin/publish 未修时即 1）
"""
import argparse
import os
import re
import subprocess
import sys
import tempfile

sys.stdout.reconfigure(encoding='utf-8', errors='replace')
REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--ref', default='origin/publish')
    a = ap.parse_args()

    r = subprocess.run(['git', 'show', '%s:scripts/build_zh.ps1' % a.ref],
                       cwd=REPO, capture_output=True)
    if r.returncode != 0:
        print('[FAIL] 取不到 %s:scripts/build_zh.ps1' % a.ref)
        return 1
    data = r.stdout
    w = tempfile.mkdtemp(prefix='qcc_pub_boot_')
    p = os.path.join(w, 'build_zh.ps1')
    open(p, 'wb').write(data)

    bom = data.startswith(b'\xef\xbb\xbf')
    try:
        t = data.decode('utf-8-sig')
    except UnicodeDecodeError:
        t = ''
    pat = re.compile(r'^\s*\$expected_([^\s=]*[\u4e00-\u9fff][^\s=]*)\s*=')
    cn = sum(1 for l in t.splitlines() if pat.match(l))
    dead = sum(1 for l in t.splitlines() if re.match(r'^\s*\$expected_', l))

    parse = os.path.join(w, 'parse.ps1')
    open(parse, 'w', encoding='utf-8-sig', newline='\r\n').write('''
$src = $args[0]
$err = $null; $tok = $null
$null = [System.Management.Automation.Language.Parser]::ParseFile($src, [ref]$tok, [ref]$err)
if ($err.Count -gt 0) { $err | Select-Object -First 5 | ForEach-Object { "L$($_.Extent.StartLineNumber): $($_.Message)" }; Write-Host "PARSE_FAIL count=$($err.Count)"; exit 1 }
Write-Host "PARSE_OK"
''')
    pr = subprocess.run(['powershell', '-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', parse, p],
                        capture_output=True, text=True, encoding='utf-8', errors='replace')
    pout = ((pr.stdout or '') + (pr.stderr or '')).strip()

    print('ref=%s size=%d head=%r BOM=%s 中文赋值行=%d 死赋值行=%d' % (
        a.ref, len(data), data[:4], bom, cn, dead))
    print('Parser: rc=%d %s' % (pr.returncode, pout.replace('\n', ' | ')[:400]))
    fails = []
    if not bom and any(b >= 0x80 for b in data):
        fails.append('non-ASCII .ps1 without UTF-8 BOM')
    if cn != 0:
        fails.append('中文变量名赋值行=%d（期望 0）' % cn)
    if dead != 0:
        fails.append('$expected_* 死赋值行=%d（期望 0；与 main 对齐）' % dead)
    if pr.returncode != 0:
        fails.append('Parser rc=%d' % pr.returncode)
    if fails:
        print('[FAIL] publish bootstrap: ' + '; '.join(fails))
        return 1
    print('[PASS] publish bootstrap: BOM / 中文赋值 0 / Parser 0')
    return 0


if __name__ == '__main__':
    sys.exit(main())
