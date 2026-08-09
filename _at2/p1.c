int printf(const char*, ...);
int f(const char* a) { a++; return *a; }
int fp(const char* a, const char* b) {
  while (*a && *a == *b) { a++; b++; }
  return *a - *b;
}
int main(void) {
  printf("f2=%d\n", f("abc"));
  printf("cmp=%d\n", fp("abc","abd"));
  printf("cmp0=%d\n", fp("abc","abc"));
  return 0;
}
