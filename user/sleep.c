// sleep <ticks>: pause for the given number of timer ticks, then exit 0.

#include "kernel/types.h"
#include "user/user.h"

static void
usage(void)
{
  fprintf(2, "usage: sleep <ticks>\n");
  exit(1);
}

// Returns 1 if s is a non-empty string made only of digits.
// atoi() does no validation, so this is what rejects "-5", "abc" and "".
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
  if (argc != 2 || !alldigits(argv[1]))
    usage();

  pause(atoi(argv[1]));
  exit(0);
}
