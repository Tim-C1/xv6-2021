#include "kernel/types.h"
#include "user/user.h"

void
prime(int rd) {
  int p;
  int pp[2];

  if (!read(rd, &p, 4)) {
    return;
  }
  pipe(pp);
  if (fork() == 0) {
    close(pp[1]);
    close(rd);
    prime(pp[0]);
    close(pp[0]);
  } else {
    int n;
    close(pp[0]);
    printf("prime %d\n", p);
    while (read(rd, &n, 4)) {
      if (n % p != 0) {
        write(pp[1], &n, 4);
      }
    }
    close(pp[1]);
  }
  wait(0);
}

int
main(int argc, char *argv[])
{
  int p[2];
  pipe(p);
  
  if (fork() == 0) {
    close(p[1]);
    prime(p[0]);
    close(p[0]);
  } else {
    close(p[0]);
    for (int i = 2; i <= 35; i++) {
      write(p[1], &i, 4);
    }
    close(p[1]);
    wait(0);
  }
  exit(0);
}
