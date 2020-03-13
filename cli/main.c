#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <getopt.h>

#include "../src/mrubyc/src/mrubyc.h"

#include "../src/mmrbc.h"
#include "../src/common.h"

#include "../src/ruby-lemon-parse/parse.c"

int handle_opt(int argc, char * const *argv)
{
  struct option longopts[] = {
    { "version",  no_argument,       NULL, 'v' },
    { "debug",    no_argument,       NULL, 'd' },
    { "verbose",  no_argument,       NULL, 'b' },
    { "loglevel", required_argument, NULL, 'l' },
    { 0,          0,                 0,     0  }
  };
  int opt;
  int longindex;
  loglevel = LOGLEVEL_INFO;
  while ((opt = getopt_long(argc, argv, "vdbl:", longopts, &longindex)) != -1) {
    switch (opt) {
      case 'v':
        fprintf(stdout, "mini mruby compiler %s\n", MMRBC_VERSION);
        return -1;
      case 'b': /* verbose */
        /* TODO */
        break;
      case 'd': /* debug */
        #ifndef DEBUG_BUILD
          fprintf(stderr, "[ERROR] `--debug` option is only valid if you did `make` with CFLAGS=-DDEBUG_BUILD\n");
          return 1;
        #endif
        loglevel = LOGLEVEL_DEBUG;
        break;
      case 'l':
        #ifndef DEBUG_BUILD
          fprintf(stderr, "[ERROR] `--loglevel=[level]` option is only valid if you made executable with -DDEBUG_BUILD\n");
          return 1;
        #endif
        if ( !strcmp(optarg, "debug") ) { loglevel = LOGLEVEL_DEBUG; } else
        if ( !strcmp(optarg, "info") )  { loglevel = LOGLEVEL_INFO; } else
        if ( !strcmp(optarg, "warn") )  { loglevel = LOGLEVEL_WARN; } else
        if ( !strcmp(optarg, "error") ) { loglevel = LOGLEVEL_ERROR; } else
        if ( !strcmp(optarg, "fatal") ) { loglevel = LOGLEVEL_FATAL; } else
        {
          fprintf(stderr, "Invalid loglevel option: %s\n", optarg);
          return 1;
        }
        break;
      default:
        fprintf(stderr, "error! \'%c\' \'%c\'\n", opt, optopt);
        return 1;
    }
  }
  return 0;
}

#define MEMORY_SIZE (1024*64-1)
static uint8_t memory_pool[MEMORY_SIZE];

int main(int argc, char * const *argv)
{
  int ret = handle_opt(argc, argv);
  if (ret != 0) return ret;

  if ( !argv[optind] ) {
    ERROR("mmrbc: no program file given");
    return 1;
  }

  mrbc_init(memory_pool, MEMORY_SIZE);
  MrbCode *mrb;

  FILE *fp;
  if( (fp = fopen( argv[optind], "r" ) ) == NULL ) {
    FATAL("mmrbc: cannot open program file. (%s)", *argv);
    return 1;
  } else {
    Tokenizer *tokenizer = Tokenizer_new(fp);
    Token *topToken = tokenizer->currentToken;
    ParserState *p = ParseInitState();
    yyParser *parser = ParseAlloc(mmrbc_alloc, p);
    while( Tokenizer_hasMoreTokens(tokenizer) ) {
      Tokenizer_advance(tokenizer, false);
      for (;;) {
        if (topToken->value == NULL) {
          DEBUG("(main)%p null", topToken);
        } else {
          if (topToken->type != ON_SP) {
            INFO("Token found: (mode=%d) (len=%ld,line=%d,pos=%d) type=%d `%s`",
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
    fclose( fp );
    Parse(parser, 0, "");
    ParseShowAllNode(parser, 1);
    mrb = Generator_generate(p->root);
    ParseFreeAllNode(parser);
    ParseFreeState(parser);
    ParseFree(parser, mmrbc_free);
    Tokenizer_free(tokenizer);
  }
  print_allocs();
  for (int i=0; i < mrb->size; i++){
    if (i % 16 == 0) printf("\n");
    printf("%02x ", mrb->body[i]);
  }
  printf("\n");
  return 0;
}
