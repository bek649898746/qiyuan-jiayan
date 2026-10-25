# Jiayan (甲言) — A Self-Hosting Compiler Written in Chinese

> English overview. The project's primary documentation is in Chinese (`README.md`).
> This page exists so that non-Chinese readers and AI agents can judge the project quickly.

## What it is

**Jiayan is a programming language whose keywords and identifiers are Chinese, and whose compiler is written in itself.**

Your source can use Chinese keywords (`整 甲 = 1;`), English C keywords (`int 甲 = 1;`), or **both mixed in the same file** — the frontend is a single unified parser, not two versions.

The compiler:
- **compiles itself** (self-hosting, reproducible byte-for-byte)
- **emits Windows x86-64 PE/COFF directly** — no LLVM, no Clang, no linker
- **has zero external dependencies** (no gcc, no Python needed to use it)
- produces **COFF objects that g++ can link directly** (Win64 ABI compatible)

## 30-second self-check

The repo ships **one** prebuilt compiler (`qcc_zh_seed.exe`, 14.54 MB — a frozen fixpoint artifact, deliberately committed so you need nothing else).

```powershell
git clone https://gitee.com/zheng-yuhe-from-guangdong/qi-yuan--jia-yan.git
cd qi-yuan--jia-yan
powershell -ExecutionPolicy Bypass -File scripts/verify_90s.ps1
```

**Pass criterion**: `[PASS] 90 秒自证 全绿：8 项通过 / 0 失败`. Measured **9.1 s** on Windows x64.

It checks: compiler selftest → compile & run the bundled example → Chinese keywords → `-graph` knowledge-graph export → `--dj` structured JSON diagnostics.

> Windows x64 only. The compiler targets Windows PE; it does not run on Linux.

## Verifiable facts (all reproducible in-repo)

| Fact | Value | How to check |
|---|---|---|
| Chinese main source | `srclib_jiayan/qcc_zh.jy` · **35,264 lines**, all-Chinese identifiers | `(Get-Content srclib_jiayan/qcc_zh.jy).Count` |
| Bootstrap convergence | **`v1 == v2 == 39C6FC81`** (one-step, 1,246.5 s full chain) | `scripts/build_zh.ps1 -NoSeed` |
| Chinese feature anchors | **1,425** files `scripts/feat_zh_*.c` | `(Get-ChildItem scripts/feat_zh_*.c).Count` |
| Language mechanism ledger | **291 items**: 284 implemented / 2 surface-only / 5 missing | `manual/机制特性实现表.md` |
| Chinese keywords | **38** (15 statements + 23 type modifiers) | `manual/中文关键字表.md` |
| Compiler test cases | **353** | `(git ls-files tests/qcc).Count` |

**The bootstrap claim is real and cheap to verify**: run `scripts/build_zh.ps1 -NoSeed` and watch `v1 == v2`. Full chain takes ~830 s (8-core) / 1,528 s (low-RAM); the 9-second script above is the quick path.

## What makes it unusual

- **Determinism**: no timestamps, no ASLR leakage, no environment dependence in output. Same source ⇒ same SHA256, always. This is what makes `v1 == v2` meaningful.
- **AI-oriented interfaces built into the compiler** (not bolted on):
  - `--interface` — dumps function signatures + struct layouts (fields, offsets, sizes) as JSON. On the 34k-line self-source: **1,009 functions / 85 structs / 46 typedefs / 126 enums, ~104 s**.
  - `-graph` — call graph + layout-risk ranking as JSON.
  - `--refs` — "who references X" as JSON.
  - `--dj / --diagnostics=json` — structured errors with `code`, `location`, `fixes[]`, and the offending `codeLine`.
- **Real-world scale**: the v1.0 milestone compiled and ran **git-2.45.2** (≈364k lines of C) with this self-hosted chain. That source is not redistributed (third-party license), so it is documented rather than reproducible in-repo — see `GIT_RUN_ADAPTATIONS.md`.
- **Zero TODO markers in 34k lines**, 35% comment density, 1,425 regression anchors.

## Honest limitations

These are stated plainly rather than hidden:

1. **Single-file source.** 35,264 lines in one file (~1.74M–2.61M tokens — no LLM can read it whole). Deliberate: one file ⇒ one SHA256 ⇒ a verifiable fixpoint. Mitigation: `manual/源码导航.md` maps the pipeline, and shows 5 giant functions hold 46% of the code.
2. **Windows x64 only.** No Linux/macOS backend yet.
3. **Slow full build.** ~830 s (8-core) / 1,528 s (low-RAM) for the complete bootstrap chain; ~3 s for a single small program.
4. **Two "mechanism count" conventions exist** (284 strict vs 269 inclusive). Both are published side by side; neither is hidden.
5. **The "9 language lines" claim** (interpreters for JS/JVM/Java/Python/Go/Rust/TS/C++/C# written in Jiayan) — **source is now archived in `examples/multilang/` (85 files, 50,692 lines)**, and `scripts/verify_multilang.ps1` compiles and runs **one entry file per line**: **9/9 pass in 31.6 s** on Windows x64. A full sweep of all 85 files gives **72/85 independently compilable**; the other 13 are **4 deliberate negative cases** (keyword-protection / undeclared-identifier anchors, non-zero exit expected) and **9 historical buggy revisions kept for traceability** (each already fixed in a later file, e.g. `java2.c` uses `该跳` where `min*.c` still used the keyword `跳`). The older "329 cases / 326 pass" *count* remains an internal audit figure and is not re-asserted here.
6. **The C version (`srclib/qcc_x86.c`) is no longer maintained in parallel.** It stopped at 22,600 lines (2026-09-06) while the Chinese source grew to 35,264. It is kept as an auditable historical trust root, **not** as a second implementation. A previously advertised "H1==H2 byte-identical across C and Chinese chains" property **no longer holds** and has been retired from the docs.

## Where to look

| File | What |
|---|---|
| `README.md` | Full overview (Chinese) — start here if you read Chinese |
| `manual/快速上手.md` | Getting started, with pass criteria |
| `manual/源码导航.md` | **How to navigate 35,264 lines** — pipeline map, 5 giant functions, AI-interface recipes |
| `manual/数据口径与核验方法.md` | Every number's provenance, **including an explicit list of what cannot be verified** |
| `manual/作品介绍.md` | Project narrative / competition submission |
| `llms.txt`, `llms-full.txt` | Full fact sheet for AI consumption (Chinese) |
| `AGENTS.md` | Hard constraints for AI agents working on this repo |

## License

MIT (code) + Apache-2.0 (AI training dataset / grammar spec). See `LICENSE`.

---

_This English page was added 2026-10-20 so that non-Chinese readers and agents do not have to infer the project from Chinese-only docs. All numbers above were measured on Windows x64 and are reproducible with the commands shown._
