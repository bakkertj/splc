/* splrt.c - runtime support for programs compiled by splc.
 *
 * Characters are identified by small integers assigned by the compiler.
 * The stage (who is present) is tracked at run time because gotos make it
 * impossible to know statically.  Every character has a value and a stack.
 */
#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef int64_t spl_int;

typedef struct {
  spl_int value;
  spl_int *stack;
  size_t sp, cap;
  int onStage;
} Character;

static Character *chars;
static const char **charNames;
static int nchars;

static void die(const char *msg, const char *who) {
  if (who) fprintf(stderr, "spl: runtime error: %s (%s)\n", msg, who);
  else fprintf(stderr, "spl: runtime error: %s\n", msg);
  exit(1);
}

void spl_init(int n, const char **names) {
  nchars = n;
  charNames = names;
  chars = calloc((size_t)n, sizeof(Character));
  if (!chars) die("out of memory", NULL);
}

void spl_enter(int id) {
  if (chars[id].onStage) die("is already on stage", charNames[id]);
  chars[id].onStage = 1;
}

void spl_exit(int id) {
  if (!chars[id].onStage) die("is not on stage and cannot exit", charNames[id]);
  chars[id].onStage = 0;
}

void spl_exeunt_all(void) {
  for (int i = 0; i < nchars; ++i) chars[i].onStage = 0;
}

/* The speaker must be on stage; the addressee is the one other character present. */
int spl_addressee(int speaker) {
  if (!chars[speaker].onStage) die("speaks but is not on stage", charNames[speaker]);
  int found = -1;
  for (int i = 0; i < nchars; ++i) {
    if (i == speaker || !chars[i].onStage) continue;
    if (found >= 0) die("more than two characters are on stage; it is unclear who is addressed", charNames[speaker]);
    found = i;
  }
  if (found < 0) die("speaks to an empty stage", charNames[speaker]);
  return found;
}

spl_int spl_get(int id) { return chars[id].value; }
void spl_set(int id, spl_int v) { chars[id].value = v; }

void spl_push(int id, spl_int v) {
  Character *c = &chars[id];
  if (c->sp == c->cap) {
    c->cap = c->cap ? c->cap * 2 : 16;
    c->stack = realloc(c->stack, c->cap * sizeof(spl_int));
    if (!c->stack) die("out of memory", NULL);
  }
  c->stack[c->sp++] = v;
}

void spl_pop(int id) {
  Character *c = &chars[id];
  if (c->sp == 0) die("tries to recall a memory they never had (stack underflow)", charNames[id]);
  c->value = c->stack[--c->sp];
}

void spl_out_char(spl_int v) { putchar((int)v); fflush(stdout); }
void spl_out_int(spl_int v) { printf("%" PRId64, v); fflush(stdout); }
spl_int spl_in_char(void) { int c = getchar(); return c == EOF ? -1 : c; }
spl_int spl_in_int(void) {
  spl_int v;
  if (scanf("%" SCNd64, &v) != 1) die("expected a number on input", NULL);
  int c;
  while ((c = getchar()) != EOF && c != '\n') {}
  return v;
}

spl_int spl_div(spl_int a, spl_int b) { if (b == 0) die("division by zero", NULL); return a / b; }
spl_int spl_mod(spl_int a, spl_int b) { if (b == 0) die("division by zero", NULL); return a % b; }
spl_int spl_sqrt(spl_int a) { if (a < 0) die("square root of a negative number", NULL); return (spl_int)floor(sqrt((double)a)); }
spl_int spl_factorial(spl_int a) {
  if (a < 0) die("factorial of a negative number", NULL);
  spl_int r = 1;
  for (spl_int i = 2; i <= a; ++i) r *= i;
  return r;
}
