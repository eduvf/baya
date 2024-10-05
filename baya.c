#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>

#include "raylib.h"

#define SCREEN_WIDTH 64
#define SCREEN_HEIGHT 32
#define TOKEN_LENGTH 32
#define LABEL_MAX 64

#define PALETTE_SIZE 8

const Color COLOR_BG = {20, 20, 40, 255};
const Color COLOR_MG = {100, 100, 140, 255};
const Color COLOR_FG = {180, 180, 190, 255};
const Color COLOR_ROSE = {200, 50, 50, 255};
const Color COLOR_WOOD = {180, 120, 50, 255};
const Color COLOR_SAND = {250, 200, 50, 255};
const Color COLOR_VINE = {80, 200, 50, 255};
const Color COLOR_WAVE = {50, 150, 200, 255};

Color PALETTE[] = {COLOR_BG,   COLOR_MG,   COLOR_FG,   COLOR_ROSE,
                   COLOR_WOOD, COLOR_SAND, COLOR_VINE, COLOR_WAVE};

uint8_t scale = 6;

FILE *file;
char token[TOKEN_LENGTH];
uint8_t line = 1;

char label[LABEL_MAX][TOKEN_LENGTH];
uint8_t label_n = 0;
uint16_t label_offset[LABEL_MAX];

uint8_t mem[(1 << 12)];
uint16_t pc = 0;

#define REGISTER_N 12
uint8_t regs[REGISTER_N];

/* ENUMS */
/* starting at 1, because 0 represents an invalid token */

typedef enum { RX = 1, RY, RZ, RW, RA, RB, RC, RD, RE, RF, RT, RCOL } reg_t;

typedef enum { KACTION = 1, KUP, KDOWN, KLEFT, KRIGHT } keys_t;

typedef enum {
  HALT = 1,
  GOTO,           // goto NNN
  PRINT,          // print x
  CLEAR,          // clear N
  COLOR,          // color N
  SPRITE,         // sprite NNN
  REG_OP_REG,     // x o= y
  REG_SET_LIT,    // x = NN
  REG_ADD_LIT,    // x += NN
  REG_RANDOM,     // x = random NN
  IF_REG_CMP_REG, // if x c y then
  IF_REG_EQ_LIT,  // if x == NN then
  IF_REG_NE_LIT,  // if x != NN then
  IF_KEY,         // key K then
} ins_t;

typedef enum {
  SET = 1, // =
  ADD,     // +=
  SUB,     // -=
  MUL,     // *=
  DIV,     // /=
  MOD,     // %=
  AND,     // &=
  OR,      // |=
  XOR,     // ^=
} op_t;

typedef enum {
  EQ = 1, // ==
  NE,     // !=
  LT,     // <
  LE,     // <=
  GT,     // >
  GE,     // >=
} cmp_t;

/* ENCODERS */

void encode_write(uint8_t n) {
  mem[pc++] = n;
  return;
}

void encode_halt() {
  mem[pc++] = HALT;
  pc += 3;
}

void encode_goto(uint16_t n) {
  mem[pc++] = GOTO;
  mem[pc++] = (n & 0xf00) >> 8;
  mem[pc++] = (n & 0xf0) >> 4;
  mem[pc++] = (n & 0xf);
}

void encode_print(reg_t r) {
  mem[pc++] = PRINT;
  mem[pc++] = r;
  pc += 2;
}

void encode_clear(uint8_t n) {
  mem[pc++] = CLEAR;
  mem[pc++] = n & (PALETTE_SIZE - 1);
  pc += 2;
}

void encode_color(uint8_t col) {
  mem[pc++] = COLOR;
  mem[pc++] = col & (PALETTE_SIZE - 1);
  pc += 2;
}

void encode_sprite(uint16_t addr) {
  mem[pc++] = SPRITE;
  mem[pc++] = (addr & 0xf00) >> 8;
  mem[pc++] = (addr & 0xf0) >> 4;
  mem[pc++] = (addr & 0xf);
}

void encode_reg_op_reg(op_t op, reg_t x, reg_t y) {
  mem[pc++] = REG_OP_REG;
  mem[pc++] = op;
  mem[pc++] = x;
  mem[pc++] = y;
}

void encode_reg_set_lit(reg_t r, uint8_t n) {
  mem[pc++] = REG_SET_LIT;
  mem[pc++] = r;
  mem[pc++] = (n & 0xf0) >> 4;
  mem[pc++] = (n & 0xf);
}

void encode_reg_add_lit(reg_t r, uint8_t n) {
  mem[pc++] = REG_ADD_LIT;
  mem[pc++] = r;
  mem[pc++] = (n & 0xf0) >> 4;
  mem[pc++] = (n & 0xf);
}

void encode_reg_random(reg_t r, uint8_t n) {
  mem[pc++] = REG_RANDOM;
  mem[pc++] = r;
  mem[pc++] = (n & 0xf0) >> 4;
  mem[pc++] = (n & 0xf);
}

void encode_if_reg_cmp_reg(cmp_t cmp, reg_t x, reg_t y) {
  mem[pc++] = IF_REG_CMP_REG;
  mem[pc++] = cmp;
  mem[pc++] = x;
  mem[pc++] = y;
}

void encode_if_reg_eq_lit(reg_t r, uint8_t n) {
  mem[pc++] = IF_REG_EQ_LIT;
  mem[pc++] = r;
  mem[pc++] = (n & 0xf0) >> 4;
  mem[pc++] = (n & 0xf);
}

void encode_if_reg_ne_lit(reg_t r, uint8_t n) {
  mem[pc++] = IF_REG_NE_LIT;
  mem[pc++] = r;
  mem[pc++] = (n & 0xf0) >> 4;
  mem[pc++] = (n & 0xf);
}

void encode_if_key(keys_t key) {
  mem[pc++] = IF_KEY;
  mem[pc++] = key;
  pc += 2;
}

/* ERROR AND CHECKS */

void error(const char *msg) {
  printf("ERROR AROUND LINE %d: %s\n", line, msg);
  exit(1);
}

bool is_number(uint8_t *n) {
  int i = 0;
  int ch = 0;
  int dig = 0;
  int num = 0;
  int len = strlen(token);
  int base = 10;
  bool neg = false;

  if (token[0] == '0' && token[1] == 'b') {
    i = 2;
    base = 2;
  } else if (token[0] == '0' && token[1] == 'x') {
    i = 2;
    base = 16;
  } else if (token[0] == '-') {
    i = 1;
    neg = true;
  }

  for (; i < len; i++) {
    ch = token[i];
    if (ch == '_') continue;

    if (48 <= ch && ch <= 57)
      // 0..9
      dig = ch - 48;
    else if (65 <= (ch & 0xdf) && (ch & 0xdf) <= 90) {
      // a..z
      dig = (ch & 0xdf) - 55;
    } else {
      return false;
    }
    num = num * base + dig;
  }

  *n = num * (neg ? -1 : 1);
  return true;
}

reg_t is_register() {
  if (strcmp(token, "x") == 0) return RX;
  if (strcmp(token, "y") == 0) return RY;
  if (strcmp(token, "z") == 0) return RZ;
  if (strcmp(token, "w") == 0) return RW;
  if (strcmp(token, "a") == 0) return RA;
  if (strcmp(token, "b") == 0) return RB;
  if (strcmp(token, "c") == 0) return RC;
  if (strcmp(token, "d") == 0) return RD;
  if (strcmp(token, "e") == 0) return RE;
  if (strcmp(token, "f") == 0) return RF;
  if (strcmp(token, "t") == 0) return RT;
  return 0;
}

keys_t is_key() {
  if (strcmp(token, "action") == 0) return KACTION;
  if (strcmp(token, "up") == 0) return KUP;
  if (strcmp(token, "down") == 0) return KDOWN;
  if (strcmp(token, "left") == 0) return KLEFT;
  if (strcmp(token, "right") == 0) return KRIGHT;
  return 0;
}

op_t is_operator() {
  if (strcmp(token, "=") == 0) return SET;
  if (strcmp(token, "+=") == 0) return ADD;
  if (strcmp(token, "-=") == 0) return SUB;
  if (strcmp(token, "*=") == 0) return MUL;
  if (strcmp(token, "/=") == 0) return DIV;
  if (strcmp(token, "%=") == 0) return MOD;
  if (strcmp(token, "&=") == 0) return AND;
  if (strcmp(token, "|=") == 0) return OR;
  if (strcmp(token, "^=") == 0) return XOR;
  return 0;
}

cmp_t is_compare() {
  if (strcmp(token, "==") == 0) return EQ;
  if (strcmp(token, "!=") == 0) return NE;
  if (strcmp(token, "<") == 0) return LT;
  if (strcmp(token, "<=") == 0) return LE;
  if (strcmp(token, ">") == 0) return GT;
  if (strcmp(token, ">=") == 0) return GE;
  return 0;
}

/* FILE PARSER */

char *scan_token() {
  char c;
  uint8_t i = 0;
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

/* STATEMENTS PARSER */

void parse_write() {
  uint8_t num;

  next_token();
  if (!is_number(&num)) error("invalid number");

  encode_write(num);
}

void parse_assign() {
  reg_t reg_to;
  reg_t reg_from;
  op_t op;
  uint8_t num;

  reg_to = is_register();
  next_token();
  if (!(op = is_operator())) error("expected operator after register");

  next_token();
  if ((reg_from = is_register())) {
    encode_reg_op_reg(op, reg_to, reg_from);
    return;
  }

  if (op == SET && strcmp(token, "random") == 0) {
    next_token();
    if (!is_number(&num)) error("invalid number");

    encode_reg_random(reg_to, num);
    return;
  }

  if (!(op == SET || op == ADD)) error("unexpected assignment operation");
  if (!is_number(&num)) error("invalid number");

  if (op == SET)
    encode_reg_set_lit(reg_to, num);
  else
    encode_reg_add_lit(reg_to, num);
}

void parse_if() {
  reg_t reg_lhs;
  reg_t reg_rhs;
  cmp_t cmp;
  uint8_t num;

  next_token();
  if (!(reg_lhs = is_register())) error("expected register in condition");

  next_token();
  if (!(cmp = is_compare())) error("expected comparison");

  next_token();
  if ((reg_rhs = is_register()))
    encode_if_reg_cmp_reg(cmp, reg_lhs, reg_rhs);
  else {
    if (!(cmp == EQ || cmp == NE)) error("unexpected comparison operator");
    if (!is_number(&num)) error("invalid number");

    if (cmp == EQ)
      encode_if_reg_eq_lit(reg_lhs, num);
    else
      encode_if_reg_ne_lit(reg_lhs, num);
  }

  next_token();
  if (strcmp(token, "then") != 0) error("expected \"then\"");
}

void parse_if_key() {
  keys_t key;

  next_token();
  if (!(key = is_key())) error("expected key in condition");

  encode_if_key(key);

  next_token();
  if (strcmp(token, "then") != 0) error("expected \"then\"");
}

void parse_print() {
  reg_t reg;

  next_token();
  if (!(reg = is_register())) error("expected register to print");

  encode_print(reg);
}

void parse_clear() {
  uint8_t col;

  next_token();
  if (!is_number(&col)) error("expected literal color");

  encode_clear(col);
}

void parse_color() {
  uint8_t col;

  next_token();
  if (!is_number(&col)) error("expected literal color");

  encode_color(col);
}

int next_token_label() {
  next_token();
  if (label_n == LABEL_MAX) error("too many labels");

  for (uint16_t i = 0; i < label_n; i++) {
    if (strcmp(token, label[i]) == 0) {
      return i;
    }
  }
  return -1;
}

void parse_sprite() {
  int i;
  if (0 <= (i = next_token_label())) {
    encode_sprite(i);
    return;
  }

  strcpy(label[label_n], token);
  label_offset[label_n] = 0;
  encode_sprite(label_n);
  label_n++;
}

void parse_label() {
  int i;
  if (0 <= (i = next_token_label())) {
    label_offset[i] = pc;
    return;
  }

  strcpy(label[label_n], token);
  label_offset[label_n] = pc;
  label_n++;
}

void parse_goto() {
  int i;
  if (0 <= (i = next_token_label())) {
    encode_goto(i);
    return;
  }

  strcpy(label[label_n], token);
  label_offset[label_n] = 0;
  encode_goto(label_n);
  label_n++;
}

/* PROCESS BYTECODE */

void resolve_gotos() {
  uint8_t a, b, c;
  uint16_t n;

  for (uint8_t i = 0; i < pc; i += 4) {
    if (mem[i] == GOTO || mem[i] == SPRITE) {
      a = mem[i + 1];
      b = mem[i + 2];
      c = mem[i + 3];

      n = label_offset[(a << 8) | (b << 4) | c];

      mem[i + 1] = (n & 0xf00) >> 8;
      mem[i + 2] = (n & 0xf0) >> 4;
      mem[i + 3] = (n & 0xf);
    }
  }
}

/* READER */

void read_file(char *name) {
  file = fopen(name, "r");
  if (file == NULL) error("couldn't open file");

  // code section
  while (scan_token() != NULL) {
    if (strcmp(token, "write") == 0)
      parse_write();
    else if (is_register())
      parse_assign();
    else if (strcmp(token, "print") == 0)
      parse_print();
    else if (strcmp(token, "clear") == 0)
      parse_clear();
    else if (strcmp(token, "color") == 0)
      parse_color();
    else if (strcmp(token, "sprite") == 0)
      parse_sprite();
    else if (strcmp(token, "if") == 0)
      parse_if();
    else if (strcmp(token, "key") == 0)
      parse_if_key();
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

/* GET FROM MEMORY */

uint8_t get_reg() {
  uint8_t c = mem[pc++];
  return c - 1;
}

uint8_t get_N() { return mem[pc++]; }

uint8_t get_NN() {
  uint8_t a = mem[pc++];
  uint8_t b = mem[pc++];
  return a * 0x10 + b;
}

uint16_t get_NNN() {
  uint8_t a = mem[pc++];
  uint8_t b = mem[pc++];
  uint8_t c = mem[pc++];
  return a * 0x100 + b * 0x10 + c;
}

/* EXECUTION FUNCTIONS */

void print_register() {
  printf("%d\n", regs[get_reg()]);
  pc += 2;
}

void clear_screen() {
  ClearBackground(PALETTE[get_N()]);
  pc += 2;
}

void assign_color() {
  uint8_t col = get_N();
  regs[RCOL - 1] = col;

  pc += 2;
}

void draw_sprite() {
  uint8_t *spr = &mem[get_NNN()];
  Color col = PALETTE[regs[RCOL - 1]];
  uint8_t ox = regs[RX - 1];
  uint8_t oy = regs[RY - 1];

  for (size_t x = 0; x < 8; x++)
    for (size_t y = 0; y < 4; y++)
      if (spr[y] & (128 >> x))
        DrawRectangle((ox + x) * scale, (oy + y) * scale, scale, scale, col);
}

void assign_register_to_register() {
  op_t op = mem[pc++];
  uint8_t reg_n = get_reg();

  switch (op) {
  case SET:
    regs[reg_n] = regs[get_reg()];
    break;
  case ADD:
    regs[reg_n] += regs[get_reg()];
    break;
  case SUB:
    regs[reg_n] -= regs[get_reg()];
    break;
  case MUL:
    regs[reg_n] *= regs[get_reg()];
    break;
  case DIV:
    regs[reg_n] /= regs[get_reg()];
    break;
  case MOD:
    regs[reg_n] %= regs[get_reg()];
    break;
  case AND:
    regs[reg_n] &= regs[get_reg()];
    break;
  case OR:
    regs[reg_n] |= regs[get_reg()];
    break;
  case XOR:
    regs[reg_n] ^= regs[get_reg()];
    break;
  }
}

void if_reg_cmp_reg_is_false_skip_next_instruction() {
  cmp_t cmp = mem[pc++];
  uint8_t a = regs[get_reg()];
  uint8_t b = regs[get_reg()];

  switch (cmp) {
  case EQ:
    if (!(a == b)) pc += 4;
    break;
  case NE:
    if (!(a != b)) pc += 4;
    break;
  case LT:
    if (!(a < b)) pc += 4;
    break;
  case LE:
    if (!(a <= b)) pc += 4;
    break;
  case GT:
    if (!(a > b)) pc += 4;
    break;
  case GE:
    if (!(a >= b)) pc += 4;
    break;
  }
}

void if_reg_eq_lit_is_false_skip_next_instruction() {
  uint8_t r_val = regs[get_reg()];
  uint8_t n = get_NN();

  if (!(r_val == n)) pc += 4;
}

void if_reg_ne_lit_is_false_skip_next_instruction() {
  uint8_t r_val = regs[get_reg()];
  uint8_t n = get_NN();

  if (!(r_val != n)) pc += 4;
}

void if_not_key_skip_next_instruction() {
  keys_t key = mem[pc++];
  pc += 2;

  switch (key) {
  case KACTION:
    if (!(IsKeyDown(KEY_SPACE) || IsKeyDown(KEY_ENTER))) pc += 4;
    break;
  case KUP:
    if (!(IsKeyDown(KEY_UP) || IsKeyDown(KEY_W))) pc += 4;
    break;
  case KDOWN:
    if (!(IsKeyDown(KEY_DOWN) || IsKeyDown(KEY_S))) pc += 4;
    break;
  case KLEFT:
    if (!(IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_A))) pc += 4;
    break;
  case KRIGHT:
    if (!(IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D))) pc += 4;
    break;
  }
}

/* EXECUTION */

void exec() {
  ins_t o;
  uint8_t reg_n;

  while ((o = mem[pc++])) {
    switch (o) {
    case HALT:
      return;
    case GOTO:
      pc = get_NNN();
      break;
    case PRINT:
      print_register();
      break;
    case CLEAR:
      clear_screen();
      break;
    case COLOR:
      assign_color();
      break;
    case SPRITE:
      draw_sprite();
      break;
    case REG_OP_REG:
      assign_register_to_register();
      break;
    case IF_REG_CMP_REG:
      if_reg_cmp_reg_is_false_skip_next_instruction();
      break;
    case IF_REG_EQ_LIT:
      if_reg_eq_lit_is_false_skip_next_instruction();
      break;
    case IF_REG_NE_LIT:
      if_reg_ne_lit_is_false_skip_next_instruction();
      break;
    case IF_KEY:
      if_not_key_skip_next_instruction();
      break;
    case REG_SET_LIT:
      reg_n = get_reg();
      regs[reg_n] = get_NN();
      break;
    case REG_ADD_LIT:
      reg_n = get_reg();
      regs[reg_n] += get_NN();
      break;
    case REG_RANDOM:
      reg_n = get_reg();
      regs[reg_n] = (float)rand() / RAND_MAX * get_NN();
      break;
    }
  }
}

/* MAIN */

int main(void) {
  read_file("game.baya");

  for (int i = 0; i < pc; i++) {
    printf("%x", mem[i]);
  }
  putchar('\n');

  SetTraceLogLevel(LOG_ERROR);
  InitWindow(SCREEN_WIDTH * scale, SCREEN_HEIGHT * scale, "🫐 baya");
  SetTargetFPS(12);

  while (!WindowShouldClose()) {
    BeginDrawing();

    exec();
    pc = 0;
    regs[RT - 1]++;

    EndDrawing();
  }

  CloseWindow();

  return 0;
}
