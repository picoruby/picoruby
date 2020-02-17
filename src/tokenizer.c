#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>

#include "common.h"
#include "tokenizer.h"
#include "token.h"
#include "my_regex.h"

static bool tokenizer_is_keyword(char *word)
{
  for (int i = 0; KEYWORDS[i].string != NULL; i++){
    if ( !strcmp(word, KEYWORDS[i].string) )
      return true;
  }
  return false;
}

static bool tokenizer_is_operator(char *word, size_t len)
{
  switch (len) {
    case 3:
      for (int i = 0; OPERATORS_3[i].string != NULL; i++){
        if ( !strncmp(word, OPERATORS_3[i].string, len) )
          return true;
      }
      break;
    case 2:
      for (int i = 0; OPERATORS_2[i].string != NULL; i++){
        if ( !strncmp(word, OPERATORS_2[i].string, len) )
          return true;
      }
      break;
    case 1:
      for (int i = 0; OPERATORS_1[i].letter != 0; i++){
        if ( *word == OPERATORS_1[i].letter )
          return true;
      }
      break;
    default:
      fprintf(stderr, "Out of range!");
      break;
  }
  return false;
}

static bool tokenizer_is_paren(int letter)
{
  for (int i = 0; PARENS[i].letter != 0; i++){
    if ( letter == PARENS[i].letter )
      return true;
  }
  return false;
}

static bool tokenizer_is_semicolon(int letter)
{
  for (int i = 0; SEMICOLON[i].letter != 0; i++){
    if ( letter == SEMICOLON[i].letter )
      return true;
  }
  return false;
}

static bool tokenizer_is_comma(int letter)
{
  for (int i = 0; COMMA[i].letter != 0; i++){
    if ( letter == COMMA[i].letter )
      return true;
  }
  return false;
}

static void tokenizer_paren_stack_pop(Tokenizer* const self)
{
  self->paren_stack_num--;
}

static void tokenizer_paren_stack_add(Tokenizer* const self, Paren paren)
{
  self->paren_stack_num++;
  self->paren_stack[self->paren_stack_num] = paren;
}

Tokenizer* const Tokenizer_new(FILE *file)
{
  Tokenizer *self = malloc(sizeof(Tokenizer));
  self->file = file;
  self->currentToken = Token_new();
  self->mode = MODE_NONE;
  self->paren_stack_num = 0;
  self->line_num = 0;
  self->line = malloc(sizeof(char) * (MAX_LINE_LENGTH + 1));
  tokenizer_paren_stack_add(self, PAREN_NONE);
  return self;
}

void Tokenizer_free(Tokenizer *self)
{
  Token_free(self->currentToken); // FIXME memory leak
  free(self->line);
  free(self);
}

void tokenizer_readLine(Tokenizer* const self)
{
//  printf("readLine line_num: %d, pos: %d\n", self->line_num, self->pos);
  if (self->pos >= strlen(self->line)) {
    if (fgets(self->line, MAX_LINE_LENGTH, self->file) == NULL)
      self->line[0] = '\0';
    printf("line size: %ld\n", strlen(self->line));
    self->line_num++;
    self->pos = 0;
  }
}

bool Tokenizer_hasMoreTokens(Tokenizer* const self)
{
  if (self->file == NULL) {
    return false;
  } else if ( feof(self->file) == 0 || (self->line[0] != '\0') ) {
    return true;
  } else {
    return false;
  }
}

void tokenizer_pushToken(Tokenizer *self, int line_num, int pos, Type type, char *value, State state)
{
  printf("pushToken: `%s`\n", value);
  self->currentToken->pos = pos;
  self->currentToken->line_num = line_num;
  self->currentToken->type = type;
  self->currentToken->value = malloc(sizeof(char) * (strlen(value) + 1));
  strcpy(self->currentToken->value, value);
  self->currentToken->state = state;
  Token *newToken = Token_new();
  newToken->prev = self->currentToken;
  self->currentToken->next = newToken;
  self->currentToken = newToken;
}

void tokenizer_freeOldTokens(Token* token)
{
  // TODO
}

int Tokenizer_advance(Tokenizer* const self, bool recursive)
{
  printf("advance\n");
  //tokenizer_freeOldTokens(self->currentToken->prev); FIXME
  Token *lazyToken = Token_new();
  char value[MAX_TOKEN_LENGTH + 1];
  memset(value, '\0', MAX_TOKEN_LENGTH + 1);
  Type type = ON_NONE;
  char c[3];
  memset(c, '\0', sizeof(c));

  RegexResult regexResult[REGEX_MAX_RESULT_NUM];

  tokenizer_readLine(self);
  if (self->line[0] == '\0') {
    Token_free(lazyToken);
    return -1;
  }
  if (self->mode == MODE_COMMENT) {
    type = (Regex_match2(self->line, "^=end(\\s|$)")) ? ON_EMBDOC_END : ON_EMBDOC;
    self->mode = MODE_NONE;
    strcpy(value, self->line);
  } else if (self->mode == MODE_QWORDS
          || self->mode == MODE_WORDS
          || self->mode == MODE_QSYMBOLS
          || self->mode == MODE_SYMBOLS) {
    for (;;) {
      tokenizer_readLine(self);
      value[0] = '\0';
      type = ON_NONE;
      if (self->line[self->pos] == self->modeTerminater) {
        lazyToken->line_num = self->line_num;
        lazyToken->pos = self->pos;
        lazyToken->type = ON_TSTRING_END;
        lazyToken->value = malloc(sizeof(char) *(2));
        *(lazyToken->value) = self->modeTerminater;
        *(lazyToken->value + 1) = '\0';
        lazyToken->state = EXPR_END;
        self->pos++;
        self->mode = MODE_NONE;
        break;
      } else if (self->line[self->pos] == ' ' || self->line[self->pos] == '\n') {
        Regex_match3(&(self->line[self->pos]), "^([\\s]+)", regexResult);
        strcpy(value, regexResult[0].value);
        type = ON_WORDS_SEP;
      } else {
        int i = 0;
        for (;;) {
          c[0] = self->line[self->pos + i];
          if (c[0] == '\0') break;
          if (c[0] != ' ' && c[0] != '\t' && c[0] != '\n' && c[0] != self->modeTerminater) {
            strcat(value, c);
            i++;
          } else {
            break;
          }
        }
        type = ON_TSTRING_CONTENT;
      }
      if (strlen(value) > 0) {
        if (type == ON_WORDS_SEP && self->currentToken->prev->type == ON_WORDS_SEP) {
          strcat(self->currentToken->prev->value, value);
        } else {
          tokenizer_pushToken(self,
            self->line_num,
            self->pos,
            type,
            value,
            EXPR_BEG);
        }
        self->pos += strlen(value);
      }
    }
    self->pos--;
  } else if (self->mode == MODE_TSTRING_DOUBLE) {
    for (;;) {
      tokenizer_readLine(self);
      if (self->line[0] == '\0') {
        Token_free(lazyToken);
        return -1;
      }
      if (self->line[self->pos] == self->modeTerminater) {
        lazyToken->line_num = self->line_num;
        lazyToken->pos = self->pos;
        lazyToken->type = ON_TSTRING_END;
        lazyToken->value = malloc(sizeof(char) *(2));
        *(lazyToken->value) = self->modeTerminater;
        *(lazyToken->value + 1) = '\0';;
        lazyToken->state = EXPR_END;
        self->pos++;
        self->mode = MODE_NONE;
        break;
      } else if (self->line[self->pos] == '#' && self->line[self->pos + 1] == '{') {
        tokenizer_pushToken(self,
          self->line_num,
          self->pos,
          ON_TSTRING_CONTENT,
          value,
          EXPR_BEG);
        value[0] = '\0';
        c[0] = '\0';
        tokenizer_pushToken(self,
          self->line_num,
          self->pos,
          ON_EMBDOC_BEG,
          "#{",
          EXPR_BEG);
        self->pos += 2;
        tokenizer_paren_stack_add(self, PAREN_BRACE);
        while (Tokenizer_hasMoreTokens(self)) {
          if (Tokenizer_advance(self, false) == 1) break;
        }
        tokenizer_pushToken(self,
          self->line_num,
          self->pos,
          ON_EMBEXPR_END,
          "}",
          EXPR_CMDARG);
        self->pos++;
      } else if (self->line[self->pos] == '\\' && self->line[self->pos + 1] == self->modeTerminater) {
        c[0] = '\\';
        c[1] = self->modeTerminater;
      } else {
        c[0] = self->line[self->pos];
      }
      self->pos += strlen(c);
      strcat(value, c);
    }
    self->pos--;
    if (strlen(value) > 0) {
      type = ON_TSTRING_CONTENT;
    }
  } else if (self->mode == MODE_TSTRING_SINGLE) {
  } else if (Regex_match2(self->line, "^=begin(\\s|$)")) { // multi lines comment began
    self->mode = MODE_COMMENT;
    strcpy(value, self->line);
    type = ON_EMBDOC_BEG;
  } else if (self->line[self->pos] == '\n') {
    value[0] = '\n';
    value[1] = '\0';
    type = ON_NL;
  } else if (self->line[self->pos] == '\r' && self->line[self->pos + 1] == '\n') {
    value[0] = '\r';
    value[1] = '\n';
    value[2] = '\0';
    type = ON_NL;
  } else if (tokenizer_is_operator(&(self->line[self->pos]), 3)) {
    value[0] = self->line[self->pos];
    value[1] = self->line[self->pos + 1];
    value[2] = self->line[self->pos + 2];
    value[3] = '\0';
    type = ON_OP;
  } else if (tokenizer_is_operator(&(self->line[self->pos]), 2)) {
    value[0] = self->line[self->pos];
    value[1] = self->line[self->pos + 1];
    value[2] = '\0';
    type = ON_OP;
  } else if (Regex_match3(&(self->line[self->pos]), "^(@\\w+)", regexResult)) {
    strcpy(value, regexResult[0].value);
    type = ON_IVAR;
  } else if (Regex_match3(&(self->line[self->pos]), "^(\\$\\w+)", regexResult)) {
    strcpy(value, regexResult[0].value);
    type = ON_GVAR;
  } else if (Regex_match3(&(self->line[self->pos]), "^(\\?.)", regexResult)) {
    strcpy(value, regexResult[0].value);
    type = ON_CHAR;
  } else if (self->line[self->pos] == '-' && self->line[self->pos + 1] == '>') {
    value[0] = '-';
    value[1] = '>';
    value[2] = '\0';
    type = ON_TLAMBDA;
  } else {
    if (self->line[self->pos] == '\\') {
      // ignore
      self->pos++;
    } else if (self->line[self->pos] == ':') {
      if (Regex_match2(&(self->line[self->pos]), "^:[A-Za-z0-9]?")) {
        value[0] = ':';
        value[1] = '\0';
        type = ON_SYMBEG;
      } else {
        // nothing TODO?
      }
    } else if (self->line[self->pos] == '#') {
      strcpy(value, &(self->line[self->pos]));
      type = ON_COMMENT;
    } else if (self->line[self->pos] == ' ' || self->line[self->pos] == '\t') {
      Regex_match3(&(self->line[self->pos]), "^(\\s+)", regexResult);
      strcpy(value, regexResult[0].value);
      type = ON_SP;
    } else if (tokenizer_is_paren(self->line[self->pos])) {
      value[0] = self->line[self->pos];
      value[1] = '\0';
      switch (value[0]) {
        case '(':
          type = ON_LPAREN;
          break;
        case ')':
          type = ON_RPAREN;
          self->state = EXPR_ENDFN;
          break;
        case '[':
          type = ON_LBRACKET;
          break;
        case ']':
          type = ON_RBRACKET;
          break;
        case '{':
          type = ON_LBRACE;
          self->state = EXPR_BEG|EXPR_LABEL;
          break;
        case '}':
          if (self->paren_stack[self->paren_stack_num] == PAREN_BRACE) {
            tokenizer_paren_stack_pop(self);
            Token_free(lazyToken);
            return 1;
          }
          type = ON_RBRACE;
          break;
        default:
          fprintf(stderr, "unknown paren error\n");
      }
    } else if (tokenizer_is_operator(&(self->line[self->pos]), 1)) {
      if (Regex_match3(&(self->line[self->pos]), "^(%[iIwWq][~!@#$%^&*()_\\-=+\\[{\\]};:'\"?])", regexResult)) {
        strcpy(value, regexResult[0].value);
        switch (value[1]) {
          case 'w':
            type = ON_QWORDS_BEG;
            self->mode = MODE_QWORDS;
            break;
          case 'W':
            type = ON_WORDS_BEG;
            self->mode = MODE_WORDS;
            break;
          case 'q':
            type = ON_TSTRING_BEG;
            self->mode = MODE_TSTRING_SINGLE;
            break;
          case 'Q':
            type = ON_TSTRING_BEG;
            self->mode = MODE_TSTRING_DOUBLE;
            break;
          case 'i':
            type = ON_QSYMBOLS_BEG;
            self->mode = MODE_QSYMBOLS;
            break;
          case 'I':
            type = ON_SYMBOLS_BEG;
            self->mode = MODE_SYMBOLS;
            break;
        }
        switch (value[2]) {
          case '[':
            self->modeTerminater = ']';
            break;
          case '{':
            self->modeTerminater = '}';
            break;
          case '(':
            self->modeTerminater = ')';
            break;
          default:
            self->modeTerminater = value[2];
        }
      }
    } else if (tokenizer_is_semicolon(self->line[self->pos])) {
      strcpy(value, &(self->line[self->pos]));
      type = ON_SEMICOLON;
      self->state = EXPR_BEG;
    } else if (tokenizer_is_comma(self->line[self->pos])) {
      strcpy(value, &(self->line[self->pos]));
      type = ON_COMMA;
      self->state = EXPR_BEG|EXPR_LABEL;
    } else if ('0' <= self->line[self->pos] && self->line[self->pos] <= '9') {
      if (Regex_match3(&(self->line[self->pos]), "^([0-9_]+\\.[0-9][0-9_]*)", regexResult)) {
        strcpy(value, regexResult[0].value);
        type = ON_FLOAT;
      } else if (Regex_match3(&(self->line[self->pos]), "^([0-9_]+)", regexResult)) {
        strcpy(value, regexResult[0].value);
        type = ON_INT;
      } else {
        fprintf(stderr, "Failed to tokenize a number");
      }
    } else if (self->line[self->pos] == '.') {
      value[0] = '.';
      value[1] = '\0';
      type = ON_PERIOD;
    } else if ( ('a' <= self->line[self->pos] && self->line[self->pos] <= 'z')
                || ('A' <= self->line[self->pos] && self->line[self->pos] <= 'Z')
                || (self->line[self->pos] == '_') ) {
      if (Regex_match3(&(self->line[self->pos]), "^([A-Za-z0-9_?!]+:)", regexResult)) {
        strcpy(value, regexResult[0].value);
        type = ON_LABEL;
      } else if (Regex_match3(&(self->line[self->pos]), "^([A-Z]\\w*[!?])", regexResult)) {
        strcpy(value, regexResult[0].value);
        type = ON_IDENT;
      } else if (Regex_match3(&(self->line[self->pos]), "^([A-Z]\\w*)", regexResult)) {
        strcpy(value, regexResult[0].value);
        type = ON_CONST;
      } else if (Regex_match3(&(self->line[self->pos]), "^(\\w+[!?]?)", regexResult)) {
        strcpy(value, regexResult[0].value);
        type = ON_IDENT;
      }
    } else if (self->line[self->pos] == '"') {
      value[0] = '"';
      value[1] = '\0';
      self->mode = MODE_TSTRING_DOUBLE;
      self->modeTerminater = '"';
      type = ON_TSTRING_BEG;
    } else if (self->line[self->pos] == '\'') {
      value[0] = '\'';
      value[1] = '\0';
      self->mode = MODE_TSTRING_SINGLE;
      self->modeTerminater = '\'';
      type = ON_TSTRING_BEG;
    } else {
      fprintf(stderr, "ERROR error\n");
    }
  }
  if (lazyToken->value == NULL) {
    self->pos += strlen(value);
  }
  if (type != ON_NONE) {
    if ( (type == ON_IDENT || type == ON_CONST)
         && tokenizer_is_keyword(value) ) {
      type = ON_KW;
      if ( !strcmp(value, "class") ) {
        self->state = EXPR_CLASS;
      } else if ( !strcmp(value, "return")
                  || !strcmp(value, "break")
                  || !strcmp(value, "next")
                  || !strcmp(value, "rescue") ) {
        self->state = EXPR_MID;
      } else if ( !strcmp(value, "def")
                  || !strcmp(value, "alias")
                  || !strcmp(value, "undef") ) {
        self->state = EXPR_FNAME;
      }
    } else { // on_ident
      switch (self->state) {
        case EXPR_CLASS:
          self->state = EXPR_ARG;
          break;
        case EXPR_FNAME:
          self->state = EXPR_ENDFN;
          break;
        default:
          // fprintf(stderr, "error\n");
          break;
      }
    }
    printf("value len: %ld, `%s`\n", strlen(value), value);
    tokenizer_pushToken(self,
      self->line_num,
      self->pos - strlen(value),
      type,
      value,
      self->state);
  }
  if (lazyToken->value != NULL) {
    lazyToken->prev = self->currentToken;
    self->currentToken->next = lazyToken;
    self->currentToken = lazyToken;
    self->pos++;
  } else {
    Token_free(lazyToken);
  }
  return 0;
}

