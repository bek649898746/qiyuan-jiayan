#include <stdio.h>
typedef struct sqlite3 sqlite3;
typedef int (*sqlite3_callback)(void*, int, char**, char**);
int sqlite3_open(const char*, sqlite3**);
int sqlite3_exec(sqlite3*, const char*, sqlite3_callback, void*, char**);
const char *sqlite3_errmsg(sqlite3*);
int sqlite3_close(sqlite3*);

int main(void) {
    sqlite3 *db = 0;
    char *err = 0;
    int rc;
    rc = sqlite3_open(":memory:", &db);
    if (rc != 0) { printf("open failed rc=%d\n", rc); return 1; }
    rc = sqlite3_exec(db, "CREATE TABLE t(a)", 0, 0, &err);
    if (rc != 0) { printf("create failed rc=%d\n", rc); return 2; }
    rc = sqlite3_exec(db, "INSERT INTO t VALUES(42)", 0, 0, &err);
    if (rc != 0) { printf("insert failed rc=%d\n", rc); return 3; }
    rc = sqlite3_exec(db, "INSERT INTO t VALUES(7)", 0, 0, &err);
    if (rc != 0) { printf("insert2 failed rc=%d\n", rc); return 4; }
    sqlite3_close(db);
    printf("sqlite3 OK\n");
    return 0;
}
