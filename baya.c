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
int p = 0;

void write1(char a) {
  mem[p++] = a;
  mem[p++] = ' ';
}

void write2(char a, char b) {
  mem[p++] = a;
  mem[p++] = b;
  mem[p++] = ' ';
}

void write4(char a, char b, char c, char d) {
  mem[p++] = a;
  mem[p++] = b;
  mem[p++] = c;
  mem[p++] = d;
  mem[p++] = ' ';
}

char numhex(uint8_t n) {
  // 0x30: '0', 0x31: '1', ..., 0x61: 'a', ...
  return (0 <= n && n <= 9) ? (n + 0x30) : (n + 0x61);
}

uint8_t hexnum(char c) {
  return ('0' <= c && c <= '9') ? (c - 0x30) : (c - 0x61);
}

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
    // printf("r%c op %c with r%c\n", reg, op, from_reg);
    // printf(":%c%c%c ", op, reg, from_reg);
    write4(':', op, reg, from_reg);
    return;
  }

  if (op != '=' && op != '+') exit(1);
  if (isnum(&num) != 0) exit(1);
  if (num > 0xff) exit(1);

  // printf("r%c op %c with 0x%x\n", reg, op, num);
  // printf("%c%c%02lx ", op, reg, num);
  write4(op, reg, numhex(num >> 4), numhex(num & 0xf));
  return;
}

void parse_print() {
  char reg;

  next();
  if (!(reg = isreg())) exit(1);

  // printf("print value of r%c\n", reg);
  // printf("!%c ", reg);
  write2('!', reg);
  return;
}

void parse_loop() {
  // puts("loop start...");
  // printf("[ ");
  write1('[');
  return;
}

void parse_again() {
  // puts("...end loop");
  // printf("] ");
  write1(']');
  return;
}

void parse_break() {
  // puts("! break out of loop");
  // printf("^ ");
  write1('^');
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
    // printf("if r%c %c= r%c skip next\n", reg, cmp, other_reg);
    // printf("?%c%c%c ", cmp, reg, other_reg);
    write4('?', cmp, reg, other_reg);
  } else {
    if (isnum(&num) != 0) exit(1);
    if (num > 0xf) exit(1);

    // printf("if r%c %c= 0x%x skip next\n", reg, cmp, num);
    // printf("?%c%c%lx ", cmp, reg, num);
    write4('?', cmp, reg, 0x30 + num);
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

  write1('.');
  write1('\0');
  puts(mem);
}

void read(char *name) {
  f = fopen(name, "r");
  if (f == NULL) exit(1);

  parse();

  fclose(f);
}

int regnum(char r) {
  if (r == 'x') return 0;
  if (r == 'y') return 1;
  if (r == 'z') return 2;
  if (r == 'w') return 3;
}

void exec() {
  uint16_t i = 0;
  uint8_t r[4];

  char a, b, c;

  while (1) {
    switch (mem[i++]) {
    case '=':
      a = mem[i++], b = mem[i++], c = mem[i++];
      r[regnum(a)] = (hexnum(b) << 4) | (hexnum(c) & 0xf);
      break;
    case '!':
      printf("%d\n", r[regnum(mem[i++])]);
      break;
    case '.':
      return;
    default:
      break;
    }
  }
}

int main(void) {
  read("game.baya");
  exec();

  return 0;
}