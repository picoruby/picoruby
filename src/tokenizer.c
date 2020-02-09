#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#include "common.h"
#include "tokenizer.h"
#include "token.h"
#include "my_regex.h"

bool tokenizer_is_keyword(char *word)
{
  for (int i = 0; KEYWORDS[i].string != NULL; i++){
    if ( !strcmp(word, KEYWORDS[i].string) ){
      return true;
    }
  }
  return false;
}

void tokenizer_paren_stack_add(Tokenizer* const self, Paren paren)
{
  self->paren_stack_num++;
  self->paren_stack[self->paren_stack_num] = paren;
}

void Tokenizer_new(Tokenizer* const self)
{
  self->mode = MODE_NONE;
  self->paren_stack_num = 0;
  self->line_num = 0;
  tokenizer_paren_stack_add(self, PAREN_NONE);
}

void Tokenizer_puts(Tokenizer* const self, char *line)
{
  self->line = line;
  self->line_num++;
  self->pos = 0;
}

bool Tokenizer_hasMoreTokens(Tokenizer* const self)
{
  if ( (strlen(self->line) == self->pos) || (self->line[0] == '\0') )
    return false;
  return true;
}

void Tokenizer_advance(Tokenizer* const self, bool recursive)
{
  char c;
  Token lazy_token, token;
  Token_new(&lazy_token);
  Token_new(&token);

  char (*regex_result)[MAX_RESULT_NUM];
  char *regex_pattern;

  if (self->mode == MODE_COMMENT) {
    regex_pattern = "\\A=end(\\s|\\z)";
    if (Regex_match2(self->line, regex_pattern)) {
      token.type = ON_EMBDOC_END;
    } else {
      token.type = ON_EMBDOC;
    }
    self->mode = MODE_NONE;
    token.value = self->line;
    char end[] ="\n";
    strcat(token.value, end);
  } else if (true) {
    for (;;) {
      if (strlen(self->line) == self->pos)
        break;
      c = self->line[self->pos];
      self->pos++;
      putchar(c);
    }
  }
  printf("SIZE: %d\n", sizeof(KEYWORDS));
}

