#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

char *scan(FILE *f, char t[16]) {
  char c;
  size_t i = 0;
  bool comment = false;

  while ((c = fgetc(f)) != EOF) {
    comment = (comment || c == '(') && c != ')';
    if (comment || c == ')') continue;

    if (isgraph(c) && i < 15)
      t[i++] = c;
    else {
      t[i++] = '\0';
      if (t[0] != '\0') return t;
      i = 0;
    }
  }
  return NULL;
}

void parse(FILE *f) {
  char *t;

  while ((t = scan(f, t)) != NULL) {
    printf("%s\n", t);
  }
}

void read(char *name) {
  FILE *f;

  f = fopen(name, "r");
  if (f == NULL) exit(1);

  parse(f);

  fclose(f);
}

int main(void) {
  read("game.baya");

  return 0;
}