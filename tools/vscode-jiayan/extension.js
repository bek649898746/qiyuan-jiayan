/*
 * extension.js — 甲言 VSCode 扩展开口（壳）
 *
 * ⚠️ 诚实分层（大哥的约束是"用甲言写"）：
 *   本文件是 **JS 壳** —— VSCode 扩展**本体只能用 JS/TS**，这是平台的硬性要求。
 *   但**逻辑与数据全部由甲言产出**：
 *     · jylsp.exe  （甲言写）—— LSP 协议（握手 + 诊断）
 *     · jy_diag.exe（甲言写）—— 诊断 JSON → LSP 帧
 *     · jy_kw.exe  （甲言写）—— 535 词关键字表 → 本扩展的语法高亮
 *     · jy_check.exe（甲言写）—— 诊断渲染成中文报告
 *   本壳只做三件事：**启动进程 · 收发字节 · 转 VSCode API** ✓
 */
const vscode = require('vscode');
const cp = require('child_process');
const fs = require('fs');
const path = require('path');
const { 甲言LSP } = require('./lspclient');

let lsp = null;
let 诊断集 = null;
let 序号 = 0;
const 待响应 = new Map();

function 配置(键) {
  const v = vscode.workspace.getConfiguration('jiayan').get(键);
  return v || '';
}
function 找工具(键, 名) {
  const 配 = 配置(键);
  if (配 && fs.existsSync(配)) return 配;
  const 候选 = [path.join(__dirname, 名), path.join(__dirname, '..', 名)];
  for (const c of 候选) { if (fs.existsSync(c)) return c; }
  return null;   // 交给 VSCode 去 PATH 找相对名
}
function 转uri(文件) {
  let p = 文件.replace(/\\/g, '/');
  if (!p.startsWith('/')) p = '/' + p;
  return 'file://' + p;
}
function 严级映射(s) {
  if (s === 1) return vscode.DiagnosticSeverity.Error;
  if (s === 2) return vscode.DiagnosticSeverity.Warning;
  if (s === 3) return vscode.DiagnosticSeverity.Information;
  return vscode.DiagnosticSeverity.Hint;
}

function 收消息(m) {
  if (m.method === 'textDocument/publishDiagnostics') {
    const uri = vscode.Uri.parse(m.params.uri);
    const 列表 = (m.params.diagnostics || []).map(function (d) {
      const r = new vscode.Range(
        d.range.start.line, d.range.start.character,
        d.range.end.line, d.range.end.character);
      const 条 = new vscode.Diagnostic(r, d.message, 严级映射(d.severity));
      条.code = d.code;
      条.source = d.source || 'jyan';
      return 条;
    });
    诊断集.set(uri, 列表);
    return;
  }
  if (m.id && 待响应.has(m.id)) {
    待响应.get(m.id)(m);
    待响应.delete(m.id);
  }
}

function 启服务() {
  const exe = 找工具('jylspPath', 'jylsp.exe') || 'jylsp.exe';
  if (lsp) { lsp.关(); }
  lsp = new 甲言LSP(exe, function (m) { console.log('[甲言] ' + m); });
  lsp.处理器 = 收消息;
  lsp.启动();
  lsp.发({ jsonrpc: '2.0', id: ++序号, method: 'initialize',
           params: { processId: process.pid, rootUri: null, capabilities: {} } });
  lsp.发({ jsonrpc: '2.0', method: 'initialized', params: {} });
  vscode.window.setStatusBarMessage('甲言语言服务已启动', 3000);
}

/* 壳的职责：跑 qcc --dj 写【约定诊断文件】<源文件>.jyan.json ⟹ 再通知 LSP 去读 */
function 产诊断并通知(文档) {
  const qcc = 找工具('qccPath', 'qcc_zh_r799g.exe');
  if (!qcc) {
    vscode.window.showWarningMessage('找不到 qcc 编译器（请在设置里填 jiayan.qccPath）');
    return;
  }
  const 文件 = 文档.uri.fsPath;
  const env = Object.assign({}, process.env);
  const rt = 配置('rtPath');
  if (rt) env.QCC_RT_PATH = rt;
  cp.execFile(qcc, ['--dj', 文件], { env: env, maxBuffer: 1 << 24 }, function (err, stdout) {
    try { fs.writeFileSync(文件 + '.jyan.json', stdout || '', 'utf8'); } catch (e) {}
    if (lsp) {
      lsp.发({ jsonrpc: '2.0', method: 'textDocument/didSave',
               params: { textDocument: { uri: 转uri(文件) } } });
    }
  });
}

function activate(上下文) {
  诊断集 = vscode.languages.createDiagnosticCollection('jiayan');
  上下文.subscriptions.push(诊断集);
  启服务();

  上下文.subscriptions.push(vscode.workspace.onDidSaveTextDocument(function (文档) {
    if (文档.languageId === 'jiayan' || 文档.fileName.endsWith('.jy')) 产诊断并通知(文档);
  }));

  上下文.subscriptions.push(vscode.window.onDidChangeActiveTextEditor(function (编辑器) {
    if (!编辑器) return;
    const t = 编辑器.document;
    if (t.languageId !== 'jiayan' && !t.fileName.endsWith('.jy')) return;
    产诊断并通知(t);
    if (lsp) {
      lsp.发({ jsonrpc: '2.0', method: 'textDocument/didOpen',
               params: { textDocument: { uri: 转uri(t.uri.fsPath), languageId: 'jiayan', version: 1, text: '' } } });
    }
  }));

  上下文.subscriptions.push(vscode.commands.registerCommand('jiayan.compile', function () {
    const e = vscode.window.activeTextEditor;
    if (e) 产诊断并通知(e.document);
  }));

  上下文.subscriptions.push(vscode.commands.registerCommand('jiayan.check', function () {
    const e = vscode.window.activeTextEditor;
    if (!e) return;
    const jc = 找工具('jyCheckPath', 'jy_check.exe');
    const df = e.document.uri.fsPath + '.jyan.json';
    if (!jc) { vscode.window.showWarningMessage('找不到 jy_check.exe'); return; }
    if (!fs.existsSync(df)) { vscode.window.showWarningMessage('先执行「甲言: 编译当前文件」生成诊断'); return; }
    cp.execFile(jc, [df, e.document.uri.fsPath], function (err, out) {
      const ch = vscode.window.createOutputChannel('甲言诊断');
      ch.clear(); ch.append(out || ''); ch.show();
    });
  }));

  上下文.subscriptions.push(vscode.commands.registerCommand('jiayan.restartLsp', function () {
    启服务();
  }));
}

function deactivate() { if (lsp) lsp.关(); }

module.exports = { activate, deactivate };
