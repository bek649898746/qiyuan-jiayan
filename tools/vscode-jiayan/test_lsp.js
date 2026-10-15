/*
 * test_lsp.js — 甲言 LSP 客户端【脱离 VSCode 的单测】
 *
 * 为什么能这么测：lspclient.js 是零依赖的纯 Node 模块 ⟹ 可以直接跑真进程、真协议 ✓
 * 这比"装进 VSCode 手工点"硬得多：**同一份客户端代码，在这里过，在 VSCode 里就是同一份代码** ✓
 *
 * 用法：node test_lsp.js
 * 判据：① initialize 有响应（result.capabilities）② 收到 publishDiagnostics 且诊断数 > 0
 *       ③ 帧全部合法（客户端解析出来即证明）
 */
const fs = require('fs');
const path = require('path');
const { 甲言LSP } = require('./lspclient');

const 目录 = __dirname;
const exe = path.join(目录, '..', 'jylsp.exe');
const 目标源 = path.join(目录, '..', 'bad3.c');
const uri = 'file:///' + 目标源.replace(/\\/g, '/').replace(/^\//, '');

if (!fs.existsSync(exe)) { console.log('找不到 jylsp.exe: ' + exe); process.exit(2); }
if (!fs.existsSync(目标源 + '.jyan.json')) { console.log('缺诊断文件: ' + 目标源 + '.jyan.json'); process.exit(2); }

const 收到 = [];
const lsp = new 甲言LSP(exe, function (m) { console.log('[日志] ' + m); });
lsp.处理器 = function (m) {
  收到.push(m);
  if (m.method === 'textDocument/publishDiagnostics') {
    console.log('== 收到 publishDiagnostics ==');
    console.log('   uri = ' + m.params.uri);
    console.log('   诊断数 = ' + m.params.diagnostics.length);
    m.params.diagnostics.forEach(function (d) {
      console.log('   [' + d.code + '] line=' + d.range.start.line +
                  ' char=' + d.range.start.character +
                  ' sev=' + d.severity + '  ' + d.message);
    });
  }
};

lsp.启动();
lsp.发({ jsonrpc: '2.0', id: 1, method: 'initialize', params: { capabilities: {} } });
lsp.发({ jsonrpc: '2.0', method: 'initialized', params: {} });
lsp.发({ jsonrpc: '2.0', method: 'textDocument/didSave',
         params: { textDocument: { uri: uri } } });

setTimeout(function () {
  lsp.发({ jsonrpc: '2.0', id: 2, method: 'shutdown' });
  lsp.发({ jsonrpc: '2.0', method: 'exit' });
  setTimeout(function () {
    const 有初始化 = 收到.some(function (m) { return m.id === 1 && m.result && m.result.capabilities; });
    const 诊针 = 收到.find(function (m) { return m.method === 'textDocument/publishDiagnostics'; });
    const 有诊断 = !!诊针 && 诊针.params.diagnostics.length > 0;
    console.log('---');
    console.log('收到帧数            = ' + 收到.length);
    console.log('initialize 响应     = ' + 有初始化);
    console.log('publishDiagnostics  = ' + 有诊断 + (诊针 ? '（诊断数 ' + 诊针.params.diagnostics.length + '）' : ''));
    const 通过 = 有初始化 && 有诊断;
    console.log('VERDICT = ' + (通过 ? 'PASS' : 'FAIL'));
    lsp.关();
    process.exit(通过 ? 0 : 1);
  }, 400);
}, 600);
