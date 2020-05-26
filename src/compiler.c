#include <stdbool.h>

#include "../src/ruby-lemon-parse/parse.c"
#include "../src/ruby-lemon-parse/token_helper.h"
#include "common.h"
#include "compiler.h"
#include "debug.h"
#include "generator.h"
#include "scope.h"
#include "stream.h"
#include "token.h"
#include "tokenizer.h"
#include "tokenizer_helper.h"

#ifdef MMRBC_DEBUG
#define ZERO     "\e[30;1m"
#define CTRL     "\e[35;1m"
#define LETTER   "\e[36m"
#define EIGHTBIT "\e[33m"
#define DUMP_LINE_LEN 210
void dumpCode(Scope *scope)
{
  char line[DUMP_LINE_LEN];
  memset(line, '\0', DUMP_LINE_LEN);
  int linelen = 0;
  int c, i, j;
  for (i = 0; i < scope->vm_code_size; i++) {
    c = scope->vm_code[i];
    if (linelen == 8) strsafecat(line, "|", DUMP_LINE_LEN);
    if (i != 0) {
      if (i % 16 == 0) {
        printf(" %s\n", line);
        linelen = 0;
        memset(line, '\0', 18);
      } else if (i % 8 == 0) {
        printf("| ");
      }
    }
    if (c == 0) {
      printf(ZERO);
      strsafecat(line, ZERO, DUMP_LINE_LEN);
      strsafecat(line, "0", DUMP_LINE_LEN);
    } else if (c < 0x20) {
      printf(CTRL);
      strsafecat(line, CTRL, DUMP_LINE_LEN);
      strsafecat(line, "\uFFED", DUMP_LINE_LEN);
    } else if (c < 0x7f) {
      printf(LETTER);
      strsafecat(line, LETTER, DUMP_LINE_LEN);
      strsafecat(line, &c, DUMP_LINE_LEN);
    } else {
      printf(EIGHTBIT);
      strsafecat(line, EIGHTBIT, DUMP_LINE_LEN);
      strsafecat(line, "\uFFED", DUMP_LINE_LEN);
    }
    strsafecat(line, "\e[m", DUMP_LINE_LEN);
    linelen++;
    printf("%02x\e[m ", c);
  }
  if (linelen != 16) {
    if (i % 16 < 9) printf("  "); /* equiv size to "| " */
    for (j = i % 16; j < 16; j++) printf("   ");
  }
  printf(" %s\n", line);
}
#endif

bool Compile(Scope *scope, StreamInterface *si)
{
  Tokenizer *tokenizer = Tokenizer_new(si);
  Token *topToken = tokenizer->currentToken;
  ParserState *p = ParseInitState();
  yyParser *parser = ParseAlloc(mmrbc_alloc, p);
  while( Tokenizer_hasMoreTokens(tokenizer) ) {
    if (Tokenizer_advance(tokenizer, false) != 0) break;
    for (;;) {
      if (topToken->value == NULL) {
        DEBUGP("(main)%p null", topToken);
      } else {
        if (topToken->type != ON_SP) {
          INFOP("\n\e[32;40;1m%s\e[m len=%ld line=%d pos=%d \e[35;40;1m%s\e[m `\e[31;40;1m%s\e[m` \e[36;40;1m%s\e[m",
             tokenizer_mode_name(tokenizer->mode),
             strlen(topToken->value),
             topToken->line_num,
             topToken->pos,
             token_name(topToken->type),
             topToken->value,
             tokenizer_state_name(topToken->state));
          LiteralStore *ls = ParsePushLiteralStore(p, topToken->value);
          Parse(parser, topToken->type, ls->str);
        }
      }
      if (topToken->next == NULL) {
        break;
      } else {
        topToken->refCount--;
        topToken = topToken->next;
      }
    }
  }
  Parse(parser, 0, "");
  bool success;
  if (p->error_count == 0) {
   success = true;
#ifdef MMRBC_DEBUG
    ParseShowAllNode(parser, 1);
#endif
    Generator_generate(scope, p->root);
  } else {
    success = false;
  }
  if (success) {
    /* FIXME skipping FreeNode causes memory leak */
    ParseFreeAllNode(parser);
  }
  ParserStateFree(p);
  ParseFree(parser, mmrbc_free);
  Tokenizer_free(tokenizer);
#ifdef MMRBC_DEBUG
  dumpCode(scope);
#endif
  return success;
}
