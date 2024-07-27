#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TOK_LEN 16
#define LBL_NUM 64

/*
 * INSTRUCTIONS
 *
 * =xNN    x = NN
 * +xNN    x += NN
 * :oxy    x o= y
 * ?oxy    if x o y then
 * ?oxN    if x o N then
 * gNNN    goto NNN
 * px      print x
 * .       halt
 *
 */

FILE *f;
char t[TOK_LEN];

char lbl_n = 0;
char lbl[LBL_NUM][TOK_LEN];
char off[LBL_NUM];

char mem[(1 << 12)];
char p = 0;

char hex(int n) {
  if (n > 0xf) exit(1);
  return n + ((0 <= n && n <= 9) ? 0x30 : 0x57);
}

void write(char a, char b, char c, char d) {
  mem[p++] = a;
  mem[p++] = b;
  mem[p++] = c;
  mem[p++] = d;
}

void write2(char a, char b) {
  mem[p++] = a;
  mem[p++] = b;
}

void writeNN(char a, char b, int n) {
  mem[p++] = a;
  mem[p++] = b;
  mem[p++] = hex((n & 0xf0) >> 4);
  mem[p++] = hex((n & 0xf));
}

void writeNNN(char a, int n) {
  mem[p++] = a;
  mem[p++] = hex((n & 0xf00) >> 8);
  mem[p++] = hex((n & 0xf0) >> 4);
  mem[p++] = hex((n & 0xf));
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
    write(':', op, reg, from_reg);
    return;
  }

  if (!(op == '=' || op == '+')) exit(1);
  if (isnum(&num) != 0) exit(1);

  writeNN(op, reg, num);
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
    write('?', cmp, reg, other_reg);
  } else {
    if (isnum(&num) != 0) exit(1);
    if (num > 0xf) exit(1);

    write('?', cmp, reg, hex(num));
  }

  next();
  if (strcmp(t, "then") != 0) exit(1);
  return;
}

void parse_print() {
  char reg;

  next();
  if (!(reg = isreg())) exit(1);

  write2('p', reg);
  return;
}

void parse_label() {
  next();
  if (lbl_n == LBL_NUM) exit(1);

  for (int i = 0; i < lbl_n; i++) {
    if (strcmp(t, lbl[i]) == 0) {
      off[i] = p;
      return;
    }
  }
  strcpy(lbl[lbl_n], t);
  off[lbl_n] = p;
  lbl_n++;
}

void parse_goto() {
  next();
  if (lbl_n == LBL_NUM) exit(1);

  for (int i = 0; i < lbl_n; i++) {
    if (strcmp(t, lbl[i]) == 0) {
      writeNNN('g', i);
      return;
    }
  }
  strcpy(lbl[lbl_n], t);
  off[lbl_n] = 0;
  writeNNN('g', lbl_n);
  lbl_n++;
}

void resolve_gotos() {
  char buf[3];
  int n;

  for (int i = 0; i < p; i += 2) {
    if (mem[i] == 'g') {
      buf[0] = mem[i + 1];
      buf[1] = mem[i + 2];
      buf[2] = mem[i + 3];
      n = off[atoi(buf)];
      mem[i + 1] = hex((n & 0xf00) >> 8);
      mem[i + 2] = hex((n & 0xf0) >> 4);
      mem[i + 3] = hex((n & 0xf));
    }
  }
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
  write2('.', ' ');

  resolve_gotos();

  fclose(f);
}

int main(void) {
  read("game.baya");

  for (int i; i < p; i++) {
    printf("%c", mem[i]);
  }
  putchar('\n');

  return 0;
}