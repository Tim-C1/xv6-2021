#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"

void 
find(char *path, char *fname)
{
  char buf[512], *p;
  int fd;
  struct stat st;
  struct dirent de;

  if ((fd = open(path, 0)) < 0) {
    fprintf(2, "find: cannot open %s\n", path);
    return;
  }

  if (fstat(fd, &st)< 0) {
    fprintf(2, "find: cannot stat %s\n", path);
    close(fd);
    return;
  }

  if (st.type == T_FILE) {
    fprintf(2, "find: the path should a directory not a file\n");
    return;
  } else {
    // dir
    strcpy(buf, path);
    p = buf + strlen(buf);
    *p++ = '/';
    while(read(fd, &de, sizeof(de)) == sizeof(de)) {
      if (de.inum == 0) continue;

      memmove(p, de.name, DIRSIZ);
      p[DIRSIZ] = 0;
      if (stat(buf, &st) < 0) {
        fprintf(2, "find: cannot stat %s\n", buf);
        continue;
      }

      if (st.type == T_FILE) {
        if (strcmp(de.name, fname) == 0) {
          write(1, buf, strlen(buf));
          write(1, "\n", 1);
        }
      } else if (st.type == T_DIR) {
        if (strcmp(de.name, ".") == 0 || strcmp(de.name, "..") == 0) {
          continue;
        } else {
          find(buf, fname);
        }
      } else {
        continue;
      }
    }
  }
}

int
main(int argc, char *argv[])
{
  if (argc != 3) fprintf(2, "find: should provide the dir and the filename to find\n");
  
  find(argv[1], argv[2]);
  exit(0);
}
