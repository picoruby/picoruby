#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include "../src/mmrbc.h"
#include "../src/tokenizer.h"

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

#define MAX_LINE_LENGTH 256

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
    Tokenizer tokenizer;
    Tokenizer_new(&tokenizer);
    char line[MAX_LINE_LENGTH];
    while( fgets(line, MAX_LINE_LENGTH, fp)  != NULL ) {
      Tokenizer_puts(&tokenizer, line);
      Tokenizer_advance(&tokenizer, false);
    }
    putchar('\n');
    fclose( fp );
  }
  return 0;
}
