// xargs [-n N] <cmd> [args...]
//
// Reads whitespace-separated words from stdin and runs <cmd> [args...]
// with those words appended.
//   no -n : one run per non-empty input line
//   -n N  : ignore lines, at most N words per run (last run may be short)
// Runs are sequential. Exit 0 if every run exited 0, otherwise 1.

#include "kernel/types.h"
#include "kernel/param.h"
#include "user/user.h"

#define MAXWORD 512

static char *argvbuf[MAXARG + 1]; // +1 for exec's terminating 0
static int nbase;                 // cmd + fixed args at the front of argvbuf
static int nwords;                // words currently appended after them
static int failed;

static void
usage(void)
{
  fprintf(2, "usage: xargs [-n N] cmd [args...]\n");
  exit(1);
}

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

// Runs the command with the words collected so far, waits for it, then
// frees the words. Does nothing if there are no words.
static void
run(void)
{
  int pid, status, i;

  if (nwords == 0)
    return;
  argvbuf[nbase + nwords] = 0;

  pid = fork();
  if (pid < 0) {
    fprintf(2, "xargs: fork failed\n");
    failed = 1;
  } else if (pid == 0) {
    exec(argvbuf[0], argvbuf);
    fprintf(2, "xargs: exec %s failed\n", argvbuf[0]);
    exit(1);
  } else {
    if (wait(&status) < 0 || status != 0)
      failed = 1;
  }

  for (i = nbase; i < nbase + nwords; i++)
    free(argvbuf[i]);
  nwords = 0;
}

// Appends one finished word; runs early if the arg list is full.
static void
addword(const char *w, int len, int perrun)
{
  char *s = malloc(len + 1);
  if (s == 0) {
    fprintf(2, "xargs: out of memory\n");
    exit(1);
  }
  memmove(s, w, len);
  s[len] = 0;
  argvbuf[nbase + nwords++] = s;

  if (nwords == perrun)
    run();
}

int
main(int argc, char *argv[])
{
  int first = 1, n = 0, perrun, i, r, wlen = 0;
  char buf[512], word[MAXWORD];

  if (argc > 1 && strcmp(argv[1], "-n") == 0) {
    if (argc < 3 || !alldigits(argv[2]) || (n = atoi(argv[2])) < 1)
      usage();
    first = 3;
  }
  if (first >= argc)
    usage();

  nbase = argc - first;
  if (nbase >= MAXARG) {
    fprintf(2, "xargs: too many arguments\n");
    exit(1);
  }
  for (i = 0; i < nbase; i++)
    argvbuf[i] = argv[first + i];

  // Never go past MAXARG total args, even if -n (or a long line) asks for it.
  perrun = MAXARG - nbase;
  if (n > 0 && n < perrun)
    perrun = n;

  while ((r = read(0, buf, sizeof(buf))) > 0) {
    for (i = 0; i < r; i++) {
      char c = buf[i];
      if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
        if (wlen > 0) {
          addword(word, wlen, perrun);
          wlen = 0;
        }
        if (c == '\n' && n == 0)
          run(); // end of line; a blank line has no words, so no run
      } else if (wlen < MAXWORD - 1) {
        word[wlen++] = c;
      }
    }
  }
  if (wlen > 0)
    addword(word, wlen, perrun);
  run(); // last line without '\n', or the short final -n group

  exit(failed ? 1 : 0);
}
