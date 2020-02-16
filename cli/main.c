#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include "../src/mmrbc.h"

typedef struct {
  int verbose;
} opt_t;

int handle_opt(const char **argv, opt_t* opt)
{
  if (!strcmp(*argv, "-v") || !strcmp(*argv, "--version")) {
    printf("mini mruby compiler %s\n", MMRBC_VERSION);
    return 0;
  } else if (!strcmp(*argv, "--verbose")) {
    opt->verbose = 1;
  }
  return -1;
}

int main(int argc, const char * argv[])
{
  opt_t opt;
  opt.verbose = 0;

  if(!*++argv) {
    printf( "mmrbc: no program file given\n" );
    return 1;
  }

  if(*argv[0] == '-') {
    int res = handle_opt(argv, &opt);
    if (res >= 0) {
      return res;
    } else {
    }
  }

  FILE *fp;
  if( (fp = fopen( *argv, "r" ) ) == NULL ) {
    fprintf( stderr, "mmrbc: cannot open program file. (%s)\n", *argv );
    return 1;
  } else {
    Tokenizer *tokenizer = Tokenizer_new(fp, PAREN_NONE, NULL);
    Token *topToken = tokenizer->currentToken;
    while( Tokenizer_hasMoreTokens(tokenizer) ) {
      Tokenizer_advance(tokenizer, false);
      for (;;) {
        if (topToken->value == NULL) {
           printf("(main1)%p null\n", topToken);
        } else {
           printf("(main1)%p value(len=%ld): `%s`\n", topToken, strlen(topToken->value), topToken->value);
        }
        if (topToken->next == NULL) {
          break;
        } else {
          topToken = topToken->next;
        }
      }
    }
    putchar('\n');
    fclose( fp );
    Tokenizer_free(tokenizer);
  }
  return 0;
}
