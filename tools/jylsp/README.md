# jylsp · 甲言语言服务器（LSP）

`jylsp.jy`（41.2 KB · 甲言源码）—— 从 **stdin 读 LSP 帧，往 stdout 写 LSP 帧**的独立语言服务器。
配套 VSCode 扩展在 [`../vscode-jiayan/`](../vscode-jiayan/)。

## 编译

**必须在它自己的目录里编**（源码用相对包含 `jyjson.jy` 与 `jy_*.jy` 模块）：

```powershell
cd tools\jylsp
..\..\qcc_zh_seed.exe jylsp.jy -o jylsp.exe
# 实测（2026-10-28 · 现役种子）：rc=0 · 433,661 bytes code
```

## 实测握手（2026-10-28 · 独立复核）

发 `initialize` → `shutdown` → `exit` 三帧（标准 LSP 分帧：`Content-Length: N\r\n\r\n<json>`），
进程 `rc=0`，stdout 305 B，stderr 0 B。**服务器返回原文**：

```json
{"jsonrpc":"2.0","id":1,"result":{"capabilities":{
  "textDocumentSync":1,
  "completionProvider":{"triggerCharacters":[".","("]},
  "definitionProvider":true,
  "hoverProvider":false
},"serverInfo":{"name":"jylsp","version":"0.4.0"}}}

{"jsonrpc":"2.0","id":2,"result":null}
```

### 因此「能做什么」以 capability 为准

| capability | 值 | 含义 |
|:--|:--|:--|
| `textDocumentSync` | **1** | 全文同步（不是增量同步） |
| `completionProvider` | `triggerCharacters: [".", "("]` | **补全**可用，`.` 与 `(` 触发 |
| `definitionProvider` | **true** | **跳转**可用（含跨文件 / 中文路径，见下） |
| `hoverProvider` | **false** | **悬停提示不可用** |

下游工具**不要**假设 `hover` 有实现——它明确声明为 `false`。

## 三项平台级事实（都是实测撞出来的）

**① `serverInfo.version` 报 `0.4.0`，但功能不止 v0.4。**
开发现场 `audit\lsp\` 里有 `jylsp_v0.8_动态直调版_留底.jy` / `v0.9_加URL编解码前_留底.jy` 等留底，
而源码里的版本串没跟着更新。**以 capability 实测为准，不要以 version 字段判断能力。**

**② 甲言产物不跑 CRT 标准流初始化** ⟹ `stdin` / `stdout` / `stderr` 三个 `FILE*` **为 NULL**。
LSP 服务器本该用 `fread`/`fwrite` 读写标准流，这就成了必须自己绕的一道坎（改走 `GetStdHandle` 一类）。
**这是「只有真去写 LSP 才会撞上」的坑**，写别的命令行工具不会遇到。

**③ 中文路径跳转是被专门验证过的能力。**
`definitionProvider` 背后的实现处理了中文文件名/路径；一般 LSP 实现在这里会因编码假设而失手。

## 分工

- **本目录**：协议层（分帧、JSON、capability、请求分发）
- `jyjson.jy`：JSON 解析库（`jypkg` 也共用同一份）
- `jy_check.jy` / `jy_diag.jy` / `jy_fam.jy` / `jy_kw.jy` / `jy_stat.jy`：分析模块
- `../vscode-jiayan/`：客户端（`lspclient.js` 负责拉起本进程并转发）

## 已知限制

- `hover` 未实现（capability 已声明 `false`）
- 全文同步（`textDocumentSync: 1`），大文件会有整份重传成本
- 协议支持面只到上面 4 个 capability；`references` / `rename` / `documentSymbol` 等均未声明

_整理：启元 · 审计岗 · 2026-10-28 · capability 与握手均来自实测原文，未采信自述_
