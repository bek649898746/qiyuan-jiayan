# -*- coding: utf-8 -*-
"""check_serve —— `--serve` 长驻编译服务器的接线判据（施工单《负锚三缺口与选项接线判据》）。

为什么需要它：`scripts/_verify_serve.py` 早就写了且写法正确，但**全仓没有任何地方调用它**
⟹ `--serve` 长期处于 ORPHAN（有判据、判据不上流水线）。本脚本是进入默认判据链的入口。

判据（全部走真实子进程，不 mock）：
  ① 连发 2 条 `{"cmd":"ping"}`：应答非空、含 "ping"、两次都成立
  ② `{"cmd":"compile","src":...}`：`"ok":true` 且产物真实落盘
  ③ `{"cmd":"quit"}`：进程在 10s 内正常退出（rc=0）

用法: python scripts/check_serve.py [--cc <编译器 exe>]
默认: 环境变量 QCC_TEST_BIN，否则 qcc_zh_seed.exe → qcc_zh_v1.exe 依次探测。
"""
import argparse
import json
import os
import queue
import subprocess
import sys
import tempfile
import threading

sys.stdout.reconfigure(encoding='utf-8', errors='replace')
REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def pick_cc(explicit):
    if explicit:
        return explicit
    env = os.environ.get('QCC_TEST_BIN')
    if env:
        return env
    for name in ('qcc_zh_seed.exe', 'qcc_zh_v1.exe'):
        p = os.path.join(REPO, name)
        if os.path.exists(p):
            return p
    return 'qcc_zh_seed.exe'


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cc', default=None)
    a = ap.parse_args()
    cc = pick_cc(a.cc)
    if not os.path.isabs(cc):
        cc = os.path.join(REPO, cc)
    if not os.path.exists(cc):
        print('[FAIL] 编译器不存在: %s' % cc)
        return 1

    tmp = tempfile.mkdtemp(prefix='qcc_serve_')
    out_exe = os.path.join(tmp, 'serve_out.exe')
    p = subprocess.Popen([cc, '--serve'], cwd=REPO, stdin=subprocess.PIPE,
                         stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                         text=True, encoding='utf-8', errors='replace')
    q = queue.Queue()

    def reader():
        for line in p.stdout:
            q.put(line.rstrip('\n'))
    threading.Thread(target=reader, daemon=True).start()

    fails = []

    def send(obj):
        # 协议是紧凑 JSON（净浅 按 `"cmd":"` 原文匹配，不跳空格）⟹ 必须 separators 去空格
        p.stdin.write(json.dumps(obj, ensure_ascii=False, separators=(',', ':')) + '\n')
        p.stdin.flush()
        try:
            return q.get(timeout=15)
        except queue.Empty:
            return ''

    for i in (1, 2):
        resp = send({'cmd': 'ping'})
        if not resp:
            fails.append('ping#%d 应答为空（读行/协议 未接通）' % i)
        elif 'ping' not in resp:
            fails.append('ping#%d 应答不含 ping: %r' % (i, resp[:120]))

    resp = send({'cmd': 'compile', 'src': 'int main(){return 7;}', 'out': out_exe})
    if not resp:
        fails.append('compile 应答为空')
    elif '"ok":true' not in resp.replace(' ', ''):
        fails.append('compile 未成功: %r' % resp[:160])
    elif not os.path.exists(out_exe):
        fails.append('compile 报 ok 但产物不存在: %s' % out_exe)

    send({'cmd': 'quit'})
    try:
        rc = p.wait(timeout=10)
    except subprocess.TimeoutExpired:
        p.kill()
        rc = -1
        fails.append('quit 后 10s 未退出')
    err = (p.stderr.read() or '').strip().splitlines()
    if rc != 0 and not fails:
        fails.append('serve 退出码 %d' % rc)

    if fails:
        print('[FAIL] --serve 接线判据: %d 项' % len(fails))
        for f in fails:
            print('   - ' + f)
        if err:
            print('   stderr 末行: ' + err[-1][:200])
        return 1
    print('[PASS] --serve 接线判据: ping×2 / compile+产物 / quit 正常退出')
    return 0


if __name__ == '__main__':
    sys.exit(main())
