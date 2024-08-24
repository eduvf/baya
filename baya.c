#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "raylib.h"

#define SCREEN_WIDTH 64
#define SCREEN_HEIGHT 32
#define TOKEN_LENGTH 16
#define LABEL_MAX 64

#define PALETTE_SIZE 4
Color PALETTE[] = {WHITE, SKYBLUE, BLUE, DARKBLUE};

char scale = 6;

FILE *file;
char token[TOKEN_LENGTH];
int line = 1;

char label_n = 0;
char label[LABEL_MAX][TOKEN_LENGTH];
char label_offset[LABEL_MAX];

uint8_t sprites[64][4];
char sprites_n = 0;
char memory[(1 << 12)];
char pc = 0;

#define REGISTER_N 5
char registers[REGISTER_N];
typedef enum { RX = 1, RY, RZ, RW, RT } reg_t;

typedef enum { KACTION = 1, KUP, KDOWN, KLEFT, KRIGHT } keys_t;

typedef enum {
  HALT = 1,
  GOTO,           // goto NNN
  PRINT,          // print x
  CLS,            // cls N
  SPRITE,         // sprite N N (id color)
  REG_OP_REG,     // x o= y
  REG_SET_LIT,    // x = NN
  REG_ADD_LIT,    // x += NN
  IF_REG_CMP_REG, // if x c y then
  IF_KEY,         // key K then
} ins_t;

typedef enum {
  SET = 1, // =
  ADD,     // +=
  SUB,     // -=
  MUL,     // *=
  DIV,     // /=
  MOD      // %=
} op_t;

typedef enum {
  EQ = 1, // ==
  NE      // !=
} cmp_t;

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

void encode_print(reg_t r) {
  memory[pc++] = PRINT;
  memory[pc++] = r;
  pc += 2;
}

void encode_cls(int n) {
  memory[pc++] = CLS;
  memory[pc++] = n & (PALETTE_SIZE - 1);
  pc += 2;
}

void encode_sprite(int id, int col) {
  memory[pc++] = SPRITE;
  memory[pc++] = id;
  memory[pc++] = col & (PALETTE_SIZE - 1);
  pc++;
}

void encode_reg_op_reg(op_t op, reg_t x, reg_t y) {
  memory[pc++] = REG_OP_REG;
  memory[pc++] = op;
  memory[pc++] = x;
  memory[pc++] = y;
}

void encode_reg_set_lit(reg_t r, int n) {
  memory[pc++] = REG_SET_LIT;
  memory[pc++] = r;
  memory[pc++] = (n & 0xf0) >> 4;
  memory[pc++] = (n & 0xf);
}

void encode_reg_add_lit(reg_t r, int n) {
  memory[pc++] = REG_ADD_LIT;
  memory[pc++] = r;
  memory[pc++] = (n & 0xf0) >> 4;
  memory[pc++] = (n & 0xf);
}

void encode_if_reg_cmp_reg(cmp_t cmp, reg_t x, reg_t y) {
  memory[pc++] = IF_REG_CMP_REG;
  memory[pc++] = cmp;
  memory[pc++] = x;
  memory[pc++] = y;
}

void encode_if_key(keys_t key) {
  memory[pc++] = IF_KEY;
  memory[pc++] = key;
  pc += 2;
}

char is_number(char *n) {
  *n = (char)strtol(token, NULL, 0);
  return errno;
}

void error(const char *msg) {
  printf("ERROR AROUND LINE %d: %s\n", line, msg);
  exit(1);
}

reg_t is_register() {
  if (strcmp(token, "x") == 0) return RX;
  if (strcmp(token, "y") == 0) return RY;
  if (strcmp(token, "z") == 0) return RZ;
  if (strcmp(token, "w") == 0) return RW;
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
  return 0;
}

cmp_t is_compare() {
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
  reg_t reg_to;
  reg_t reg_from;
  op_t op;
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
  reg_t reg_lhs;
  reg_t reg_rhs;
  cmp_t cmp;
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

void parse_if_key() {
  keys_t key;

  next_token();
  if (!(key = is_key())) error("expected key in condition");

  encode_if_key(key);

  next_token();
  if (strcmp(token, "then") != 0) error("expected \"then\"");
  return;
}

void parse_print() {
  reg_t reg;

  next_token();
  if (!(reg = is_register())) error("expected register to print");

  encode_print(reg);
  return;
}

void parse_cls() {
  char col;

  next_token();
  if (is_number(&col) != 0) error("expected literal color");

  encode_cls(col);
  return;
}

void parse_sprite() {
  char id;
  char col;

  next_token();
  if (is_number(&id) != 0) error("expected sprite id");
  next_token();
  if (is_number(&col) != 0) error("expected literal color");

  encode_sprite(id, col);
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

bool parse_sprite_data() {
  char ch;
  uint8_t spr[4] = {0};

  for (size_t i = 0; i < 4; i++) {
    if (scan_token() == NULL) return false;

    for (size_t j = 0; j < 8; j++) {
      ch = token[j];
      if (ch == 'x') spr[i] |= 128 >> j;
    }
    sprites[sprites_n][i] = spr[i];
  }
  sprites_n++;
  return true;
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

void read_file(char *name) {
  file = fopen(name, "r");
  if (file == NULL) error("couldn't open file");

  // ignore header
  while (scan_token() != NULL) {
    if (strcmp(token, "::") == 0) break;
  }

  // code section
  while (scan_token() != NULL) {
    if (is_register())
      parse_assign();
    else if (strcmp(token, "print") == 0)
      parse_print();
    else if (strcmp(token, "cls") == 0)
      parse_cls();
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
    else if (strcmp(token, "::") == 0)
      break;
    else
      error("invalid instruction");
  }
  encode_halt();

  resolve_gotos();

  // process sprites
  while (parse_sprite_data())
    ;

  fclose(file);
}

int get_register() {
  char c = memory[pc++];
  return c - 1;
}

int getN() { return memory[pc++]; }

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

void print_register() {
  printf("%d\n", registers[get_register()]);
  pc += 2;
}

void clear_screen() {
  ClearBackground(PALETTE[getN()]);
  pc += 2;
}

void draw_sprite() {
  uint8_t *spr = sprites[getN()];
  Color col = PALETTE[getN()];
  int ox = registers[RX - 1];
  int oy = registers[RY - 1];

  for (size_t x = 0; x < 8; x++)
    for (size_t y = 0; y < 4; y++)
      if (spr[y] & (128 >> x)) {
        DrawRectangle((ox + x) * scale, (oy + y) * scale, scale, scale, col);
      }

  pc++;
}

void assign_register_to_register() {
  op_t op = memory[pc++];
  int reg_n = get_register();

  switch (op) {
  case SET: registers[reg_n] = registers[get_register()]; break;
  case ADD: registers[reg_n] += registers[get_register()]; break;
  case SUB: registers[reg_n] -= registers[get_register()]; break;
  case MUL: registers[reg_n] *= registers[get_register()]; break;
  case DIV: registers[reg_n] /= registers[get_register()]; break;
  case MOD: registers[reg_n] %= registers[get_register()]; break;
  }
}

void if_false_skip_next_instruction() {
  cmp_t cmp = memory[pc++];
  int a = registers[get_register()];
  int b = registers[get_register()];

  switch (cmp) {
  case EQ:
    if (!(a == b)) pc += 4;
    break;
  case NE:
    if (!(a != b)) pc += 4;
    break;
  }
}

void if_not_key_skip_next_instruction() {
  keys_t key = memory[pc++];
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

void exec() {
  ins_t o;
  int reg_n;

  while ((o = memory[pc++])) {
    switch (o) {
    case HALT: return;
    case GOTO: {
      pc = getNNN();
      break;
    }
    case PRINT: {
      print_register();
      break;
    }
    case CLS: {
      clear_screen();
      break;
    }
    case SPRITE: {
      draw_sprite();
      break;
    }
    case REG_OP_REG: {
      assign_register_to_register();
      break;
    }
    case IF_REG_CMP_REG: {
      if_false_skip_next_instruction();
      break;
    }
    case IF_KEY: {
      if_not_key_skip_next_instruction();
      break;
    }
    case REG_SET_LIT: {
      reg_n = get_register();
      registers[reg_n] = getNN();
      break;
    }
    case REG_ADD_LIT: {
      reg_n = get_register();
      registers[reg_n] += getNN();
      break;
    }
    }
  }
}

int main(void) {
  read_file("game.baya");

  for (int i; i < pc; i += 4) {
    printf("%x", memory[i]);
    printf("%x", memory[i + 1]);
    printf("%x", memory[i + 2]);
    printf("%x ", memory[i + 3]);
  }
  putchar('\n');

  SetTraceLogLevel(LOG_ERROR);
  InitWindow(SCREEN_WIDTH * scale, SCREEN_HEIGHT * scale, "🫐 baya");
  SetTargetFPS(12);

  // pc = 4; // ignore first instruction
  while (!WindowShouldClose()) {
    BeginDrawing();

    exec();
    pc = 0;
    registers[RT - 1]++;

    EndDrawing();
  }

  CloseWindow();

  return 0;
}