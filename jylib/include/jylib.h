/* ==========================================================================
 * jylib.h — 甲言基础库 (Jiayan Base Library) 总入口
 *
 * 编译方 : 甲言 qcc_x86 (中文 C 方言, 自举编译器)
 * 调用方 : gcc / clang / msvc / 任何 C ABI 兼容工具链
 *
 * 设计约定
 *   1. 零外部依赖 —— 除 C 标准库外不链接任何东西
 *   2. C ABI 兼容 —— 用甲言编译出的 .obj 可直接被 gcc 链接调用 (已实测)
 *   3. 符号前缀 jy_ —— 避免与系统库/第三方库冲突
 *   4. 头文件纯 C89 声明 —— C / C++ 双可用
 *
 * 用法
 *   1) 用甲言编译需要的模块:
 *        qcc_x86 -c src/jylib_sha256.c -o lib/jylib_sha256.obj
 *   2) 你的程序 #include "jylib_sha256.h" 并链接该 .obj
 * ========================================================================== */
#ifndef JYLIB_H
#define JYLIB_H

#define JYLIB_VERSION_MAJOR 1
#define JYLIB_VERSION_MINOR 1
#define JYLIB_VERSION_PATCH 0
#define JYLIB_VERSION_STRING "1.1.0"

/* 构建工具链标识 —— 用于文档与诊断，不参与逻辑 */
#define JYLIB_BUILT_BY "qcc_x86 (甲言)"

/* 已交付模块（源文件在 src/，公开头文件各自单独 include）
 *   jylib_sha256   SHA-256 / HMAC-SHA256 / HKDF     （1.0.0 起）
 *   jylib_result   错误联合 + 显式传播宏             （1.1.0 起）
 */

#ifdef __cplusplus
extern "C" {
#endif

/* 库级自测：调用各模块的 selftest，返回失败总数（0 = 全过）。
   实现见 src/jylib_selftest.c —— 可选模块，不需要可不要。 */
int jy_selftest(void);

#ifdef __cplusplus
}
#endif

#endif /* JYLIB_H */
