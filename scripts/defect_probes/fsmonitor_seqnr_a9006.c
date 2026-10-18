/* 真缺陷探针 · fsmonitor A9006（2026-10-17 挂账）
   真实载体: git `builtin/fsmonitor--daemon.c:636` `*seq_nr = 0;`（形参 `uint64_t *seq_nr`，非 const）
   期望（修好后）: rc=0；当前: rc=1 A9006「所指对象是 const」。 */
static int fsmonitor_parse_client_token(const char *buf_token,
                                        unsigned long long *seq_nr)
{
    const char *p;
    char *p_end;
    if (!buf_token)
        return -1;
    *seq_nr = 0;
    (void)p;
    (void)p_end;
    return 0;
}
