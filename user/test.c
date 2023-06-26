#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main (int argc, char *argv[])
{
  if (fork() == 0) {
    char *argv_s[3];
    argv_s[0] = "echo";
    argv_s[1] = "hello\n";
    argv_s[2] = 0;
    exec("echo", argv_s);
  } else {
    wait(0);
  }
  exit(0);
}

