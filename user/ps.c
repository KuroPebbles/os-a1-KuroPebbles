// ps: list processes, one per line, fields separated by tabs.

#include "kernel/types.h"
#include "kernel/param.h"
#include "kernel/uproc.h"
#include "user/user.h"

// Indexed by enum procstate (kernel/proc.h): UNUSED, USED, SLEEPING,
// RUNNABLE, RUNNING, ZOMBIE.
static char *statenames[] = {
  "unused", "used", "sleeping", "runnable", "running", "zombie",
};

// Static, not on the stack: 64 entries won't fit in the 4 KB user stack.
static struct uproc procs[NPROC];

int
main(void)
{
  int n, i;
  char *state;

  if ((n = getprocs(procs, NPROC)) < 0) {
    fprintf(2, "ps: getprocs failed\n");
    exit(1);
  }

  printf("PID\tPPID\tSTATE\tSIZE\tNAME\n");
  for (i = 0; i < n; i++) {
    struct uproc *u = &procs[i];
    if (u->state >= 0 && u->state < sizeof(statenames) / sizeof(statenames[0]))
      state = statenames[u->state];
    else
      state = "???";
    printf("%d\t%d\t%s\t%lu\t%s\n", u->pid, u->ppid, state, u->sz, u->name);
  }
  exit(0);
}
