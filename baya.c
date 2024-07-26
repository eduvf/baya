#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TOK_LEN 16

FILE *f;
char t[TOK_LEN];
char mem[(1 << 12)];
char ins = 0;

char isnum(char *num) {
  *num = (char)strtol(t, NULL, 0);
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
  char i = 0;
  bool comment = false;

  while ((c = fgetc(f)) != EOF) {
    comment = comment && c != ')' || c == '(';
    if (comment || c == ')') continue;

    if (isgraph(c) && i < (TOK_LEN - 1)) {
      t[i++] = c;
      continue;
    }

    if (i == 0) continue;
    t[i] = '\0';
    return t;
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
  char num;
  char from_reg;

  reg = isreg();
  next();
  if (!(op = isop())) exit(1);

  next();
  if ((from_reg = isreg())) {
    printf("%02x  r%c %c r%c\n", ins++, reg, op, from_reg);
    return;
  }

  if (!(op == '=' || op == '+')) exit(1);
  if (isnum(&num) != 0) exit(1);

  printf("%02x  r%c %c 0x%x\n", ins++, reg, op, num);
  return;
}

void parse_if() {
  char reg;
  char cmp;
  char num;
  char other_reg;

  next();
  if (!(reg = isreg())) exit(1);

  next();
  if (!(cmp = iscmp())) exit(1);

  next();
  if ((other_reg = isreg())) {
    printf("%02x  if r%c %c= r%c then\n", ins++, reg, cmp, other_reg);
  } else {
    if (isnum(&num) != 0) exit(1);
    if (num > 0xf) exit(1);

    printf("%02x  if r%c %c= 0x%x\n", ins++, reg, cmp, num);
  }

  next();
  if (strcmp(t, "then") != 0) exit(1);
  return;
}

void parse_print() {
  char reg;

  next();
  if (!(reg = isreg())) exit(1);

  printf("%02x  print r%c\n", ins++, reg);
  return;
}

void parse_label() {
  next();
  printf("(label %s)\n", t);
}

void parse_goto() {
  next();
  printf("%02x  goto %s\n", ins++, t);
}

void read(char *name) {
  f = fopen(name, "r");
  if (f == NULL) exit(1);

  while (scan() != NULL) {
    if (isreg())
      parse_assign();
    else if (strcmp(t, "print") == 0)
      parse_print();
    else if (strcmp(t, "if") == 0)
      parse_if();
    else if (strcmp(t, ":") == 0)
      parse_label();
    else if (strcmp(t, "goto") == 0)
      parse_goto();
    else
      exit(1);
  }

  fclose(f);
}

int main(void) {
  read("game.baya");

  return 0;
}