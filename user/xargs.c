#include "kernel/types.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  char buf[512], *p, *head;

  p = buf;
  while (read(0, p++, 1)) {}

  head = buf;
  for (p = buf; *p != '\0'; p++) {
    if (*p == '\n') {
      *p = '\0';
      char *xarg = head;
      head = p + 1;
      if (fork() == 0) {
        char *xargv[argc + 1];
        for (int i = 1; i < argc; i++) {
          xargv[i-1] = argv[i];
        }
        xargv[argc - 1] = xarg;
        xargv[argc] = 0;
        exec(xargv[0], xargv);
      } else {
        wait(0);
      }
    }
  }
  exit(0);
}
