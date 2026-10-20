# -*- coding: utf-8 -*-
"""asm_zh 自宿主对齐验收（host vs v2 产物 SHA 必须一致）

来历：原先是 .github/workflows/ci.yml 里 asmzh job 的「自宿主对齐」步骤内联
      bash heredoc（`python - <<'PY'`），而该步骤的 shell 是 PowerShell —— **必红**。
      2026-10-21 启元审计（D41）实测：
          powershell -File 内执行 `python - <<'PY'`
          ⟹ Missing file specification after redirection operator.
          ⟹ The '<' operator is reserved for future use.

      故抽成独立脚本，不依赖任何 shell 特性（PowerShell / bash / cmd 都能调）。

用法（仓库根目录）：
    python scripts/check_asmzh_selfhost.py

前置（由 CI job 或本机准备）：
    qcc_x86.exe            宿主编译器（gcc -O2 -Wall -Werror srclib/qcc_x86.c -o qcc_x86.exe）
    asm_zh.exe             宿主汇编器（gcc -O2 -Wall -Werror srclib/asm_zh.c   -o asm_zh.exe）
    build_asmzh_v2.exe     qcc_x86.exe 编译 srclib/asm_zh.jy 得到的自宿主版本

判据：
    对每个样本源，用宿主汇编器与自宿主汇编器分别汇编同一份 .asm，
    两个产物的 SHA256 必须逐字节相同。任一不一致 ⟹ exit 1。

边界（诚实标注）：
    · 不改任何编译器行为，只做「两产物是否一致」的比对
    · 若 qcc -S 产出为空或缺失，直接判失败（不静默跳过）
"""
import hashlib
import os
import subprocess
import sys

FILES = [
    'tests/behavior/b_scanf.c',
    'tests/behavior/b_bitfield.c',
    'tests/qcc/math_call_test.c',
]

QCC = 'qcc_x86.exe'
ASM_HOST = 'asm_zh.exe'
ASM_V2 = 'build_asmzh_v2.exe'


def sha256_of(path):
    with open(path, 'rb') as f:
        return hashlib.sha256(f.read()).hexdigest()


def main():
    if hasattr(sys.stdout, 'reconfigure'):
        sys.stdout.reconfigure(encoding='utf-8')

    # ---- 前置件在位检查（缺件要明确报，不静默）----
    missing = [p for p in (QCC, ASM_HOST, ASM_V2) if not os.path.exists(p)]
    if missing:
        print(f'[FAIL] 缺前置件: {", ".join(missing)}（请在仓库根目录运行）')
        return 2

    ok = 0
    for i, src in enumerate(FILES):
        base = f'szh_ci_{i}'
        asm_out = base + '.asm'

        # ① qcc -S 生成汇编
        # ⚠️ 甲言 -S 的真实行为（2026-10-21 实测，勿"优化"）：
        #      `-o base.asm` 会同时产生 base.asm（246 KB，开头是 MZ = PE 文件！不可当汇编输入）
        #      与 base.asm.asm（1.16 MB，真汇编）。
        #     ⟹ 必须用 base.asm.asm。原先写成「两个都认、优先 base.asm」是我引入的 bug，
        #       当场实测被 asm_zh 报 `unknown instruction 'MZ' at line 1` 抓住。
        r = subprocess.run([QCC, '-S', src, '-o', asm_out],
                           capture_output=True, text=True,
                           encoding='utf-8', errors='replace')
        asm_file = asm_out + '.asm'
        if r.returncode != 0 or not os.path.exists(asm_file):
            print(f'[FAIL] {src}: qcc -S 失败 rc={r.returncode}（期望产物 {asm_file}）')
            if r.stdout:
                print('  stdout:', r.stdout.strip()[:400])
            if r.stderr:
                print('  stderr:', r.stderr.strip()[:400])
            return 1

        # ② 宿主汇编器
        r1 = subprocess.run([ASM_HOST, asm_file, '-o', base + '_h.exe'],
                            capture_output=True, text=True,
                            encoding='utf-8', errors='replace')
        if r1.returncode != 0 or not os.path.exists(base + '_h.exe'):
            print(f'[FAIL] {src}: 宿主汇编器失败 rc={r1.returncode} {(r1.stderr or "").strip()[:300]}')
            return 1

        # ③ 自宿主汇编器（v2）—— 超时 900s 与原文一致，防孤儿进程
        try:
            r2 = subprocess.run([ASM_V2, asm_file, '-o', base + '_v.exe'],
                                capture_output=True, text=True,
                                encoding='utf-8', errors='replace',
                                timeout=900)
        except subprocess.TimeoutExpired:
            print(f'[FAIL] {src}: 自宿主汇编器超时(900s)，已强杀')
            return 1
        if r2.returncode != 0 or not os.path.exists(base + '_v.exe'):
            print(f'[FAIL] {src}: 自宿主汇编器失败 rc={r2.returncode} {(r2.stderr or "").strip()[:300]}')
            return 1

        # ④ 逐字节比对
        h1 = sha256_of(base + '_h.exe')
        h2 = sha256_of(base + '_v.exe')
        if h1 != h2:
            print(f'[FAIL] {src}: host={h1[:16]} v2={h2[:16]} 不一致')
            return 1

        ok += 1
        print(f'  {src}: MATCH  ({h1[:16]})')

    print(f'asm_zh 自宿主对齐: {ok}/{len(FILES)} 一致  ⟹ PASS')
    return 0


if __name__ == '__main__':
    sys.exit(main())
