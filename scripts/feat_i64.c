#include <stdio.h>
typedef __int64 i64x;
typedef signed __int64 si64x;
typedef unsigned __int64 ui64x;
int main(void) {
    printf("sizeof __int64=%d signed=%d unsigned=%d\n",
        (int)sizeof(i64x), (int)sizeof(si64x), (int)sizeof(ui64x));
    return 0;
}
