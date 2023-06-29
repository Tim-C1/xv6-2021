#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "syscall.h"
#include "defs.h"

// Fetch the uint64 at addr from the current process.
int
fetchaddr(uint64 addr, uint64 *ip)
{
  struct proc *p = myproc();
  if(addr >= p->sz || addr+sizeof(uint64) > p->sz)
    return -1;
  if(copyin(p->pagetable, (char *)ip, addr, sizeof(*ip)) != 0)
    return -1;
  return 0;
}

// Fetch the nul-terminated string at addr from the current process.
// Returns length of string, not including nul, or -1 for error.
int
fetchstr(uint64 addr, char *buf, int max)
{
  struct proc *p = myproc();
  int err = copyinstr(p->pagetable, buf, addr, max);
  if(err < 0)
    return err;
  return strlen(buf);
}

static uint64
argraw(int n)
{
  struct proc *p = myproc();
  switch (n) {
  case 0:
    return p->trapframe->a0;
  case 1:
    return p->trapframe->a1;
  case 2:
    return p->trapframe->a2;
  case 3:
    return p->trapframe->a3;
  case 4:
    return p->trapframe->a4;
  case 5:
    return p->trapframe->a5;
  }
  panic("argraw");
  return -1;
}

// Fetch the nth 32-bit system call argument.
int
argint(int n, int *ip)
{
  *ip = argraw(n);
  return 0;
}

// Retrieve an argument as a pointer.
// Doesn't check for legality, since
// copyin/copyout will do that.
int
argaddr(int n, uint64 *ip)
{
  *ip = argraw(n);
  return 0;
}

// Fetch the nth word-sized system call argument as a null-terminated string.
// Copies into buf, at most max.
// Returns string length if OK (including nul), -1 if error.
int
argstr(int n, char *buf, int max)
{
  uint64 addr;
  if(argaddr(n, &addr) < 0)
    return -1;
  return fetchstr(addr, buf, max);
}

extern uint64 sys_chdir(void);
extern uint64 sys_close(void);
extern uint64 sys_dup(void);
extern uint64 sys_exec(void);
extern uint64 sys_exit(void);
extern uint64 sys_fork(void);
extern uint64 sys_fstat(void);
extern uint64 sys_getpid(void);
extern uint64 sys_kill(void);
extern uint64 sys_link(void);
extern uint64 sys_mkdir(void);
extern uint64 sys_mknod(void);
extern uint64 sys_open(void);
extern uint64 sys_pipe(void);
extern uint64 sys_read(void);
extern uint64 sys_sbrk(void);
extern uint64 sys_sleep(void);
extern uint64 sys_unlink(void);
extern uint64 sys_wait(void);
extern uint64 sys_write(void);
extern uint64 sys_uptime(void);
extern uint64 sys_trace(void);

static uint64 (*syscalls[])(void) = {
[SYS_fork]    sys_fork,
[SYS_exit]    sys_exit,
[SYS_wait]    sys_wait,
[SYS_pipe]    sys_pipe,
[SYS_read]    sys_read,
[SYS_kill]    sys_kill,
[SYS_exec]    sys_exec,
[SYS_fstat]   sys_fstat,
[SYS_chdir]   sys_chdir,
[SYS_dup]     sys_dup,
[SYS_getpid]  sys_getpid,
[SYS_sbrk]    sys_sbrk,
[SYS_sleep]   sys_sleep,
[SYS_uptime]  sys_uptime,
[SYS_open]    sys_open,
[SYS_write]   sys_write,
[SYS_mknod]   sys_mknod,
[SYS_unlink]  sys_unlink,
[SYS_link]    sys_link,
[SYS_mkdir]   sys_mkdir,
[SYS_close]   sys_close,
[SYS_trace]   sys_trace,
};

static void
print_trace (int syscall_num, int ret_val, int pid)
{
  switch (syscall_num) {
    case (SYS_fork):
      printf("%d: syscall %s -> %d\n", pid, "fork", ret_val);
      break;
    case (SYS_exit):
      printf("%d: syscall %s -> %d\n", pid, "exit", ret_val);
      break;
    case (SYS_wait):
      printf("%d: syscall %s -> %d\n", pid, "wait", ret_val);
      break;
    case (SYS_pipe):
      printf("%d: syscall %s -> %d\n", pid, "pipe", ret_val);
      break;
    case (SYS_read):
      printf("%d: syscall %s -> %d\n", pid, "read", ret_val);
      break;
    case (SYS_kill):
      printf("%d: syscall %s -> %d\n", pid, "kill", ret_val);
      break;
    case (SYS_exec):
      printf("%d: syscall %s -> %d\n", pid, "exec", ret_val);
      break;
    case (SYS_fstat):
      printf("%d: syscall %s -> %d\n", pid, "fstat", ret_val);
      break;
    case (SYS_chdir):
      printf("%d: syscall %s -> %d\n", pid, "chdir", ret_val);
      break;
    case (SYS_dup):
      printf("%d: syscall %s -> %d\n", pid, "dup", ret_val);
      break;
    case (SYS_getpid):
      printf("%d: syscall %s -> %d\n", pid, "getpid", ret_val);
      break;
    case (SYS_sbrk):
      printf("%d: syscall %s -> %d\n", pid, "sbrk", ret_val);
      break;
    case (SYS_sleep):
      printf("%d: syscall %s -> %d\n", pid, "sleep", ret_val);
      break;
    case (SYS_uptime):
      printf("%d: syscall %s -> %d\n", pid, "uptime", ret_val);
      break;
    case (SYS_open):
      printf("%d: syscall %s -> %d\n", pid, "open", ret_val);
      break;
    case (SYS_write):
      printf("%d: syscall %s -> %d\n", pid, "write", ret_val);
      break;
    case (SYS_mknod):
      printf("%d: syscall %s -> %d\n", pid, "mknod", ret_val);
      break;
    case (SYS_unlink):
      printf("%d: syscall %s -> %d\n", pid, "unlink", ret_val);
      break;
    case (SYS_link):
      printf("%d: syscall %s -> %d\n", pid, "link", ret_val);
      break;
    case (SYS_mkdir):
      printf("%d: syscall %s -> %d\n", pid, "mkdir", ret_val);
      break;
    case (SYS_close):
      printf("%d: syscall %s -> %d\n", pid, "close", ret_val);
      break;
    case (SYS_trace):
      printf("%d: syscall %s -> %d\n", pid, "trace", ret_val);
      break;
    default:
      printf("trace: unrecognize syscall: %d\n", syscall_num);
  }
}

void
syscall(void)
{
  int num;
  struct proc *p = myproc();

  num = p->trapframe->a7;
  if(num > 0 && num < NELEM(syscalls) && syscalls[num]) {
      p->trapframe->a0 = syscalls[num]();
      // printf("p->trace_mask: %d, num:%d\n", p->trace_mask, num);
      if (p->trace_mask & 1 << num) {
        print_trace(num, p->trapframe->a0, p->pid);
      }
    } else {
    printf("%d %s: unknown sys call %d\n",
            p->pid, p->name, num);
    p->trapframe->a0 = -1;
  }
}
