#ifdef picorb_NO_STDIO
# error picoruby-bin-picoruby conflicts 'picorb_NO_STDIO' in your build configuration
#endif

#include "picoruby.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#if defined(PICORB_VM_MRUBY)
# define EXECUTABLE_NAME "microruby"
# define VM_NAME "mruby"

struct RProc* read_irep(mrb_state *mrb, const uint8_t *bin, size_t bufsize, uint8_t flags);

mrb_value mrb_load_irep_file(mrb_state *mrb, FILE* fp);

void
mrb_mruby_compiler2_gem_init(mrb_state *mrb)
{
}

void
mrb_mruby_compiler2_gem_final(mrb_state *mrb)
{
}

#elif defined(PICORB_VM_MRUBYC)

#define EXECUTABLE_NAME "picoruby"
#define VM_NAME "mruby/c"
#ifndef HEAP_SIZE
#define HEAP_SIZE (1024 * 6400 - 1)
#endif
static uint8_t mrbc_heap[HEAP_SIZE];
#ifndef MAX_REGS_SIZE
#define MAX_REGS_SIZE 255
#endif

#endif


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
  wcsp = (wchar_t*) picorb_alloc((wcssize + 1) * sizeof(wchar_t));
  if (!wcsp)
    return NULL;
  wcssize = MultiByteToWideChar(GetACP(), 0, str, len, wcsp, wcssize + 1);
  wcsp[wcssize] = 0;

  mbssize = WideCharToMultiByte(CP_UTF8, 0, (LPCWSTR) wcsp, -1, NULL, 0, NULL, NULL);
  mbsp = (char*) picorb_alloc((mbssize + 1));
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

struct _args {
  char *cmdline;
  const char *fname;
  picorb_bool mrbfile      : 1;
  picorb_bool check_syntax : 1;
  picorb_bool verbose      : 1;
  picorb_bool version      : 1;
  picorb_bool debug        : 1;
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

static void
picorb_show_version(void)
{
  fprintf(stdout, EXECUTABLE_NAME " %s\n", PICORUBY_VERSION);
}

static void
picorb_show_copyright(void)
{
  picorb_show_version();
  fprintf(stdout, EXECUTABLE_NAME " is a lightweight implementation of the Ruby language.\n");
  fprintf(stdout, EXECUTABLE_NAME " is based on " VM_NAME ".\n");
  fprintf(stdout, EXECUTABLE_NAME " is released under the MIT License.\n");
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
  printf("  programfile can be comma-linked multiple files like `a.rb,b.rb,c.rb`\n");
  while (*p)
    printf("  %s\n", *p++);
}

/*
 * In order to be recognized as a `.mrb` file, the following three points must be satisfied:
 * - File starts with "RITE"
 */
static picorb_bool
picorb_mrb_p(const char *path)
{
  FILE *fp;
  char buf[4];
  picorb_bool ret = FALSE;

  if ((fp = fopen(path, "rb")) == NULL) {
    return FALSE;
  }
  if (fread(buf, 1, sizeof(buf), fp) == sizeof(buf)) {
    if (memcmp(buf, "RITE", 4) == 0) {
      ret = TRUE;
    }
  }
  fclose(fp);
  return ret;
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
  char *buf = (char*)picorb_alloc(buflen);
  memcpy(buf, item, buflen);
  return buf;
}

static int
parse_args(int argc, char **argv, struct _args *args)
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
          args->cmdline = (char*)picorb_realloc(args->cmdline, cmdlinelen + itemlen + 2);
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
          args->libv = (char**)picorb_alloc(sizeof(char*));
        }
        else {
          args->libv = (char**)picorb_realloc(args->libv, sizeof(char*) * (args->libc + 1));
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
        picorb_show_version();
        args->version = TRUE;
      }
      args->verbose = TRUE;
    }
    else if (strcmp(opt, "version") == 0) {
      picorb_show_version();
      exit(EXIT_SUCCESS);
    }
    else if (strcmp(opt, "verbose") == 0) {
      args->verbose = TRUE;
    }
    else if (strcmp(opt, "copyright") == 0) {
      picorb_show_copyright();
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
  args->argv = (char **)picorb_alloc(sizeof(char*) * (argc + 1));
  memcpy(args->argv, argv, (argc+1) * sizeof(char*));
  args->argc = argc;

  return EXIT_SUCCESS;
}

static void
cleanup(struct _args *args)
{
  if (!args->fname)
    picorb_free(args->cmdline);
  picorb_free(args->argv);
  if (args->libc) {
    while (args->libc--) {
      picorb_free(args->libv[args->libc]);
    }
    picorb_free(args->libv);
  }
//  mrb_close(mrb);
}

//static picorb_bool
//picorb_undef_p(picorb_value *v)
//{
//  if (!v) return TRUE;
//  return v->tt == MRBC_TT_EMPTY;
//}

//static void
//picorb_vm_init(mrbc_vm *vm)
//{
//  mrbc_init(mrbc_heap, HEAP_SIZE);
//  picoruby_init_require(vm);
//}

//static void
//lib_run(mrc_ccontext *c, mrbc_vm *vm, uint8_t *mrb)
//{
//  if (mrbc_load_mrb(vm, mrb) != 0) {
//    fprintf(stderr, "mrbc_load_mrb failed\n");
//    return;
//  }
//  mrbc_vm_begin(vm);
//  mrbc_vm_run(vm);
//  mrbc_vm_end(vm);
//}

static mrc_irep *
picorb_load_rb_file_cxt(mrc_ccontext *c, const char *fname, uint8_t **source)
{
  char *filenames[2];// = (char**)picorb_alloc(sizeof(char*) * 2);
  filenames[0] = (char *)fname;
  filenames[1] = NULL;
  mrc_irep *irep = mrc_load_file_cxt(c, (const char **)filenames, source);
  if (irep == NULL) {
    fprintf(stderr, "irep load error\n");
  }
  return irep;
}

static void
picorb_print_error(void)
{
  //TODO
}

static void
resolve_intern(mrb_state *mrb, pm_constant_pool_t *constant_pool, mrc_irep *irep)
{
  mrb_sym *new_syms = picorb_alloc(sizeof(mrb_sym) *irep->slen);
  for (int i = 0; i < irep->slen; i++) {
    mrc_sym sym = irep->syms[i];
    pm_constant_t *constant = pm_constant_pool_id_to_constant(constant_pool, sym);
    const char *lit = (const char *)constant->start;
    size_t len = constant->length;
    new_syms[i] = mrb_intern(mrb, lit, len);
  }
  free((mrc_sym *)irep->syms); // discard const
  irep->syms = new_syms;
  for (int i = 0; i < irep->rlen; i++) {
    resolve_intern(mrb, constant_pool, (mrc_irep *)irep->reps[i]);
  }
}

int
main(int argc, char **argv)
{
//  picorb_vm_init(NULL);

  mrb_state *mrb = mrb_open();

  int n = -1;
  struct _args args;
  picorb_value ARGV;
  mrc_irep *irep = NULL;

  n = parse_args(argc, argv, &args);
  if (n == EXIT_FAILURE || (args.cmdline == NULL)) {
    cleanup(&args);
    return n;
  }

//  int ai = mrb_gc_arena_save(mrb);
  ARGV = picorb_array_new(mrb, args.argc);
  for (int i = 0; i < args.argc; i++) {
    char* utf8 = picorb_utf8_from_locale(args.argv[i], -1);
    if (utf8) {
      picorb_value str = picorb_string_new(mrb, utf8, strlen(utf8));
      picorb_array_push(mrb, ARGV, str);
      picorb_utf8_free(utf8);
    }
  }
  picorb_define_const(mrb, "ARGV", ARGV);
  picorb_value debug = picorb_bool_value(args.debug);
  picorb_define_global_const(mrb, "$DEBUG", debug);

  /* Set $0 */
  const char *cmdline;
  if (args.fname) {
    cmdline = args.cmdline ? args.cmdline : "-";
  }
  else {
    cmdline = "-e";
  }
  picorb_value cmd = picorb_string_new(mrb, cmdline, strlen(cmdline));
  picorb_define_global_const(mrb, "$0", cmd);

  uint8_t *source = NULL;

//  mrbc_vm *lib_vm_list[args.libc];
  int lib_vm_list_size = 0;
  uint8_t *lib_mrb_list[args.libc];
  int lib_mrb_list_size = 0;

  /* Load libraries */
//  for (int i = 0; i < args.libc; i++) {
//    if (!picoruby_load_model_by_name(args.libv[i])) {
//      fprintf(stderr, "cannot load library: %s\n", args.libv[i]);
//      exit(EXIT_FAILURE);
//    }
//
//    mrc_ccontext *c = mrc_ccontext_new(NULL);
//    if (args.verbose) c->dump_result = TRUE;
//    if (args.check_syntax) c->no_exec = TRUE;
//    mrc_ccontext_filename(c, args.libv[i]);
//
//    irep = NULL;
//    uint8_t *mrb = NULL;
//
//    if (picorb_mrb_p(args.libv[i])) {
//      size_t size;
//      FILE *fp = fopen(args.libv[i], "rb");
//      if (fp == NULL) {
//        fprintf(stderr, "cannot open file: %s\n", args.libv[i]);
//        exit(EXIT_FAILURE);
//      }
//      fseek(fp, 0, SEEK_END);
//      size = ftell(fp);
//      fseek(fp, 0, SEEK_SET);
//      mrb = (uint8_t *)picorb_alloc(size);
//      if (fread(mrb, 1, size, fp) != size) {
//        fprintf(stderr, "cannot read file: %s\n", args.libv[i]);
//        exit(EXIT_FAILURE);
//      }
//      fclose(fp);
//    }
//    else {
//      source = NULL;
//      irep = picorb_load_rb_file_cxt(c, args.libv[i], &source);
//    }
//
//    if (irep) {
//      size_t mrb_size = 0;
//      int result;
//      result = mrc_dump_irep(c, irep, 0, &mrb, &mrb_size);
//      if (result != MRC_DUMP_OK) {
//        fprintf(stderr, "irep dump error: %d\n", result);
//        exit(EXIT_FAILURE);
//      }
//    }
//
//    if (mrb) {
//      lib_mrb_list[lib_mrb_list_size++] = mrb;
//      if (irep) mrc_irep_free(c, irep);
//      mrc_ccontext_free(c);
//      mrbc_vm *libvm = mrbc_vm_open(NULL);
////      lib_vm_list[lib_vm_list_size++] = libvm;
////      lib_run(c, libvm, mrb);
//    }
//    if (source) {
//      // TODO refactor
//      picorb_free(source);
//      source = NULL;
//    }
//  }

  /*
   * [tasks]
   * args.fname possibly looks like `a.rb,b.rb,c.rb` (comma separated file names)
   */
  int taskc = 1;
  if (args.fname) {
    for (int i = 0; args.fname[i]; i++) {
      if (args.fname[i] == ',') {
        taskc++;
      }
    }
  }
  else {
    /* -e option */
  }
  uint8_t *task_mrb_list[taskc];
  int tasks_mrb_list_size = 0;
//  mrbc_tcb *tcb_list[taskc];
  int tcb_list_size = 0;
  char *fnames[taskc];
  char *token = strtok((char *)cmdline, ",");
  int index = 0;
  while (token != NULL) {
    fnames[index] = picorb_alloc(strlen(token) + 1);
    if (fnames[index] == NULL) {
      fprintf(stderr, "Failed to allocate memory");
      exit(EXIT_FAILURE);
    }
    strcpy(fnames[index], token);
    token = strtok(NULL, ",");
    index++;
  }
  size_t mrb_size_g = 0;
  mrc_ccontext *c;
  for (int i = 0; i < taskc; i++) {
    /* set program file name */
    c = mrc_ccontext_new(NULL);
    if (args.verbose) c->dump_result = TRUE;
    if (args.check_syntax) c->no_exec = TRUE;
    mrc_ccontext_filename(c, fnames[i]);

    uint8_t *mrbcode = NULL;
    size_t mrb_size = 0;

    /* Load program */
    if (args.mrbfile || picorb_mrb_p(fnames[i])) {
      irep = NULL;
    }
    else if (args.fname) {
      // TODO refactor
      source = picorb_alloc(sizeof(uint8_t) * 2);
      source[0] = 0x0;
      source[1] = 0x0;
      irep = picorb_load_rb_file_cxt(c, fnames[i], &source);
    }
    else {
      char* utf8 = picorb_utf8_from_locale(args.cmdline, -1);
      if (!utf8) abort();
      irep = mrc_load_string_cxt(c, (const uint8_t **)&utf8, strlen(utf8));
      picorb_utf8_free(utf8);
    }

    if (!irep) { // mrb file
      FILE *fp = fopen(fnames[i], "rb");
      if (fp == NULL) {
        fprintf(stderr, "cannot open file: %s\n", fnames[i]);
        exit(EXIT_FAILURE);
      }
      fseek(fp, 0, SEEK_END);
      mrb_size = ftell(fp);
      fseek(fp, 0, SEEK_SET);
      mrbcode = (uint8_t *)picorb_alloc(mrb_size);
      if (fread(mrbcode, 1, mrb_size, fp) != mrb_size) {
        fprintf(stderr, "cannot read file: %s\n", fnames[i]);
        exit(EXIT_FAILURE);
      }
      fclose(fp);
    }
    else {
      int result;
      result = mrc_dump_irep(c, irep, 0, &mrbcode, &mrb_size);
      mrb_size_g = mrb_size;
      if (result != MRC_DUMP_OK) {
        fprintf(stderr, "irep dump error: %d\n", result);
        exit(EXIT_FAILURE);
      }
    }

    task_mrb_list[tasks_mrb_list_size++] = mrbcode;

    if (irep) {
//      mrc_ccontext_free(c);
//      mrc_irep_free(c, irep);
    }
    if (source) {
      mrc_free(source);
      source = NULL;
    }

//    mrbc_tcb *tcb = mrbc_create_task(mrbcode, NULL);
//    if (!tcb) {
//      fprintf(stderr, "mrbc_create_task failed\n");
//      exit(EXIT_FAILURE);
//    }
//    else {
//      tcb_list[tcb_list_size++] = tcb;
//    }

    if (args.check_syntax) {
      printf("Syntax OK: %s\n", fnames[i]);
    }
  }

  /* run tasks */
  //if (!args.check_syntax && mrbc_run() != 0) {
  if (!args.check_syntax) {
    struct RClass *target = mrb->object_class;
    //irep->reps = NULL;
    struct RProc *proc = mrb_proc_new(mrb, (mrb_irep *)irep);
    proc->c = NULL;
    if (mrb->c->cibase && mrb->c->cibase->proc == proc->upper) {
      proc->upper = NULL;
    }
    MRB_PROC_SET_TARGET_CLASS(proc, target);
    if (mrb->c->ci) {
      mrb_vm_ci_target_class_set(mrb->c->ci, target);
    }

    resolve_intern(mrb, &c->p->constant_pool, irep);

    struct RProc *proc2 = read_irep(mrb, task_mrb_list[0], mrb_size_g, 0);
    mrb_value v = mrb_top_run(mrb, proc, mrb_top_self(mrb), 0);
    if (mrb->exc) {
      MRB_EXC_CHECK_EXIT(mrb, mrb->exc);
      if (!mrb_undef_p(v)) {
        mrb_print_error(mrb);
      }
      n = EXIT_FAILURE;
    }
    else if (args.check_syntax) {
      puts("Syntax OK");
    }
  }

//  mrb_gc_arena_restore(mrb, ai);


//  for (int i = 0; i < lib_vm_list_size; i++) {
//    mrbc_vm_close(lib_vm_list[i]);
//  }
//  for (int i = 0; i < lib_mrb_list_size; i++) {
//    picorb_free(lib_mrb_list[i]);
//  }
//  for (int i = 0; i < tasks_mrb_list_size; i++) {
//    picorb_free(task_mrb_list[i]);
//  }
//  for (int i = 0; i < taskc; i++) {
//    picorb_free(fnames[i]);
//  }
//  for (int i = 0; i < tcb_list_size; i++) {
//    mrbc_vm_close(&tcb_list[i]->vm);
//  }
  cleanup(&args);

  return n;
}
