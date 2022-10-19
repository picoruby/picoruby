#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>

#include <mrubyc.h>

#include <picorbc.h>

#include "heap.h"

int loglevel;

void run(uint8_t *mrb)
{
  mrbc_tcb *tcb = mrbc_create_task(mrb, 0);
  mrbc_run();
  mrbc_raw_free(tcb);
}

static bool verbose = false;

int handle_opt(int argc, char * const *argv, char **oneliner)
{
  struct option longopts[] = {
    { "version",   no_argument,       NULL, 'v' },
    { "copyright", no_argument,       NULL, 'c' },
    { "verbose",   no_argument,       NULL, 'V' },
    { "help"   ,   no_argument,       NULL, 'h' },
    { "evaluate",  required_argument, NULL, 'e' },
    { "loglevel",  required_argument, NULL, 'l' },
    { NULL,        0,                 NULL,  0  }
  };
  int opt;
  int longindex;
  loglevel = LOGLEVEL_ERROR;
  while ((opt = getopt_long(argc, argv, "vcVhe:l:", longopts, &longindex)) != -1) {
    switch (opt) {
      case 'v':
        DEBUGP("v add: %p\n", optarg);
        fprintf(stdout, "PicoRuby %s", PICORBC_VERSION);
        #ifdef PICORUBY_DEBUG
          fprintf(stdout, " (debug build)");
        #endif
        fprintf(stdout, "\n");
        return -1;
      case 'e':
        *oneliner = optarg;
        break;
      case 'l':
        DEBUGP("l add: %p\n", optarg);
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
      case 'c':
        fprintf(stdout, "PicoRuby - Copyright (c) 2011-2022 HASUMI Hitoshi\n");
        return -1;
      case 'V':
        verbose = true;
        break;
      case 'h':
        fprintf(stdout, "Usage: (ToDo)\n");
        return -1;
      default:
        fprintf(stderr, "piroruby: invalid option (-h will show valid options)\n");
        return 1;
    }
  }
  return 0;
}

static uint8_t heap[HEAP_SIZE];

int main(int argc, char *argv[])
{
  mrbc_init(heap, HEAP_SIZE);

  char *oneliner = NULL;
  int ret = handle_opt(argc, argv, &oneliner);
  if (ret == -1) return 0;
  if (ret != 0) return ret;

  StreamInterface *si;
  FILE *fp = NULL;

  if (oneliner != NULL) {
    si = StreamInterface_new(NULL, oneliner, STREAM_TYPE_MEMORY);
  } else {
    if ( !argv[optind] ) {
      ERRORP("picoruby: no program file given");
      return 1;
    }
    fp = fopen(argv[optind], "rb");
    if (!fp) {
      FATALP("picoruby: failed to open file");
      return 1;
    }
    si = StreamInterface_new(fp, NULL, STREAM_TYPE_FILE);
    if (si == NULL) return 1;
  }
  ParserState *p = Compiler_parseInitState(si->node_box_size);
  p->verbose = verbose;
  if (Compiler_compile(p, si, NULL)) { /* TODO picorb_context */
    run(p->scope->vm_code);
  } else {
    ret = 1;
  }

  if (fp) fclose(fp);
  StreamInterface_free(si);
  Compiler_parserStateFree(p);
#ifdef PICORUBY_DEBUG
  memcheck();
#endif
  return ret;
}
