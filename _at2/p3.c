int printf(const char*, ...);
int main(void) {
  printf("[%05d]\n", -42);
  printf("[%05d]\n", 42);
  printf("[%03d]\n", 12345);
  printf("[%x][%#x]\n", 0, 0);
  printf("[%02d]\n", 0);
  printf("[%5d]\n", 42);
  return 0;
}
