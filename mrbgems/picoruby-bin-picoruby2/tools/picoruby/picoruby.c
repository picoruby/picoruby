#ifdef picorb_NO_STDIO
# error picoruby-bin-picoruby conflicts 'picorb_NO_STDIO' in your build configuration
#endif

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <mrubyc.h>

#include <mrc_common.h>
#include <mrc_ccontext.h>
#include <mrc_compile.h>
#include <mrc_dump.h>

#if defined(_WIN32) || defined(_WIN64)
# include <io.h> /* for setmode */
# include <fcntl.h>
#endif

#ifdef _WIN32
#include <stdlib.h>
#include <malloc.h>
#include <windows.h>
char*
picorb_utf8_from_locale(const char *str, int len)
{
  wchar_t* wcsp;
  char* mbsp;
  int mbssize, wcssize;

  if (len == 0)
    return strdup("");
  if (len == -1)
    len = (int)strlen(str);
  wcssize = MultiByteToWideChar(GetACP(), 0, str, len,  NULL, 0);
  wcsp = (wchar_t*) malloc((wcssize + 1) * sizeof(wchar_t));
  if (!wcsp)
    return NULL;
  wcssize = MultiByteToWideChar(GetACP(), 0, str, len, wcsp, wcssize + 1);
  wcsp[wcssize] = 0;

  mbssize = WideCharToMultiByte(CP_UTF8, 0, (LPCWSTR) wcsp, -1, NULL, 0, NULL, NULL);
  mbsp = (char*) malloc((mbssize + 1));
  if (!mbsp) {
    free(wcsp);
    return NULL;
  }
  mbssize = WideCharToMultiByte(CP_UTF8, 0, (LPCWSTR) wcsp, -1, mbsp, mbssize, NULL, NULL);
  mbsp[mbssize] = 0;
  free(wcsp);
  return mbsp;
}
#define picorb_utf8_free(p) free(p)
#else
#define picorb_utf8_from_locale(p, l) ((char*)(p))
#define picorb_utf8_free(p)
#endif

#ifndef HEAP_SIZE
#define HEAP_SIZE (1024 * 64 - 1)
#endif
static uint8_t mrbc_heap[HEAP_SIZE];

#ifndef MAX_REGS_SIZE
#define MAX_REGS_SIZE 255
#endif

struct _args {
  char *cmdline;
  const char *fname;
  mrc_bool mrbfile      : 1;
  mrc_bool check_syntax : 1;
  mrc_bool verbose      : 1;
  mrc_bool version      : 1;
  mrc_bool debug        : 1;
  int argc;
  char **argv;
  int libc;
  char **libv;
};

struct options {
  int argc;
  char **argv;
  char *program;
  char *opt;
  char short_opt[2];
};

#define PICORUBY_VERSION "3.3.0"

static void
picorb_show_version(struct VM *vm)
{
  fprintf(stdout, "picoruby %s\n", PICORUBY_VERSION);
}

static void
picorb_show_copyright(struct VM *vm)
{
  picorb_show_version(vm);
  fprintf(stdout, "picoruby is a lightweight implementation of the Ruby language.\n");
  fprintf(stdout, "picoruby is based on mruby/c.\n");
  fprintf(stdout, "picoruby is released under the MIT License.\n");
}

static void
usage(const char *name)
{
  static const char *const usage_msg[] = {
  "switches:",
  "-b           load and execute RiteBinary (mrb) file",
  "-c           check syntax only",
  "-d           set debugging flags (set $DEBUG to true)",
  "-e 'command' one line of script",
  "-r library   load the library before executing your script",
  "-v           print version number, then run in verbose mode",
  "--verbose    run in verbose mode",
  "--version    print the version",
  "--copyright  print the copyright",
  NULL
  };
  const char *const *p = usage_msg;

  printf("Usage: %s [switches] [programfile] [arguments]\n", name);
  while (*p)
    printf("  %s\n", *p++);
}

static mrc_bool
picorb_extension_p(const char *path)
{
  const char *e = strrchr(path, '.');
  if (e && e[1] == 'm' && e[2] == 'r' && e[3] == 'b' && e[4] == '\0') {
    return TRUE;
  }
  return FALSE;
}

static void
options_init(struct options *opts, int argc, char **argv)
{
  opts->argc = argc;
  opts->argv = argv;
  opts->program = *argv;
  *opts->short_opt = 0;
}

static const char *
options_opt(struct options *opts)
{
  /* concatenated short options (e.g. `-cv`) */
  if (*opts->short_opt && *++opts->opt) {
   short_opt:
    opts->short_opt[0] = *opts->opt;
    opts->short_opt[1] = 0;
    return opts->short_opt;
  }

  while (++opts->argv, --opts->argc) {
    opts->opt = *opts->argv;

    /*  not start with `-`  || `-` */
    if (opts->opt[0] != '-' || !opts->opt[1]) return NULL;

    if (opts->opt[1] == '-') {
      /* `--` */
      if (!opts->opt[2]) {
        opts->argv++, opts->argc--;
        return NULL;
      }
      /* long option */
      opts->opt += 2;
      *opts->short_opt = 0;
      return opts->opt;
    }
    else {
      /* short option */
      opts->opt++;
      goto short_opt;
    }
  }
  return NULL;
}

static const char *
options_arg(struct options *opts)
{
  if (*opts->short_opt && opts->opt[1]) {
    /* concatenated short option and option argument (e.g. `-rLIBRARY`) */
    *opts->short_opt = 0;
    return opts->opt + 1;
  }
  --opts->argc, ++opts->argv;
  return opts->argc ? *opts->argv : NULL;
}

static char *
dup_arg_item(const char *item)
{
  size_t buflen = strlen(item) + 1;
  char *buf = (char*)mrbc_raw_alloc(buflen);
  memcpy(buf, item, buflen);
  return buf;
}

static int
parse_args(struct VM *vm, int argc, char **argv, struct _args *args)
{
  static const struct _args args_zero = { 0 };
  struct options opts[1];
  const char *opt, *item;

  *args = args_zero;
  options_init(opts, argc, argv);
  while ((opt = options_opt(opts))) {
    if (strcmp(opt, "b") == 0) {
      args->mrbfile = TRUE;
    }
    else if (strcmp(opt, "c") == 0) {
      args->check_syntax = TRUE;
    }
    else if (strcmp(opt, "d") == 0) {
      args->debug = TRUE;
    }
    else if (strcmp(opt, "e") == 0) {
      if ((item = options_arg(opts))) {
        if (!args->cmdline) {
          args->cmdline = dup_arg_item(item);
        }
        else {
          size_t cmdlinelen;
          size_t itemlen;

          cmdlinelen = strlen(args->cmdline);
          itemlen = strlen(item);
          args->cmdline = (char*)mrbc_raw_realloc(args->cmdline, cmdlinelen + itemlen + 2);
          args->cmdline[cmdlinelen] = '\n';
          memcpy(args->cmdline + cmdlinelen + 1, item, itemlen + 1);
        }
      }
      else {
        fprintf(stderr, "%s: No code specified for -e\n", opts->program);
        return EXIT_FAILURE;
      }
    }
    else if (strcmp(opt, "h") == 0) {
      usage(opts->program);
      exit(EXIT_SUCCESS);
    }
    else if (strcmp(opt, "r") == 0) {
      if ((item = options_arg(opts))) {
        if (args->libc == 0) {
          args->libv = (char**)mrbc_raw_alloc(sizeof(char*));
        }
        else {
          args->libv = (char**)mrbc_raw_realloc(args->libv, sizeof(char*) * (args->libc + 1));
        }
        args->libv[args->libc++] = dup_arg_item(item);
      }
      else {
        fprintf(stderr, "%s: No library specified for -r\n", opts->program);
        return EXIT_FAILURE;
      }
    }
    else if (strcmp(opt, "v") == 0) {
      if (!args->verbose) {
        picorb_show_version(vm);
        args->version = TRUE;
      }
      args->verbose = TRUE;
    }
    else if (strcmp(opt, "version") == 0) {
      picorb_show_version(vm);
      exit(EXIT_SUCCESS);
    }
    else if (strcmp(opt, "verbose") == 0) {
      args->verbose = TRUE;
    }
    else if (strcmp(opt, "copyright") == 0) {
      picorb_show_copyright(vm);
      exit(EXIT_SUCCESS);
    }
    else {
      fprintf(stderr, "%s: invalid option %s%s (-h will show valid options)\n",
              opts->program, opt[1] ? "--" : "-", opt);
      return EXIT_FAILURE;
    }
  }

  argc = opts->argc; argv = opts->argv;
  if (args->cmdline == NULL) {
    if (*argv == NULL) {
      if (args->version) exit(EXIT_SUCCESS);
      args->cmdline = (char *)"-";
      args->fname = "-";
    }
    else {
      args->fname = *argv;
      args->cmdline = argv[0];
      argc--; argv++;
    }
  }
  args->argv = (char **)mrbc_raw_realloc(args->argv, sizeof(char*) * (argc + 1));
  memcpy(args->argv, argv, (argc+1) * sizeof(char*));
  args->argc = argc;

  return EXIT_SUCCESS;
}

static void
cleanup(struct _args *args)
{
  if (!args->fname)
    mrbc_raw_free(args->cmdline);
  mrbc_raw_free(args->argv);
  if (args->libc) {
    while (args->libc--) {
      mrbc_raw_free(args->libv[args->libc]);
    }
    mrbc_raw_free(args->libv);
  }
//  mrb_close(mrb);
}

static mrc_bool
picorb_undef_p(mrbc_value v)
{
  return v.tt == MRBC_TT_EMPTY;
}

static void
picorb_vm_init()
{
  hal_init();
  mrbc_init_alloc(mrbc_heap, HEAP_SIZE);
  mrbc_init_global();
  mrbc_init_class();
}

static void
picorb_run(mrc_ccontext *c, mrbc_vm *vm, mrc_irep *irep)
{
  uint8_t *bin = NULL;
  size_t bin_size = 0;
  int result;

  result = mrc_dump_irep(c, irep, 0, &bin, &bin_size);
  if (result != MRC_DUMP_OK) {
    fprintf(stderr, "irep dump error: %d\n", result);
    return;
  }
  if (mrbc_load_mrb(vm, bin) != 0) {
    fprintf(stderr, "mrbc_load_mrb failed\n");
    return;
  }
  mrbc_vm_begin(vm);
  mrbc_vm_run(vm);
  mrbc_raw_free(bin);
  mrbc_vm_end(vm);
}

static mrc_irep *
picorb_load_detect_file_cxt(mrc_ccontext *c, const char *fname, uint8_t **source)
{
  char **filenames = (char**)mrbc_raw_alloc(sizeof(char*) * 2);
  filenames[0] = (char *)fname;
  filenames[1] = NULL;
  mrc_irep *irep = mrc_load_file_cxt(c, (const char **)filenames, source);
  if (irep == NULL) {
    fprintf(stderr, "irep load error\n");
  }
  return irep;
}

int
main(int argc, char **argv)
{
  picorb_vm_init();
  mrbc_vm *vm = mrbc_vm_open(NULL);
  if(vm == NULL) {
    fprintf(stderr, "%s: Invalid mrbc_vm, exiting picoruby\n", *argv);
    return EXIT_FAILURE;
  }

  int n = -1;
  struct _args args;
  mrbc_value ARGV;
  mrc_irep *irep = NULL;

  n = parse_args(vm, argc, argv, &args);
  if (n == EXIT_FAILURE || (args.cmdline == NULL)) {
    cleanup(&args);
    return n;
  }
  else {
//    int ai = mrb_gc_arena_save(mrb);
    ARGV = mrbc_array_new(vm, args.argc);
    for (int i = 0; i < args.argc; i++) {
      char* utf8 = picorb_utf8_from_locale(args.argv[i], -1);
      if (utf8) {
        mrbc_value str = mrbc_string_new(vm, utf8, strlen(utf8));
        mrbc_array_push(&ARGV, &str);
        picorb_utf8_free(utf8);
      }
    }
    mrbc_set_const(mrbc_str_to_symid("ARGV"), &ARGV);
    mrbc_value debug = mrbc_bool_value(args.debug);
    mrbc_set_global(mrbc_str_to_symid("$DEBUG"), &debug);

    mrc_ccontext *c = mrc_ccontext_new(NULL);
    if (args.verbose)
      c->dump_result = TRUE;
    if (args.check_syntax)
      c->no_exec = TRUE;

    /* Set $0 */
    const char *cmdline;
    if (args.fname) {
      cmdline = args.cmdline ? args.cmdline : "-";
    }
    else {
      cmdline = "-e";
    }
    mrbc_value cmd = mrbc_string_new(vm, cmdline, strlen(cmdline));
    mrbc_set_global(mrbc_str_to_symid("$0"), &cmd);

    uint8_t *source = NULL;

    /* Load libraries */
    for (int i = 0; i < args.libc; i++) {
      mrc_ccontext_filename(c, args.libv[i]);
      if (picorb_extension_p(args.libv[i])) {
//        irep = picorb_load_irep_file_cxt(c, args.libv[i]);
      }
      else {
        source = NULL;
        irep = picorb_load_detect_file_cxt(c, args.libv[i], &source);
      }
      if (irep) {
        picorb_run(c, vm, irep);
        mrc_irep_free(c, irep);
        if (source) mrc_free(source);
      }
//      mrb_vm_ci_env_clear(mrb, mrb->c->cibase);
//      mrb_ccontext_cleanup_local_variables(mrb, c);
    }

    /* set program file name */
    mrc_ccontext_filename(c, cmdline);

    /* Load program */
    if (args.mrbfile || picorb_extension_p(cmdline)) {
//      irep = picorb_load_irep_file_cxt(c, args.fname);
    }
    else if (args.fname) {
      source = NULL;
      irep = picorb_load_detect_file_cxt(c, args.fname, &source);
    }
    else {
      char* utf8 = picorb_utf8_from_locale(args.cmdline, -1);
      if (!utf8) abort();
      irep = mrc_load_string_cxt(c, (const uint8_t **)&utf8, strlen(utf8));
      picorb_utf8_free(utf8);
    }

    if (irep) {
      picorb_run(c, vm, irep);
      mrc_irep_free(c, irep);
      if (source) mrc_free(source);
    }
    mrc_ccontext_free(c);
//    mrb_gc_arena_restore(mrb, ai);
    if (vm->exception.tt != MRBC_TT_NIL) {
      //MRB_EXC_CHECK_EXIT(mrb, mrb->exc);
      // TODO
      //if (!picorb_undef_p(v)) {
      //  picorb_print_error(vm);
      //}
      n = EXIT_FAILURE;
    }
    else if (args.check_syntax) {
      puts("Syntax OK");
    }
  }
  cleanup(&args);
  mrbc_vm_close(vm);

  return n;
}
