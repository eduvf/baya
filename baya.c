#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TOK_LEN 16
#define LBL_NUM 64

FILE *f;
char t[TOK_LEN];

char lbl_n = 0;
char lbl[LBL_NUM][TOK_LEN];
char off[LBL_NUM];

char mem[(1 << 12)];
char p = 0;

void write(char a, char b, char c, char d) {
  mem[p++] = a;
  mem[p++] = b;
  mem[p++] = c;
  mem[p++] = d;
}

char hex(int n) {
  if (n > 0xf) exit(1);
  return n + ((0 <= n && n <= 9) ? 0x30 : 0x61);
}

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
    write(':', reg, op, from_reg);
    return;
  }

  if (!(op == '=' || op == '+')) exit(1);
  if (isnum(&num) != 0) exit(1);

  write(op, reg, hex((num & 0xf0) >> 4), hex(num & 0xf));
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
    write('?', reg, cmp, other_reg);
  } else {
    if (isnum(&num) != 0) exit(1);
    if (num > 0xf) exit(1);

    write('?', reg, cmp, hex(num));
  }

  next();
  if (strcmp(t, "then") != 0) exit(1);
  return;
}

void parse_print() {
  char reg;

  next();
  if (!(reg = isreg())) exit(1);

  write('p', reg, '.', '.');
  return;
}

void parse_label() {
  next();
  if (lbl_n == LBL_NUM) exit(1);

  strcpy(lbl[lbl_n], t);
  off[lbl_n++] = p;
}

void parse_goto() {
  next();
  write('g', '.', '.', '.');
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

  for (int i; i < p; i += 4) {
    printf("%c%c%c%c\n", mem[i], mem[i + 1], mem[i + 2], mem[i + 3]);
  }

  puts("----");
  for (int i = 0; i < lbl_n; i++) {
    printf("%s: 0x%x\n", lbl[i], off[i]);
  }

  return 0;
}