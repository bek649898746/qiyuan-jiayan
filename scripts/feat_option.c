// @EXPECTED out:PASS
#include <stdio.h>

/* Option: C 可空指针约定 (NULL = 无值) */
static int *find_int(int *arr, int n, int target) {
    for (int i = 0; i < n; i++)
        if (arr[i] == target) return &arr[i];
    return NULL;
}

/* Result: C 错误码约定 (返回码 + 输出参数) */
static int safe_div(int a, int b, int *out) {
    if (b == 0) return -1;   /* 错误 */
    *out = a / b;
    return 0;                /* OK */
}

typedef struct {
    int ok;
    union {
        int val;             /* ok 时的值 */
        const char *err;     /* 错误信息 */
    } u;
} Result;

static Result try_parse(const char *s) {
    Result r;
    if (s[0] == '0') {
        r.ok = 1;
        r.u.val = 0;
    } else if (s[0] >= '1' && s[0] <= '9') {
        r.ok = 1;
        r.u.val = s[0] - '0';
    } else {
        r.ok = 0;
        r.u.err = "not a digit";
    }
    return r;
}

int main(void) {
    int arr[5] = { 10, 20, 30, 40, 50 };
    int *p = find_int(arr, 5, 30);
    if (!p) return 1;
    if (*p != 30) return 2;
    int *n = find_int(arr, 5, 99);
    if (n != NULL) return 3;

    int q;
    if (safe_div(10, 2, &q) != 0 || q != 5) return 4;
    if (safe_div(10, 0, &q) != -1) return 5;

    Result r1 = try_parse("7");
    Result r2 = try_parse("x");
    if (!r1.ok || r1.u.val != 7) return 6;
    if (r2.ok) return 7;

    printf("p=%d q=%d r1=%d r2err=%s\n", *p, q, r1.u.val, r2.u.err);
    printf("PASS\n");
    return 0;
}
