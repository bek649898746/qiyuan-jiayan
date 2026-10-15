/*
 * lspclient.js — 甲言 LSP 客户端（零依赖 · 纯 Node 内置）
 *
 * 为什么不用 vscode-languageclient：
 *   ⟹ 避免 npm 依赖；LSP 的帧协议我们已经完全掌握（Content-Length 帧），
 *     自己收发只要几十行，而且**可以脱离 VSCode 单独用 Node 测试** ✓
 *
 * ⚠️ 关键点：Content-Length 必须是【字节数】—— 用 Buffer.byteLength / Buffer.from(…,'utf8')
 *    中文一字 3 字节，用 String.length 会算错 ⟹ 帧就废了。
 */
const cp = require('child_process');

class 甲言LSP {
  constructor(exe, 日志) {
    this.exe = exe;
    this.日志 = 日志 || function () {};
    this.buf = Buffer.alloc(0);
    this.处理器 = null;
    this.proc = null;
  }

  启动() {
    const 我 = this;
    this.proc = cp.spawn(this.exe, [], { stdio: ['pipe', 'pipe', 'pipe'] });
    this.proc.stdout.on('data', function (d) {
      我.buf = Buffer.concat([我.buf, d]);
      我.切帧();
    });
    this.proc.stderr.on('data', function (d) {
      我.日志('[jylsp stderr] ' + d.toString('utf8').trim());
    });
    this.proc.on('error', function (e) {
      我.日志('[jylsp 启动失败] ' + e.message);
    });
    this.proc.on('exit', function (c) {
      我.日志('[jylsp 退出] code=' + c);
    });
  }

  切帧() {
    for (;;) {
      const i = this.buf.indexOf('\r\n\r\n');
      if (i < 0) return;
      const 头 = this.buf.slice(0, i).toString('ascii');
      const m = /Content-Length:\s*(\d+)/i.exec(头);
      if (!m) { this.buf = this.buf.slice(i + 4); continue; }
      const 长 = parseInt(m[1], 10);
      if (this.buf.length < i + 4 + 长) return;              // 帧未到齐
      const 体 = this.buf.slice(i + 4, i + 4 + 长).toString('utf8');
      this.buf = this.buf.slice(i + 4 + 长);
      try {
        const 对象 = JSON.parse(体);
        if (this.处理器) this.处理器(对象);
      } catch (e) {
        this.日志('[JSON 解析失败] ' + e.message);
      }
    }
  }

  发(对象) {
    if (!this.proc) return;
    const 体 = Buffer.from(JSON.stringify(对象), 'utf8');   // ★ 字节数
    this.proc.stdin.write('Content-Length: ' + 体.length + '\r\n\r\n');
    this.proc.stdin.write(体);
  }

  关() {
    try { this.proc.stdin.end(); } catch (e) {}
    try { this.proc.kill(); } catch (e) {}
  }
}

module.exports = { 甲言LSP };
