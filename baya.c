#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#define TOK_LEN 16

FILE *f;
char t[TOK_LEN];

char *scan() {
  char c;
  size_t i = 0;
  bool comment = false;

  while ((c = fgetc(f)) != EOF) {
    comment = (comment || c == '(') && c != ')';
    if (comment || c == ')') continue;

    if (isgraph(c) && i < (TOK_LEN - 1))
      t[i++] = c;
    else {
      t[i++] = '\0';
      if (t[0] != '\0') return t;
      i = 0;
    }
  }
  return NULL;
}

void parse() {
  while (scan() != NULL) {
    printf("%s\n", t);
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