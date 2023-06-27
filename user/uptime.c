#include "kernel/types.h"
#include "user/user.h"

int
main(int argc,  char *argv[])
{
  int ticks;
  while (1) {
    ticks = uptime();
    if (ticks > 0) break;
  }
  printf("ticks: %d\n", ticks);
  exit(0);
}
