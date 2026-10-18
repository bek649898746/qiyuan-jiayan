/* 真缺陷探针 · kwset 声明符 const 位置（2026-10-17 挂账）
   真实载体: git `kwset.c:624` `struct trie * const *next;` + `:641 next = kwset->next;`
   语义: `next` 是「指向 (const 指针 to struct trie) 的指针」——**指针本身可写**。
   期望（修好后）: rc=0；当前: rc=1 A9006「next 是 const 对象」。 */
struct trie { int x; };

void probe_kwset(struct trie * const *next, struct trie **src)
{
    next = src;
}
