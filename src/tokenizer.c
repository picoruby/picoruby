#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>

#include "mmrbc.h"
#include "ruby-lemon-parse/parse.h"
#include "ruby-lemon-parse/keyword_helper.h"
#include "common.h"
#include "tokenizer.h"
#include "token.h"
#include "my_regex.h"

#include "token_data.h"

#define IS_ARG() (self->state == EXPR_ARG || self->state == EXPR_CMDARG)
#define IS_END() (self->state == EXPR_END || self->state == EXPR_ENDARG || self->state == EXPR_ENDFN)
#define IS_BEG() (self->state == EXPR_BEG || self->state == EXPR_MID || self->state == EXPR_VALUE || self->state == EXPR_CLASS)

#define IS_NUM(n) ('0' <= self->line[self->pos+(n)] && self->line[self->pos+(n)] <= '9')

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
      ERRORP("Out of range!");
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
  for (int i = 0; SEMICOLONS[i].letter != 0; i++){
    if ( letter == SEMICOLONS[i].letter )
      return true;
  }
  return false;
}

static bool tokenizer_is_comma(int letter)
{
  for (int i = 0; COMMAS[i].letter != 0; i++){
    if ( letter == COMMAS[i].letter )
      return true;
  }
  return false;
}

static void tokenizer_paren_stack_pop(Tokenizer* const self)
{
  self->paren_stack_num--;
  DEBUGP("Paren popped stack_num: %d", self->paren_stack_num);
}

static void tokenizer_paren_stack_add(Tokenizer* const self, Paren paren)
{
  self->paren_stack_num++;
  self->paren_stack[self->paren_stack_num] = paren;
  DEBUGP("Paren added stack_num: %d, `%d`", self->paren_stack_num, paren);
}

Tokenizer* const Tokenizer_new(StreamInterface *si)
{
  Tokenizer *self = (Tokenizer *)mmrbc_alloc(sizeof(Tokenizer));
  /* class vars in mmrbc.gem */
  {
    self->currentToken = Token_new();
    self->paren_stack_num = -1;
    self->line = (char *)mmrbc_alloc(sizeof(char) * (MAX_LINE_LENGTH));
    self->line[0] = '\0';
    self->line_num = 0;
    self->pos = 0;
  }
  tokenizer_paren_stack_add(self, PAREN_NONE);
  /* instance vars in mmrbc.gem */
  {
    self->si = si;
    self->mode = MODE_NONE;
    self->modeTerminater = '\0';
    self->state = EXPR_BEG;
    self->cmd_start = true;
  }
  return self;
}

void Tokenizer_free(Tokenizer *self)
{
  Token_free(self->currentToken);
  mmrbc_free(self->line);
  mmrbc_free(self);
}

void tokenizer_readLine(Tokenizer* const self)
{
  DEBUGP("readLine line_num: %d, pos: %d", self->line_num, self->pos);
  if (self->pos >= strlen(self->line)) {
    if (self->si->fgetsProc(self->line, MAX_LINE_LENGTH, self->si->stream) == NULL)
      self->line[0] = '\0';
    DEBUGP("line size: %ld", strlen(self->line));
    self->line_num++;
    self->pos = 0;
  }
}

bool Tokenizer_hasMoreTokens(Tokenizer* const self)
{
  if (self->si->stream == NULL) {
    return false;
  } else if ( self->si->feofProc(self->si->stream) == 0 || (self->line[0] != '\0') ) {
    return true;
  } else {
    return false;
  }
}

void tokenizer_pushToken(Tokenizer *self, int line_num, int pos, Type type, char *value, State state)
{
  DEBUGP("pushToken: `%s`", value);
  self->currentToken->pos = pos;
  self->currentToken->line_num = line_num;
  self->currentToken->type = type;
  self->currentToken->value = (char *)mmrbc_alloc(sizeof(char) * (strlen(value) + 1));
  self->currentToken->value[0] = '\0';
  strsafecpy(self->currentToken->value, value, strlen(value) + 1);
  DEBUGP("Pushed token: `%s`", self->currentToken->value);
  self->currentToken->state = state;
  Token *newToken = Token_new();
  newToken->prev = self->currentToken;
  self->currentToken->next = newToken;
  self->currentToken = newToken;
}

int Tokenizer_advance(Tokenizer* const self, bool recursive)
{
  DEBUGP("Aadvance. mode: `%d`", self->mode);
  Token_GC(self->currentToken->prev);
  Token *lazyToken = Token_new();
  char value[MAX_TOKEN_LENGTH];
  char c[3];
  int cmd_state;
  cmd_state = self->cmd_start;
  self->cmd_start = false;
retry:
  memset(value, '\0', MAX_TOKEN_LENGTH);
  Type type = ON_NONE;
  memset(c, '\0', sizeof(c));

  RegexResult regexResult[REGEX_MAX_RESULT_NUM];

  tokenizer_readLine(self);
  if (self->line[0] == '\0') {
    Token_free(lazyToken);
    return -1;
  }
  if (self->mode == MODE_COMMENT) {
    if (Regex_match2(self->line, "^=end$") || Regex_match2(self->line, "^=end\\s")) {
      type = EMBDOC_END;
    } else {
      type = EMBDOC;
    }
    self->mode = MODE_NONE;
    strsafecpy(value, self->line, MAX_TOKEN_LENGTH);
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
        lazyToken->type = STRING_END;
        lazyToken->value = (char *)mmrbc_alloc(sizeof(char) *(2));
        *(lazyToken->value) = self->modeTerminater;
        *(lazyToken->value + 1) = '\0';
        lazyToken->state = EXPR_END;
        self->pos++;
        self->mode = MODE_NONE;
        break;
      } else if (self->line[self->pos] == ' ' || self->line[self->pos] == '\n') {
        Regex_match3(&(self->line[self->pos]), "^([\\s]+)", regexResult);
        strsafecpy(value, regexResult[0].value, MAX_TOKEN_LENGTH);
        type = WORDS_SEP;
      } else {
        int i = 0;
        for (;;) {
          c[0] = self->line[self->pos + i];
          c[1] = '\0';
          if (c[0] == '\0') break;
          if (c[0] != ' ' && c[0] != '\t' && c[0] != '\n' && c[0] != self->modeTerminater) {
            strsafecat(value, c, MAX_TOKEN_LENGTH);
            i++;
          } else {
            break;
          }
        }
        type = STRING_MID;
      }
      if (strlen(value) > 0) {
        if (type == WORDS_SEP && self->currentToken->prev->type == WORDS_SEP) {
          strsafecat(self->currentToken->prev->value, value, MAX_TOKEN_LENGTH);
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
      DEBUGP("modeTerminater: `%c`", self->modeTerminater);
      tokenizer_readLine(self);
      if (self->line[0] == '\0') {
        Token_free(lazyToken);
        return -1;
      }
      if (self->line[self->pos] == self->modeTerminater) {
        lazyToken->line_num = self->line_num;
        lazyToken->pos = self->pos;
        lazyToken->type = STRING_END;
        lazyToken->value = (char *)mmrbc_alloc(sizeof(char) *(2));
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
          STRING_MID,
          value,
          EXPR_BEG);
        value[0] = '\0';
        c[0] = '\0';
        tokenizer_pushToken(self,
          self->line_num,
          self->pos,
          EMBEXPR_BEG,
          "#{",
          EXPR_BEG);
        self->pos += 2;
        tokenizer_paren_stack_add(self, PAREN_BRACE);
        /**
         * Reserve some properties in order to revert them
         * after recursive calling
         */
        {
          Mode reserveMode = self->mode;
          State reserveState = self->state;
          char reserveModeTerminater = self->modeTerminater;
          bool reserveCmd_start = self->cmd_start;
          self->mode = MODE_NONE;
          self->state = EXPR_BEG;
          self->modeTerminater = '\0';
          self->cmd_start = true;
          while (Tokenizer_hasMoreTokens(self)) { /* recursive */
            if (Tokenizer_advance(self, false) == 1) break;
          }
          self->mode = reserveMode;
          self->state = reserveState;
          self->modeTerminater = reserveModeTerminater;
          self->cmd_start = reserveCmd_start;
        }
        tokenizer_pushToken(self,
          self->line_num,
          self->pos,
          EMBEXPR_END,
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
      strsafecat(value, c, MAX_TOKEN_LENGTH);
    }
    self->pos--;
    if (strlen(value) > 0) {
      type = STRING_MID;
    }
  } else if (self->mode == MODE_TSTRING_SINGLE) {
    for (;;) {
      tokenizer_readLine(self);
      if (self->line[0] == '\0') {
        Token_free(lazyToken);
        return -1;
      }
      if (self->line[self->pos] == '\'') {
        lazyToken->line_num = self->line_num;
        lazyToken->pos = self->pos;
        lazyToken->type = STRING_END;
        lazyToken->value = (char *)mmrbc_alloc(sizeof(char) *(2));
        *(lazyToken->value) = '\'';
        *(lazyToken->value + 1) = '\0';
        lazyToken->state = EXPR_END;
        self->pos++;
        self->mode = MODE_NONE;
        break;
      } else {
        c[0] = self->line[self->pos];
      }
      self->pos += strlen(c);
      strsafecat(value, c, MAX_TOKEN_LENGTH);
    }
    self->pos--;
    if (strlen(value) > 0) type = STRING_MID;
  } else if (Regex_match2(self->line, "^=begin$") || Regex_match2(self->line, "^=begin\\s")) { // multi lines comment began
    self->mode = MODE_COMMENT;
    strsafecpy(value, strsafecat(self->line, "\n", MAX_TOKEN_LENGTH), MAX_TOKEN_LENGTH);
    type = EMBDOC_BEG;
  } else if (self->line[self->pos] == '\n') {
    value[0] = '\n';
    value[1] = '\0';
    type = NL;
  } else if (self->line[self->pos] == '\r' && self->line[self->pos + 1] == '\n') {
    value[0] = '\r';
    value[1] = '\n';
    value[2] = '\0';
    type = NL;
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
    switch (value[0]) {
      case '*':
        switch (value[1]) {
          case '*':
            type = POW;
            break;
          default:
            FATALP("error");
        }
        break;
      default:
        FATALP("error");
        break;
    }
  } else if (Regex_match3(&(self->line[self->pos]), "^(@\\w+)", regexResult)) {
    strsafecpy(value, regexResult[0].value, MAX_TOKEN_LENGTH);
    type = IVAR;
  } else if (Regex_match3(&(self->line[self->pos]), "^(\\$\\w+)", regexResult)) {
    strsafecpy(value, regexResult[0].value, MAX_TOKEN_LENGTH);
    type = GVAR;
  } else if (Regex_match3(&(self->line[self->pos]), "^(\\?.)", regexResult)) {
    strsafecpy(value, regexResult[0].value, MAX_TOKEN_LENGTH);
    type = CHAR;
  } else if (self->line[self->pos] == '-' && self->line[self->pos + 1] == '>') {
    value[0] = '-';
    value[1] = '>';
    value[2] = '\0';
    type = TLAMBDA;
  } else {
    if (self->line[self->pos] == '\\') {
      // ignore
      self->pos++;
    } else if (self->line[self->pos] == ':') {
      if (Regex_match2(&(self->line[self->pos]), "^:[A-Za-z0-9]?")) {
        value[0] = ':';
        value[1] = '\0';
        type = SYMBEG;
        self->state = EXPR_FNAME;
      } else {
        // nothing TODO?
      }
    } else if (self->line[self->pos] == '#') {
      strsafecpy(value, &(self->line[self->pos]), MAX_TOKEN_LENGTH);
      type = COMMENT;
    } else if (self->line[self->pos] == ' ' || self->line[self->pos] == '\t') {
      Regex_match3(&(self->line[self->pos]), "^(\\s+)", regexResult);
      strsafecpy(value, regexResult[0].value, MAX_TOKEN_LENGTH);
      type = ON_SP;
    } else if (tokenizer_is_paren(self->line[self->pos])) {
      value[0] = self->line[self->pos];
      value[1] = '\0';
      switch (value[0]) {
        case '(':
//          if (IS_BEG()) {
            type = LPAREN;
//          } else {
//            type = LPAREN_ARG;
//          }
          self->state = EXPR_BEG;
          tokenizer_paren_stack_add(self, PAREN_PAREN);
          break;
        case ')':
          type = RPAREN;
          self->state = EXPR_ENDFN;
          tokenizer_paren_stack_pop(self);
          break;
        case '[':
          if ( IS_BEG() || (IS_ARG() && self->line[self->pos - 1] == ' ') ) {
            type = LBRACKET_ARRAY;
          } else {
            type = LBRACKET;
          }
          self->state = EXPR_BEG|EXPR_LABEL;
          break;
        case ']':
          type = RBRACKET;
          self->state = EXPR_END;
          break;
        case '{':
          type = LBRACE;
          self->state = EXPR_BEG|EXPR_LABEL;
          break;
        case '}':
          if (self->paren_stack[self->paren_stack_num] == PAREN_BRACE) {
            tokenizer_paren_stack_pop(self);
            Token_free(lazyToken);
            return 1;
          }
          type = RBRACE;
          break;
        default:
          ERRORP("unknown paren error");
      }
    } else if (tokenizer_is_operator(&(self->line[self->pos]), 1)) {
      if (Regex_match3(&(self->line[self->pos]), "^(%[iIwWq][]-~!@#$%^&*()_=+\[{};:'\"?])", regexResult)) {
        strsafecpy(value, regexResult[0].value, MAX_TOKEN_LENGTH);
        switch (value[1]) {
          case 'w':
            type = QWORDS_BEG;
            self->mode = MODE_QWORDS;
            break;
          case 'W':
            type = WORDS_BEG;
            self->mode = MODE_WORDS;
            break;
          case 'q':
            type = STRING_BEG;
            self->mode = MODE_TSTRING_SINGLE;
            break;
          case 'Q':
            type = STRING_BEG;
            self->mode = MODE_TSTRING_DOUBLE;
            break;
          case 'i':
            type = QSYMBOLS_BEG;
            self->mode = MODE_QSYMBOLS;
            break;
          case 'I':
            type = SYMBOLS_BEG;
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
      } else {
        value[0] = self->line[self->pos];
        value[1] = '\0';
        if (self->line[self->pos] == '-' && IS_NUM(1)) {
          type = UMINUS_NUM;
        } else {
          switch (value[0]) {
            case '=':
              type = E;
              self->state = EXPR_BEG;
              break;
            case '+':
              type = PLUS;
              break;
            case '-':
              type = MINUS;
              break;
            case '*':
              type = TIMES;
              break;
            case '/':
              type = DIVIDE;
              break;
            case '%':
              type = SURPLUS;
              break;
            default:
              type = ON_OP;
          }
        }
      }
    } else if (tokenizer_is_semicolon(self->line[self->pos])) {
      value[0] = self->line[self->pos];
      type = SEMICOLON;
      self->state = EXPR_BEG;
    } else if (tokenizer_is_comma(self->line[self->pos])) {
      value[0] = self->line[self->pos];
      type = COMMA;
      self->state = EXPR_BEG|EXPR_LABEL;
    } else if (IS_NUM(0)) {
      self->state = EXPR_END;
      if (Regex_match3(&(self->line[self->pos]), "^([0-9_]+\\.[0-9][0-9_]*)", regexResult)) {
        strsafecpy(value, regexResult[0].value, MAX_TOKEN_LENGTH);
        if (strchr(value, (int)'.')[-1] == '_') {
          ERRORP("trailing `_' in number");
          return -1;
        }
        type = FLOAT;
      } else if (Regex_match3(&(self->line[self->pos]), "^([0-9_]+)", regexResult)) {
        strsafecpy(value, regexResult[0].value, MAX_TOKEN_LENGTH);
        type = INTEGER;
      } else {
        ERRORP("Failed to tokenize a number");
        return -1;
      }
      if (value[strlen(value) - 1] == '_') {
        ERRORP("trailing `_' in number");
        return -1;
      }
    } else if (self->line[self->pos] == '.') {
      value[0] = '.';
      value[1] = '\0';
      type = PERIOD;
      self->state = EXPR_DOT;
    } else if (Regex_match2(&(self->line[self->pos]), "^\\w")) {
      if (Regex_match3(&(self->line[self->pos]), "^([A-Za-z0-9_?!]+:)", regexResult)) {
        strsafecpy(value, regexResult[0].value, MAX_TOKEN_LENGTH);
        type = LABEL;
      } else if (Regex_match3(&(self->line[self->pos]), "^([A-Z]\\w*[!?])", regexResult)) {
        strsafecpy(value, regexResult[0].value, MAX_TOKEN_LENGTH);
        type = IDENTIFIER;
      } else if (Regex_match3(&(self->line[self->pos]), "^([A-Z]\\w*)", regexResult)) {
        strsafecpy(value, regexResult[0].value, MAX_TOKEN_LENGTH);
        type = CONSTANT;
      } else if (Regex_match3(&(self->line[self->pos]), "^(\\w+[!?]?)", regexResult)) {
        strsafecpy(value, regexResult[0].value, MAX_TOKEN_LENGTH);
        type = IDENTIFIER;
      } else {
        ERRORP("Failed to tokenize!");
        Token_free(lazyToken);
        return 1;
      }
    } else if (self->line[self->pos] == '"') {
      value[0] = '"';
      value[1] = '\0';
      self->mode = MODE_TSTRING_DOUBLE;
      self->modeTerminater = '"';
      type = STRING_BEG;
    } else if (self->line[self->pos] == '\'') {
      value[0] = '\'';
      value[1] = '\0';
      self->mode = MODE_TSTRING_SINGLE;
      self->modeTerminater = '\'';
      type = STRING_BEG;
    } else {
      ERRORP("Failed to tokenize!");
      Token_free(lazyToken);
      self->pos += 1; /* skip one */
      return 1;
    }
  }
  if (lazyToken->value == NULL) {
    self->pos += strlen(value);
  }
  if (type == NL) {
    switch (self->state) {
      case EXPR_BEG:
      case EXPR_FNAME:
      case EXPR_DOT:
      case EXPR_CLASS:
        goto retry;
      default:
        break;
    }
    self->state = EXPR_BEG;
  } else if (type != ON_NONE) {
    /* FIXME from here */
    int8_t kw_num = keyword(value);
    if ( kw_num > 0 ) {
      type = (uint8_t)kw_num;
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
      } else {
        self->state = EXPR_END;
      }
    } else if (type == IDENTIFIER) {
      switch (self->state) {
        case EXPR_CLASS:
          self->state = EXPR_ARG;
          break;
        case EXPR_FNAME:
          self->state = EXPR_ENDFN;
          break;
        default:
          if (IS_BEG() || self->state == EXPR_DOT || IS_ARG()) {
            if (cmd_state) {
              self->state = EXPR_CMDARG;
            }
            else {
              self->state = EXPR_ARG;
            }
          }
          else if (self->state == EXPR_FNAME) {
            self->state = EXPR_ENDFN;
          }
          else {
            self->state = EXPR_END;
          }
          break;
      }
    }
    DEBUGP("value len: %ld, `%s`", strlen(value), value);
    tokenizer_pushToken(self,
      self->line_num,
      self->pos - strlen(value),
      type,
      value,
      self->state);
  }
  if (lazyToken->value != NULL) {
    tokenizer_pushToken(self,
      lazyToken->line_num,
      lazyToken->pos,
      lazyToken->type,
      lazyToken->value,
      lazyToken->state);
    self->pos++;
  }
  Token_free(lazyToken);
  return 0;
}

