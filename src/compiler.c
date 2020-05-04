#include <stdbool.h>

#include "../src/ruby-lemon-parse/parse.c"
#include "common.h"
#include "compiler.h"
#include "debug.h"
#include "generator.h"
#include "scope.h"
#include "stream.h"
#include "token.h"
#include "tokenizer.h"

bool Compile(Scope *scope, StreamInterface *si)
{
  Tokenizer *tokenizer = Tokenizer_new(si);
  Token *topToken = tokenizer->currentToken;
  ParserState *p = ParseInitState();
  yyParser *parser = ParseAlloc(mmrbc_alloc, p);
  while( Tokenizer_hasMoreTokens(tokenizer) ) {
    Tokenizer_advance(tokenizer, false);
    for (;;) {
      if (topToken->value == NULL) {
        DEBUGP("(main)%p null", topToken);
      } else {
        if (topToken->type != ON_SP) {
          INFOP("Token found: (mode=%d) (len=%ld,line=%d,pos=%d) type=%d `%s`",
             tokenizer->mode,
             strlen(topToken->value),
             topToken->line_num,
             topToken->pos,
             topToken->type,
             topToken->value);
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
  return success;
}
