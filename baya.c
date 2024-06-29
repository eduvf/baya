#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TOK_LEN 16

FILE *f;
char t[TOK_LEN];

int isnum(long *num) {
  *num = strtol(t, NULL, 0);
  return errno;
}

char isreg() {
  if (strcmp(t, "x") == 0) return 'x';
  if (strcmp(t, "y") == 0) return 'y';
  if (strcmp(t, "z") == 0) return 'z';
  if (strcmp(t, "w") == 0) return 'w';
  return 0;
}

char isop() {
  if (strcmp(t, "=") == 0) return '=';
  if (strcmp(t, "+=") == 0) return '+';
  if (strcmp(t, "-=") == 0) return '-';
  if (strcmp(t, "*=") == 0) return '*';
  if (strcmp(t, "/=") == 0) return '/';
  if (strcmp(t, "%=") == 0) return '%';
  return 0;
}

char iscmp() {
  if (strcmp(t, "==") == 0) return '=';
  if (strcmp(t, "!=") == 0) return '!';
  return 0;
}

char *scan() {
  char c;
  size_t i = 0;
  bool comment = false;

  while ((c = fgetc(f)) != EOF) {
    comment = (comment || c == '(') && c != ')';
    if (comment || c == ')') continue;

    if (isgraph(c) && i < (TOK_LEN - 1)) {
      t[i++] = c;
    } else {
      if (i == 0) continue;
      t[i] = '\0';
      return t;
    }
  }
  return NULL;
}

void next() {
  if (scan() != NULL) return;
  exit(1);
}

void parse_assign() {
  char reg;
  char op;
  long num;
  char from_reg;

  reg = isreg();
  next();
  if (!(op = isop())) exit(1);

  next();
  if ((from_reg = isreg())) {
    printf("r%c op %c with r%c\n", reg, op, from_reg);
    return;
  }

  if (op != '=' && op != '+') exit(1);
  if (isnum(&num) != 0) exit(1);

  printf("r%c op %c with 0x%lx\n", reg, op, num);
  return;
}

void parse_print() {
  char reg;

  next();
  if (!(reg = isreg())) exit(1);

  printf("print value of r%c\n", reg);
  return;
}

void parse_loop() {
  puts("loop start...");
  return;
}

void parse_again() {
  puts("...end loop");
  return;
}

void parse_break() {
  puts("! break out of loop");
  return;
}

void parse_if() {
  char reg;
  char cmp;
  long num;
  char other_reg;

  next();
  if (!(reg = isreg())) exit(1);

  next();
  if (!(cmp = iscmp())) exit(1);

  next();
  if ((other_reg = isreg())) {
    printf("if r%c %c= r%c skip next\n", reg, cmp, other_reg);
  } else {
    if (isnum(&num) != 0) exit(1);
    printf("if r%c %c= 0x%lx skip next\n", reg, cmp, num);
  }

  next();
  if (strcmp(t, "then") != 0) exit(1);
  return;
}

void parse() {
  while (scan() != NULL) {
    if (isreg())
      parse_assign();
    else if (strcmp(t, "print") == 0)
      parse_print();
    else if (strcmp(t, "loop") == 0)
      parse_loop();
    else if (strcmp(t, "again") == 0)
      parse_again();
    else if (strcmp(t, "break") == 0)
      parse_break();
    else if (strcmp(t, "if") == 0)
      parse_if();
  }
}

void read(char *name) {
  f = fopen(name, "r");
  if (f == NULL) exit(1);

  parse();

  fclose(f);
}

int main(void) {
  read("game.baya");

  return 0;
}