# -*- coding: utf-8 -*-
"""check_p0_bootstrap —— P0 自举入口证据（可复跑；替代上一轮手工拼的那份 logs/p0_bootstrap.log）。

为什么存在（2026-10-28 · R900 收尾）：
  `verify_plan_steps.py` 的 `bootstrap` 一步要求 `logs/p0_bootstrap.log` 里同时出现
  `中文变量名赋值行: 0`、`PARSE_OK`、`NOT_UTF8(UTF-16 BOM)` 三个观测。上一轮那份是**手工拼**的，
  换一轮就没人能复算。本脚本把它做成**一次可复跑的命令**，三项全部现场取证：

  ① `scripts/build_zh.ps1` 的编码面：是否带 UTF-8 BOM、中文变量名死赋值行数（必须 0）；
  ② PowerShell `Parser::ParseFile` 解析该脚本 ⟹ `PARSE_OK`（0 错误）；
  ③ 造一个 UTF-16LE+BOM 的 .ps1 样本，用**出厂的** `scripts/check_encoding.py` 扫它 ⟹
     必须 rc=1 且报 `NOT_UTF8(UTF-16 BOM)`（证明编码闸真能拦住这类事故）；
  ④ 若给了 `--bootstrap-log`，把其中的 `v1 = …` / `v2 = …` 摘要附在末尾。

用法: python scripts/check_p0_bootstrap.py [--bootstrap-log <logs/bootstrap.log>] [--out <file>]
退出码: 0 = 三项全部成立；1 = 任一不成立（不得当绿）
"""
import argparse
import io
import os
import re
import subprocess
import sys
import tempfile

sys.stdout.reconfigure(encoding='utf-8', errors='replace')
REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PS1 = os.path.join(REPO, 'scripts', 'build_zh.ps1')


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--bootstrap-log', default='')
    ap.add_argument('--out', default='')
    a = ap.parse_args()
    L, bad = [], []

    L.append('=== P0 bootstrap 证据（check_p0_bootstrap.py 现场取证）===')
    data = open(PS1, 'rb').read()
    bom = data.startswith(b'\xef\xbb\xbf')
    txt = data.decode('utf-8-sig', 'replace')
    cn = sum(1 for l in txt.splitlines() if re.match(r'^\s*\$expected_[\u4e00-\u9fff]', l))
    dead = sum(1 for l in txt.splitlines() if re.match(r'^\s*\$expected_[^\s=]*\s*=', l))
    L.append('build_zh.ps1 %d B · head=%r BOM=%s' % (len(data), data[:3], bom))
    L.append('中文变量名赋值行: %d（期望 0）· $expected_* 赋值行: %d' % (cn, dead))
    if cn != 0:
        bad.append('中文变量名赋值行=%d' % cn)

    w = tempfile.mkdtemp(prefix='qcc_p0boot_')
    parse = os.path.join(w, 'parse.ps1')
    io.open(parse, 'w', encoding='utf-8-sig', newline='\r\n').write(
        '$src = $args[0]\n'
        '$err = $null; $tok = $null\n'
        '$null = [System.Management.Automation.Language.Parser]::ParseFile($src, [ref]$tok, [ref]$err)\n'
        'if ($err.Count -gt 0) { $err | Select-Object -First 5 | ForEach-Object { "L$($_.Extent.StartLineNumber): $($_.Message)" };'
        ' Write-Host "PARSE_FAIL count=$($err.Count)"; exit 1 }\n'
        'Write-Host "PARSE_OK"\n')
    pr = subprocess.run(['powershell', '-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', parse, PS1],
                        capture_output=True, text=True, encoding='utf-8', errors='replace')
    pout = ((pr.stdout or '') + (pr.stderr or '')).strip()
    L.append('Parser::ParseFile: rc=%d %s' % (pr.returncode, 'PARSE_OK' if 'PARSE_OK' in pout else pout[:200]))
    if pr.returncode != 0 or 'PARSE_OK' not in pout:
        bad.append('PARSE_OK')

    # ③ UTF-16 样本必须被判红
    u16dir = os.path.join(w, 'u16')
    os.makedirs(u16dir, exist_ok=True)
    io.open(os.path.join(u16dir, 'u16.ps1'), 'wb').write(b'\xff\xfe' + '写 主 { }'.encode('utf-16-le'))
    ce = subprocess.run([sys.executable, '-X', 'utf8', os.path.join(REPO, 'scripts', 'check_encoding.py'), u16dir],
                        capture_output=True, text=True, encoding='utf-8', errors='replace')
    cout = ((ce.stdout or '') + (ce.stderr or '')).strip().replace('\n', ' | ')
    ok_enc = (ce.returncode != 0 and 'NOT_UTF8(UTF-16 BOM)' in cout)
    L.append('UTF-16 样本: rc=%d %s' % (ce.returncode, cout[:300]))
    if not ok_enc:
        bad.append('UTF-16 样本未被判红')

    if a.bootstrap_log and os.path.exists(a.bootstrap_log):
        b = open(a.bootstrap_log, 'rb').read()
        t = b.decode('utf-16-le', 'replace') if b[:2] == b'\xff\xfe' else b.decode('utf-8', 'replace')
        # 2026-10-30：不动点 = **相邻两代全等**，不必是 v1==v2 —— 本轮两刀都需 3 代才稳定
        # （v1 由 seed 编新源码，v2 由 v1 编，v3 由 v2 编；`v2 == v3` 即收敛）。
        seq = re.findall(r'v(\d+) = ([0-9A-F]{8})', t)
        seq = [(int(n), h) for n, h in seq]
        seq.sort()
        hs = [h for _n, h in seq]
        conv = len(hs) >= 2 and hs[-1] == hs[-2]
        L.append('自举: %s → 最后两代 %s PASS=%s'
                 % (' '.join('v%d=%s' % (n, h) for n, h in seq),
                    '全等' if conv else '不等', conv))
        if not conv:
            bad.append('bootstrap 最后两代不全等（%s）' % hs)

    L.append('结论: %s%s' % ('PASS' if not bad else 'FAIL ', '' if not bad else str(bad)))
    out = '\n'.join(L)
    print(out)
    if a.out:
        io.open(a.out, 'w', encoding='utf-8', newline='\n').write(out + '\n')
    return 0 if not bad else 1


if __name__ == '__main__':
    sys.exit(main())
