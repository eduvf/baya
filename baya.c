#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TOKEN_LENGTH 16
#define LABEL_MAX 64

/*
 * INSTRUCTIONS
 *
 * =xNN    x = NN
 * +xNN    x += NN
 * :oxy    x o= y
 * ?oxy    if x o y then
 * gNNN    goto NNN
 * px      print x
 * .       halt
 *
 */

FILE *file;
char token[TOKEN_LENGTH];

char label_n = 0;
char label[LABEL_MAX][TOKEN_LENGTH];
char label_offset[LABEL_MAX];

char memory[(1 << 12)];
char pc = 0;

char hex(int n) { return n + ((n <= 9) ? '0' : 'A' - 10); }

int ord(char c) { return (c <= '9') ? c - '0' : (c & 0x7) + 9; }

void write(char a, char b, char c, char d) {
  memory[pc++] = a;
  memory[pc++] = b;
  memory[pc++] = c;
  memory[pc++] = d;
}

void writeNN(char a, char b, int n) {
  memory[pc++] = a;
  memory[pc++] = b;
  memory[pc++] = hex((n & 0xf0) >> 4);
  memory[pc++] = hex((n & 0xf));
}

void writeNNN(char a, int n) {
  memory[pc++] = a;
  memory[pc++] = hex((n & 0xf00) >> 8);
  memory[pc++] = hex((n & 0xf0) >> 4);
  memory[pc++] = hex((n & 0xf));
}

char is_number(char *n) {
  *n = (char)strtol(token, NULL, 0);
  return errno;
}

char is_register() {
  if (strcmp(token, "x") == 0) return 'x';
  if (strcmp(token, "y") == 0) return 'y';
  if (strcmp(token, "z") == 0) return 'z';
  if (strcmp(token, "w") == 0) return 'w';
  return 0;
}

char is_operator() {
  if (strcmp(token, "=") == 0) return '=';
  if (strcmp(token, "+=") == 0) return '+';
  if (strcmp(token, "-=") == 0) return '-';
  if (strcmp(token, "*=") == 0) return '*';
  if (strcmp(token, "/=") == 0) return '/';
  if (strcmp(token, "%=") == 0) return '%';
  return 0;
}

char is_compare() {
  if (strcmp(token, "==") == 0) return '=';
  if (strcmp(token, "!=") == 0) return '!';
  return 0;
}

char *scan_token() {
  char c;
  char i = 0;
  bool comment = false;

  while ((c = fgetc(file)) != EOF) {
    comment = comment && c != ')' || c == '(';
    if (comment || c == ')') continue;

    if (isgraph(c) && i < (TOKEN_LENGTH - 1)) {
      token[i++] = c;
      continue;
    }

    if (i == 0) continue;
    token[i] = '\0';
    return token;
  }
  return NULL;
}

void next_token() {
  if (scan_token() != NULL) return;
  exit(1);
}

void parse_assign() {
  char reg_to;
  char reg_from;
  char op;
  char num;

  reg_to = is_register();
  next_token();
  if (!(op = is_operator())) exit(1);

  next_token();
  if ((reg_from = is_register())) {
    write(':', op, reg_to, reg_from);
    return;
  }

  if (!(op == '=' || op == '+')) exit(1);
  if (is_number(&num) != 0) exit(1);

  writeNN(op, reg_to, num);
  return;
}

void parse_if() {
  char reg_lhs;
  char reg_rhs;
  char cmp;
  char num;

  next_token();
  if (!(reg_lhs = is_register())) exit(1);

  next_token();
  if (!(cmp = is_compare())) exit(1);

  next_token();
  if ((reg_rhs = is_register())) {
    write('?', cmp, reg_lhs, reg_rhs);
  } else {
    exit(1);
  }

  /* else {
    if (isnum(&num) != 0) exit(1);
    if (num > 0xf) exit(1);

    write('?', cmp, reg, hex(num));
  } */

  next_token();
  if (strcmp(token, "then") != 0) exit(1);
  return;
}

void parse_print() {
  char reg;

  next_token();
  if (!(reg = is_register())) exit(1);

  write('p', reg, '_', '_');
  return;
}

void parse_label() {
  next_token();
  if (label_n == LABEL_MAX) exit(1);

  for (int i = 0; i < label_n; i++) {
    if (strcmp(token, label[i]) == 0) {
      label_offset[i] = pc;
      return;
    }
  }

  strcpy(label[label_n], token);
  label_offset[label_n] = pc;
  label_n++;
}

void parse_goto() {
  next_token();
  if (label_n == LABEL_MAX) exit(1);

  for (int i = 0; i < label_n; i++) {
    if (strcmp(token, label[i]) == 0) {
      writeNNN('g', i);
      return;
    }
  }

  strcpy(label[label_n], token);
  label_offset[label_n] = 0;
  writeNNN('g', label_n);
  label_n++;
}

void resolve_gotos() {
  char buf[3];
  int n;

  for (int i = 0; i < pc; i += 2) {
    if (memory[i] == 'g') {
      buf[0] = memory[i + 1];
      buf[1] = memory[i + 2];
      buf[2] = memory[i + 3];

      n = label_offset[atoi(buf)];

      memory[i + 1] = hex((n & 0xf00) >> 8);
      memory[i + 2] = hex((n & 0xf0) >> 4);
      memory[i + 3] = hex((n & 0xf));
    }
  }
}

void read(char *name) {
  file = fopen(name, "r");
  if (file == NULL) exit(1);

  while (scan_token() != NULL) {
    if (is_register())
      parse_assign();
    else if (strcmp(token, "print") == 0)
      parse_print();
    else if (strcmp(token, "if") == 0)
      parse_if();
    else if (strcmp(token, ":") == 0)
      parse_label();
    else if (strcmp(token, "goto") == 0)
      parse_goto();
    else
      exit(1);
  }
  write('.', '_', '_', '_');

  resolve_gotos();

  fclose(file);
}

int get_register() {
  char c = memory[pc++];
  switch (c) {
  case 'x': return 0;
  case 'y': return 1;
  case 'z': return 2;
  case 'w': return 3;
  }
}

int getNN() {
  char a = memory[pc++];
  char b = memory[pc++];
  return ord(a) * 0x10 + ord(b);
}

int getNNN() {
  char a = memory[pc++];
  char b = memory[pc++];
  char c = memory[pc++];
  return ord(a) * 0x100 + ord(b) * 0x10 + ord(c);
}

void assign_register_to_register(int *r) {
  char op = memory[pc++];
  int reg_n = get_register();

  switch (op) {
  case '+': r[reg_n] += r[get_register()]; break;
  case '-': r[reg_n] -= r[get_register()]; break;
  case '*': r[reg_n] *= r[get_register()]; break;
  case '/': r[reg_n] /= r[get_register()]; break;
  case '%': r[reg_n] %= r[get_register()]; break;
  }
}

void if_false_skip_next_instruction(int *r) {
  char cmp = memory[pc++];
  int a = r[get_register()];
  int b = r[get_register()];

  switch (cmp) {
  case '=':
    if (!(a == b)) pc += 4;
    break;
  case '!':
    if (!(a != b)) pc += 4;
    break;
  }
}

void exec() {
  char o;
  int r[4];
  int reg_n;
  pc = 0;

  while ((o = memory[pc++]) != '.') {
    switch (o) {
    case '=':
      reg_n = get_register();
      r[reg_n] = getNN();
      break;
    case '+':
      reg_n = get_register();
      r[reg_n] += getNN();
      break;
    case ':': assign_register_to_register(r); break;
    case '?': if_false_skip_next_instruction(r); break;
    case 'g': pc = getNNN(); break;
    case 'p': printf("%d\n", r[get_register()]); break;
    }
  }
}

int main(void) {
  read("game.baya");

  for (int i; i < pc; i += 4) {
    printf("%c", memory[i]);
    printf("%c", memory[i + 1]);
    printf("%c", memory[i + 2]);
    printf("%c ", memory[i + 3]);
  }
  putchar('\n');

  exec();

  return 0;
}