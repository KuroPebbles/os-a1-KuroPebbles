// find <dir> <pattern> [-type f|d]
//
// Recursively walks <dir> and prints the path of every entry whose name
// matches <pattern>. '*' in the pattern matches any run of characters
// (including none); every other character must match exactly.

#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fs.h"
#include "kernel/fcntl.h"
#include "user/user.h"

#define WANT_ANY  0
#define WANT_FILE 1 // -type f: T_FILE and T_DEVICE
#define WANT_DIR  2 // -type d: T_DIR

// One shared path buffer for the whole walk. The user stack is a single
// 4 KB page, so per-frame buffers would cap the depth of trees we can walk.
static char path[512];
static char *pattern;
static int want;

static void
usage(void)
{
  fprintf(2, "usage: find <dir> <pattern> [-type f|d]\n");
  exit(1);
}

// Returns 1 if name matches pat in full.
int
match(const char *pat, const char *name)
{
  if (*pat == 0)
    return *name == 0;
  if (*pat == '*')
    // '*' matches empty (skip it) or eats one char and stays put.
    return match(pat + 1, name) || (*name && match(pat, name + 1));
  return *name && *pat == *name && match(pat + 1, name + 1);
}

static int
typeok(short type)
{
  if (want == WANT_ANY)
    return 1;
  if (want == WANT_DIR)
    return type == T_DIR;
  return type == T_FILE || type == T_DEVICE;
}

// One open directory level of the walk: its entry names and where we are.
struct level {
  char *names; // n names, DIRSIZ+1 bytes each
  int n;       // number of names
  int next;    // index of the next name to visit
  int len;     // strlen of this directory's path in path[]
};

// Reads every entry name of the current working directory into a new level.
// The fd is closed before returning, so the walk never holds more than one
// open fd (NOFILE is only 16). Returns 0 if the directory can't be read.
static int
readlevel(struct level *lv, int len)
{
  static struct dirent de;
  static struct stat st;
  int fd;

  if ((fd = open(".", O_RDONLY)) < 0) {
    fprintf(2, "find: cannot open %s\n", path);
    return 0;
  }
  if (fstat(fd, &st) < 0 || st.type != T_DIR) {
    close(fd);
    return 0;
  }
  // The directory's size bounds how many entries it can hold.
  lv->names = malloc((st.size / sizeof(de)) * (DIRSIZ + 1) + 1);
  if (lv->names == 0) {
    fprintf(2, "find: out of memory\n");
    close(fd);
    return 0;
  }
  lv->n = 0;
  lv->next = 0;
  lv->len = len;
  while (read(fd, &de, sizeof(de)) == sizeof(de)) {
    if (de.inum == 0)
      continue;
    char *dst = lv->names + lv->n * (DIRSIZ + 1);
    memmove(dst, de.name, DIRSIZ);
    dst[DIRSIZ] = 0; // a name may fill all DIRSIZ bytes with no NUL
    lv->n++;
  }
  close(fd);
  return 1;
}

// Walks the tree rooted at path[0..len); the cwd must already be that dir.
//  - Iterative, with the stack of levels on the heap: the user stack is a
//    single 4 KB page, and a recursive walk overflows it on deep trees.
//  - Moves through the tree with chdir() and only ever hands the kernel a
//    single entry name, so trees deeper than MAXPATH (128 bytes) still work.
//    path[] is only used for printing.
static void
walk(int len)
{
  static struct stat st;
  struct level *stack;
  int depth = 0, cap = 16;

  stack = malloc(cap * sizeof(struct level));
  if (stack == 0) {
    fprintf(2, "find: out of memory\n");
    return;
  }
  if (!readlevel(&stack[0], len)) {
    free(stack);
    return;
  }
  depth = 1;

  while (depth > 0) {
    struct level *lv = &stack[depth - 1];
    path[lv->len] = 0; // back to this level's own path

    if (lv->next == lv->n) { // level finished: pop it
      free(lv->names);
      depth--;
      if (depth > 0 && chdir("..") < 0) {
        fprintf(2, "find: cannot leave %s\n", path);
        break;
      }
      continue;
    }
    char *name = lv->names + lv->next++ * (DIRSIZ + 1);
    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
      continue;

    int nlen = strlen(name);
    int at = lv->len;
    if (at + 1 + nlen + 1 > sizeof(path)) {
      fprintf(2, "find: path too long\n");
      continue;
    }
    // Avoid a double slash when <dir> already ends in '/' (e.g. "/").
    if (at == 0 || path[at - 1] != '/')
      path[at++] = '/';
    memmove(path + at, name, nlen);
    path[at + nlen] = 0;

    if (stat(name, &st) < 0) {
      fprintf(2, "find: cannot stat %s\n", path);
      continue;
    }
    if (match(pattern, name) && typeok(st.type))
      printf("%s\n", path);
    if (st.type != T_DIR)
      continue;

    if (depth == cap) { // grow the level stack
      struct level *bigger = malloc(2 * cap * sizeof(struct level));
      if (bigger == 0) {
        fprintf(2, "find: out of memory\n");
        continue;
      }
      memmove(bigger, stack, cap * sizeof(struct level));
      free(stack);
      stack = bigger;
      cap *= 2;
    }
    if (chdir(name) < 0) {
      fprintf(2, "find: cannot open %s\n", path);
      continue;
    }
    if (readlevel(&stack[depth], at + nlen))
      depth++;
    else
      chdir("..");
  }
  free(stack);
}

int
main(int argc, char *argv[])
{
  int fd, len;

  if (argc == 5 && strcmp(argv[3], "-type") == 0) {
    if (strcmp(argv[4], "f") == 0)
      want = WANT_FILE;
    else if (strcmp(argv[4], "d") == 0)
      want = WANT_DIR;
    else
      usage();
  } else if (argc != 3) {
    usage();
  }

  len = strlen(argv[1]);
  if (len + 1 > sizeof(path))
    usage();
  // Check up front so an unopenable <dir> gives the required error + exit 1.
  if ((fd = open(argv[1], O_RDONLY)) < 0) {
    fprintf(2, "find: cannot open %s\n", argv[1]);
    exit(1);
  }
  close(fd);
  // <dir> is never printed itself; if it's a file there is nothing to walk.
  if (chdir(argv[1]) < 0)
    exit(0);

  strcpy(path, argv[1]);
  pattern = argv[2];
  walk(len);
  exit(0);
}
