int printf(const char*, ...);
int main(void) {
  printf("%08x\n", -1);
  printf("%#X\n", 255);
  printf("%08X\n", 255);
  printf("%05d\n", 42);
  printf("%x\n", 0);
  printf("%08x\n", 0);
  printf("%#x\n", 0);
  return 0;
}
