# -*- coding: utf-8 -*-
"""check_serve —— `--serve` 长驻编译服务器的接线判据（施工单《负锚三缺口与选项接线判据》）。

为什么需要它：`scripts/_verify_serve.py` 早就写了且写法正确，但**全仓没有任何地方调用它**
⟹ `--serve` 长期处于 ORPHAN（有判据、判据不上流水线）。本脚本是进入默认判据链的入口。

判据（全部走真实子进程，不 mock）：
  ① 连发 2 条 `{"cmd":"ping"}`：应答非空、含 "ping"、两次都成立
  ② ping 应答是**标准 JSON**（`json.loads` 可解析 + `ok:true` + `cmd:ping`）
  ③ 协议**对空白容错**：`{"cmd": "ping"}`（标准序列化器默认写法）也能识别
  ④ `{"cmd":"compile","src":...}`：`"ok":true` 且产物真实落盘，且**句柄已释放**（可立即重命名）
  ⑤ `{"cmd":"quit"}`：**有 `{"ok":true,"cmd":"quit"}` 回包**，且进程在 10s 内正常退出（rc=0）

2026-10-28（R891）补 ②③④⑤：原判据只验「含 ping 字样 / ok:true / 进程退出」，
实测漏掉三个真缺口 —— ping 回的是甲言 JSON 方言（`{"成功":真}`，`真` 不是 JSON 真值，客户端 `json.loads` 必失败）、
`{"cmd": "ping"}` 的空白不认（任何标准序列化器默认带空格）、编译后产物句柄不释放
（`--serve` 长驻时客户端编译完无法运行或替换产物，实测 PermissionError）。

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

    def send(obj, compact=True):
        # 紧凑 JSON 是历史协议（`净浅` 曾按 `"cmd":"` 原文匹配）；
        # 2026-10-28 起 `净浅` 也容忍 `"cmd": "ping"` 的空白 ⟹ compact=False 用于验证这条容错。
        s = json.dumps(obj, ensure_ascii=False,
                       separators=(',', ':') if compact else None)
        try:                       # R905d: serve 已死（崩/被我们判据打死）时 write/flush 抛 OSError ⟹ 退化成空应答，
            p.stdin.write(s + '\n')  # 由调用处 断言「无应答」报 FAIL，而不是让判据脚本自己炸出 traceback。
            p.stdin.flush()
        except (OSError, ValueError):
            return ''
        try:
            return q.get(timeout=15)
        except queue.Empty:
            return ''

    def parse_ok(s):
        try:
            json.loads(s)
            return True
        except Exception:
            return False

    for i in (1, 2):
        resp = send({'cmd': 'ping'})
        if not resp:
            fails.append('ping#%d 应答为空（读行/协议 未接通）' % i)
        elif 'ping' not in resp:
            fails.append('ping#%d 应答不含 ping: %r' % (i, resp[:120]))

    # 2026-10-28（R891）新增三条：都是实测踩到的真缺口
    # ① 应答必须是**标准 JSON**（原 ping 回 `{"成功":真,...}`，`真` 不是 JSON 真值 ⟹ 任何 json.loads 客户端失败）
    resp = send({'cmd': 'ping'})
    if not parse_ok(resp):
        fails.append('ping 应答不是标准 JSON（json.loads 失败）: %r' % resp[:140])
    else:
        d = json.loads(resp)
        if d.get('ok') is not True or d.get('cmd') != 'ping':
            fails.append('ping 标准键缺失/不符（需要 ok:true 与 cmd:ping）: %r' % resp[:140])
    # ② 协议对空白容错（标准序列化器默认输出 `"cmd": "ping"`）
    resp_sp = send({'cmd': 'ping'}, compact=False)
    if '"ok":true' not in resp_sp.replace(' ', ''):
        fails.append('带空格 JSON 未被识别（`{"cmd": "ping"}` ⟹ %r）' % resp_sp[:120])

    resp = send({'cmd': 'compile', 'src': 'int main(){return 7;}', 'out': out_exe})
    if not resp:
        fails.append('compile 应答为空')
    elif '"ok":true' not in resp.replace(' ', ''):
        fails.append('compile 未成功: %r' % resp[:160])
    elif not os.path.exists(out_exe):
        fails.append('compile 报 ok 但产物不存在: %s' % out_exe)
    else:
        # ③ 编译后**产物句柄必须已释放**（serve 长驻时原实现每个文件留句柄 ⟹ 客户端无法运行/替换产物）
        try:
            os.replace(out_exe, out_exe + '.moved')
            os.replace(out_exe + '.moved', out_exe)
        except OSError as e:
            fails.append('compile 后产物仍被 serve 占用（无法重命名：%s）' % e.__class__.__name__)

    # ⑥ 同进程**双源复用**回归（2026-10-29 · 审计 skeptic 1 实测缺口的钉子）：
    #    节点号跨编译复用 ⟹ 逐节点平表（`转基浮[]` 等）必须随节点清零；否则上一轮 `(float*)`
    #    转型位残留，本轮 `*(&整g)` 被判 双精度 ⟹ 合法程序 A9002 拒编（实测：serve 连编两条即现；
    #    单次调用 4/4 PASS ⟹ 只有长驻场景暴露）。修法见 `建节`/`终子` 的 `转基浮[计]=0` / memset。
    out2 = os.path.join(tmp, 'serve_reuse2.exe')
    r1 = send({'cmd': 'compile', 'src': 'int main(){ float f = 1.5f; float h = *(float*)&f; return (int)h; }',
               'out': out_exe})
    if '"ok":true' not in (r1 or '').replace(' ', ''):
        fails.append('双源复用#1（(float*) 转型）未成功: %r' % (r1 or '')[:160])
    r2 = send({'cmd': 'compile', 'src': 'int main(){ int g = 7; int h = *(&g); return h; }', 'out': out2})
    if not r2:
        fails.append('双源复用#2（*(&整g)）无应答 —— 疑 serve 进程已被上一轮节点的残留状态打死')
    elif '"ok":true' not in r2.replace(' ', ''):
        fails.append('双源复用#2（*(&整g)）被上一轮转型位污染: %r' % r2[:160])
    elif not os.path.exists(out2):
        fails.append('双源复用#2 报 ok 但产物不存在: %s' % out2)

    qresp = send({'cmd': 'quit'})
    if '"ok":true' not in (qresp or '').replace(' ', ''):
        fails.append('quit 回包缺失或非 ok:true: %r' % (qresp or '')[:120])
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
    print('[PASS] --serve 接线判据: ping×2（标准 JSON）/ 带空格容错 / compile+产物+句柄释放 / quit 回包+退出')
    return 0


if __name__ == '__main__':
    sys.exit(main())
