#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TOKEN_LENGTH 16
#define LABEL_MAX 64

FILE *file;
char token[TOKEN_LENGTH];
int line = 1;

char label_n = 0;
char label[LABEL_MAX][TOKEN_LENGTH];
char label_offset[LABEL_MAX];

char memory[(1 << 12)];
char pc = 0;

enum REGISTER { RX = 1, RY, RZ, RW };

enum INSTRUCTION {
  HALT = 1,
  GOTO,           // goto NNN
  PRINT,          // print x
  REG_OP_REG,     // x o= y
  REG_SET_LIT,    // x = NN
  REG_ADD_LIT,    // x += NN
  IF_REG_CMP_REG, // if x c y then
};

enum OPERATION {
  SET = 1, // =
  ADD,     // +=
  SUB,     // -=
  MUL,     // *=
  DIV,     // /=
  MOD      // %=
};

enum COMPARISON {
  EQ = 1, // ==
  NE      // !=
};

char hex(int n) { return n + ((n <= 9) ? '0' : 'A' - 10); }

int ord(char c) { return (c <= '9') ? c - '0' : (c & 0x7) + 9; }

// void write(char a, char b, char c, char d) {
//   memory[pc++] = a;
//   memory[pc++] = b;
//   memory[pc++] = c;
//   memory[pc++] = d;
// }

// void writeNN(char a, char b, int n) {
//   memory[pc++] = a;
//   memory[pc++] = b;
//   memory[pc++] = hex((n & 0xf0) >> 4);
//   memory[pc++] = hex((n & 0xf));
// }

// void writeNNN(char a, int n) {
//   memory[pc++] = a;
//   memory[pc++] = hex((n & 0xf00) >> 8);
//   memory[pc++] = hex((n & 0xf0) >> 4);
//   memory[pc++] = hex((n & 0xf));
// }

void encode_halt() {
  memory[pc++] = HALT;
  pc += 3;
}

void encode_goto(int n) {
  memory[pc++] = GOTO;
  memory[pc++] = (n & 0xf00) >> 8;
  memory[pc++] = (n & 0xf0) >> 4;
  memory[pc++] = (n & 0xf);
}

void encode_print(enum REGISTER r) {
  memory[pc++] = PRINT;
  memory[pc++] = r;
  pc += 2;
}

void encode_reg_op_reg(enum OPERATION op, enum REGISTER x, enum REGISTER y) {
  memory[pc++] = REG_OP_REG;
  memory[pc++] = op;
  memory[pc++] = x;
  memory[pc++] = y;
}

void encode_reg_set_lit(enum REGISTER r, int n) {
  memory[pc++] = REG_SET_LIT;
  memory[pc++] = r;
  memory[pc++] = (n & 0xf0) >> 4;
  memory[pc++] = (n & 0xf);
}

void encode_reg_add_lit(enum REGISTER r, int n) {
  memory[pc++] = REG_ADD_LIT;
  memory[pc++] = r;
  memory[pc++] = (n & 0xf0) >> 4;
  memory[pc++] = (n & 0xf);
}

void encode_if_reg_cmp_reg(enum COMPARISON cmp, enum REGISTER x,
                           enum REGISTER y) {
  memory[pc++] = IF_REG_CMP_REG;
  memory[pc++] = cmp;
  memory[pc++] = x;
  memory[pc++] = y;
}

char is_number(char *n) {
  *n = (char)strtol(token, NULL, 0);
  return errno;
}

void error(const char *msg) {
  printf("ERROR AROUND LINE %d: %s\n", line, msg);
  exit(1);
}

enum REGISTER is_register() {
  if (strcmp(token, "x") == 0) return RX;
  if (strcmp(token, "y") == 0) return RY;
  if (strcmp(token, "z") == 0) return RZ;
  if (strcmp(token, "w") == 0) return RW;
  return 0;
}

enum OPERATION is_operator() {
  if (strcmp(token, "=") == 0) return SET;
  if (strcmp(token, "+=") == 0) return ADD;
  if (strcmp(token, "-=") == 0) return SUB;
  if (strcmp(token, "*=") == 0) return MUL;
  if (strcmp(token, "/=") == 0) return DIV;
  if (strcmp(token, "%=") == 0) return MOD;
  return 0;
}

enum COMPARISON is_compare() {
  if (strcmp(token, "==") == 0) return EQ;
  if (strcmp(token, "!=") == 0) return NE;
  return 0;
}

char *scan_token() {
  char c;
  char i = 0;
  bool comment = false;

  while ((c = fgetc(file)) != EOF) {
    if (c == '\n') line++;

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
  error("missing token");
}

void parse_assign() {
  enum REGISTER reg_to;
  enum REGISTER reg_from;
  enum OPERATION op;
  char num;

  reg_to = is_register();
  next_token();
  if (!(op = is_operator())) error("expected operator after register");

  next_token();
  if ((reg_from = is_register())) {
    encode_reg_op_reg(op, reg_to, reg_from);
    return;
  }

  if (!(op == EQ || op == NE))
    error("unexpected operation between register and literal");
  if (is_number(&num) != 0) error("invalid number");

  if (op == EQ)
    encode_reg_set_lit(reg_to, num);
  else
    encode_reg_add_lit(reg_to, num);
  return;
}

void parse_if() {
  enum REGISTER reg_lhs;
  enum REGISTER reg_rhs;
  enum COMPARISON cmp;
  char num;

  next_token();
  if (!(reg_lhs = is_register())) error("expected register in condition");

  next_token();
  if (!(cmp = is_compare())) error("expected comparison");

  next_token();
  if ((reg_rhs = is_register())) {
    encode_if_reg_cmp_reg(cmp, reg_lhs, reg_rhs);
  } else {
    error("<< TODO >>");
  }

  next_token();
  if (strcmp(token, "then") != 0) error("expected \"then\"");
  return;
}

void parse_print() {
  enum REGISTER reg;

  next_token();
  if (!(reg = is_register())) error("expected register to print");

  encode_print(reg);
  return;
}

void parse_label() {
  next_token();
  if (label_n == LABEL_MAX) error("too many labels");

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
  if (label_n == LABEL_MAX) error("too many labels");

  for (int i = 0; i < label_n; i++) {
    if (strcmp(token, label[i]) == 0) {
      encode_goto(i);
      return;
    }
  }

  strcpy(label[label_n], token);
  label_offset[label_n] = 0;
  encode_goto(label_n);
  label_n++;
}

void resolve_gotos() {
  char a, b, c;
  int n;

  for (int i = 0; i < pc; i += 4) {
    if (memory[i] == GOTO) {
      a = memory[i + 1];
      b = memory[i + 2];
      c = memory[i + 3];

      n = label_offset[(a << 8) | (b << 4) | c];

      memory[i + 1] = (n & 0xf00) >> 8;
      memory[i + 2] = (n & 0xf0) >> 4;
      memory[i + 3] = (n & 0xf);
    }
  }
}

void read(char *name) {
  file = fopen(name, "r");
  if (file == NULL) error("couldn't open file");

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
      error("invalid instruction");
  }
  encode_halt();

  resolve_gotos();

  fclose(file);
}

int get_register() {
  char c = memory[pc++];
  return c - 1;
}

int getNN() {
  char a = memory[pc++];
  char b = memory[pc++];
  return a * 0x10 + b;
}

int getNNN() {
  char a = memory[pc++];
  char b = memory[pc++];
  char c = memory[pc++];
  return a * 0x100 + b * 0x10 + c;
}

void assign_register_to_register(int *r) {
  char op = memory[pc++];
  int reg_n = get_register();

  switch (op) {
  case SET: r[reg_n] = r[get_register()]; break;
  case ADD: r[reg_n] += r[get_register()]; break;
  case SUB: r[reg_n] -= r[get_register()]; break;
  case MUL: r[reg_n] *= r[get_register()]; break;
  case DIV: r[reg_n] /= r[get_register()]; break;
  case MOD: r[reg_n] %= r[get_register()]; break;
  }
}

void if_false_skip_next_instruction(int *r) {
  char cmp = memory[pc++];
  int a = r[get_register()];
  int b = r[get_register()];

  switch (cmp) {
  case EQ:
    if (!(a == b)) pc += 4;
    break;
  case NE:
    if (!(a != b)) pc += 4;
    break;
  }
}

void exec() {
  char o;
  int r[4];
  int reg_n;
  pc = 0;

  while ((o = memory[pc++]) != HALT) {
    switch (o) {
    case REG_SET_LIT:
      reg_n = get_register();
      r[reg_n] = getNN();
      break;
    case REG_ADD_LIT:
      reg_n = get_register();
      r[reg_n] += getNN();
      break;
    case REG_OP_REG: assign_register_to_register(r); break;
    case IF_REG_CMP_REG: if_false_skip_next_instruction(r); break;
    case GOTO: pc = getNNN(); break;
    case PRINT: printf("%d\n", r[get_register()]); break;
    }
  }
}

int main(void) {
  read("game.baya");

  for (int i; i < pc; i += 4) {
    printf("%x", memory[i]);
    printf("%x", memory[i + 1]);
    printf("%x", memory[i + 2]);
    printf("%x ", memory[i + 3]);
  }
  putchar('\n');

  exec();

  return 0;
}