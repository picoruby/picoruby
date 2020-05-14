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
  return success;
}
