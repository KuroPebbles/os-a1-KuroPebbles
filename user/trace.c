// trace <mask> <cmd> [args...]: turn on syscall tracing, then exec cmd.

#include "kernel/types.h"
#include "user/user.h"

static int
alldigits(const char *s)
{
  if (*s == 0)
    return 0;
  for (; *s; s++)
    if (*s < '0' || *s > '9')
      return 0;
  return 1;
}

int
main(int argc, char *argv[])
{
  if (argc < 3 || !alldigits(argv[1])) {
    fprintf(2, "usage: trace <mask> <cmd> [args...]\n");
    exit(1);
  }
  if (trace(atoi(argv[1])) < 0) {
    fprintf(2, "trace: trace failed\n");
    exit(1);
  }
  exec(argv[2], argv + 2);
  fprintf(2, "trace: exec %s failed\n", argv[2]);
  exit(1);
}
