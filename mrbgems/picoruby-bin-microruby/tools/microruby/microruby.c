#ifdef picorb_NO_STDIO
# error picoruby-bin-picoruby conflicts 'picorb_NO_STDIO' in your build configuration
#endif

#include "picoruby.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#if defined(PICORB_VM_MRUBY)
#define EXECUTABLE_NAME "microruby"
struct RProc* read_irep(mrb_state *vm, const uint8_t *bin, size_t bufsize, uint8_t flags);

mrb_value mrb_load_irep_file(mrb_state *vm, FILE* fp);

#elif defined(PICORB_VM_MRUBYC)

#define EXECUTABLE_NAME "picoruby"
#ifndef MAX_REGS_SIZE
#define MAX_REGS_SIZE 255
#endif

#endif /* PICORB_VM_MRUBYC */

#ifndef HEAP_SIZE
#define HEAP_SIZE (1024 * 6400 - 1)
#endif
static uint8_t vm_heap[HEAP_SIZE];

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
  wcsp = (wchar_t*) picorb_alloc(vm, (wcssize + 1) * sizeof(wchar_t));
  if (!wcsp)
    return NULL;
  wcssize = MultiByteToWideChar(GetACP(), 0, str, len, wcsp, wcssize + 1);
  wcsp[wcssize] = 0;

  mbssize = WideCharToMultiByte(CP_UTF8, 0, (LPCWSTR) wcsp, -1, NULL, 0, NULL, NULL);
  mbsp = (char*) picorb_alloc(vm, (mbssize + 1));
  if (!mbsp) {
    free(wcsp);
    return NULL;
  }
  mbssize = WideCharToMultiByte(CP_UTF8, 0, (LPCWSTR) wcsp, -1, mbsp, mbssize, NULL, NULL);
  mbsp[mbssize] = 0;
  free(wcsp);
  return mbsp;
}
#define picorb_utf8_free(vm,p) picorb_free(vm,p)
#else
#define picorb_utf8_from_locale(p, l) ((char*)(p))
#define picorb_utf8_free(vm,p)
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
  "-b           load and execute RiteBinary (vm) file",
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
 * In order to be recognized as a `.vm` file, the following three points must be satisfied:
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
dup_arg_item(void *vm, const char *item)
{
  size_t buflen = strlen(item) + 1;
  char *buf = (char*)picorb_alloc(vm, buflen);
  memcpy(buf, item, buflen);
  return buf;
}

static int
parse_args(void *vm, int argc, char **argv, struct _args *args)
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
          args->cmdline = dup_arg_item(vm, item);
        }
        else {
          size_t cmdlinelen;
          size_t itemlen;

          cmdlinelen = strlen(args->cmdline);
          itemlen = strlen(item);
          args->cmdline = (char*)picorb_realloc(vm, args->cmdline, cmdlinelen + itemlen + 2);
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
          args->libv = (char**)picorb_alloc(vm, sizeof(char*));
        }
        else {
          args->libv = (char**)picorb_realloc(vm, args->libv, sizeof(char*) * (args->libc + 1));
        }
        args->libv[args->libc++] = dup_arg_item(vm, item);
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
  args->argv = (char **)picorb_alloc(vm, sizeof(char*) * (argc + 1));
  memcpy(args->argv, argv, (argc+1) * sizeof(char*));
  args->argc = argc;

  return EXIT_SUCCESS;
}

static void
cleanup(mrb_state *vm, struct _args *args)
{
#if defined(PICORB_VM_MRUBY)
  // TODO: fix segv
//  mrb_close(vm);
#elif defined(PICORB_VM_MRUBYC)
  (void)vm;
#endif
  if (!args->fname)
    picorb_free(vm, args->cmdline);
  picorb_free(vm, args->argv);
  if (args->libc) {
    while (args->libc--) {
      picorb_free(vm, args->libv[args->libc]);
    }
    picorb_free(vm, args->libv);
  }
}

static void
picorb_print_error(mrb_state *vm)
{
#if defined(PICORB_VM_MRUBY)
  mrb_print_error(vm);
#elif defined(PICORB_VM_MRUBYC)
  //TODO
#endif
}

#if defined(PICORB_VM_MRUBY)
static int /* macro needs `mrb` */
mrb_lib_run(mrc_ccontext *cc, mrc_irep *irep)
{
  mrc_resolve_intern(cc, irep);
  mrb_state *mrb = cc->mrb;
  struct RClass *target = mrb->object_class;
  struct RProc *proc = mrb_proc_new(mrb, (mrb_irep *)irep);
  proc->c = NULL;
  if (mrb->c->cibase && mrb->c->cibase->proc == proc->upper) {
    proc->upper = NULL;
  }
  MRB_PROC_SET_TARGET_CLASS(proc, target);
  if (mrb->c->ci) {
    mrb_vm_ci_target_class_set(mrb->c->ci, target);
  }
  mrb_value v = mrb_top_run(mrb, proc, mrb_top_self(mrb), 1);
  if (mrb->exc) {
    MRB_EXC_CHECK_EXIT(mrb, mrb->exc);
    if (!mrb_undef_p(v)) {
      picorb_print_error(mrb);
    }
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}

#elif defined(PICORB_VM_MRUBYC)

static picorb_bool
picorb_undef_p(picorb_value *v)
{
  if (!v) return TRUE;
  return v->tt == MRBC_TT_EMPTY;
}

static int
mrbc_lib_run(mrbc_vm *vm, uint8_t *vm_code)
{
  if (mrbc_load_mrb(vm, vm_code) != 0) {
    fprintf(stderr, "mrbc_load_mrb failed\n");
    return EXIT_FAILURE;
  }
  mrbc_vm_begin(vm);
  mrbc_vm_run(vm);
  mrbc_vm_end(vm);
  return EXIT_SUCCESS;
}
#endif

static mrc_irep *
picorb_load_rb_file_cxt(mrc_ccontext *cc, const char *fname, uint8_t **source)
{
  char *filenames[2];// = (char**)picorb_alloc(vm, sizeof(char*) * 2);
  filenames[0] = (char *)fname;
  filenames[1] = NULL;
  mrc_irep *irep = mrc_load_file_cxt(cc, (const char **)filenames, source);
  if (irep == NULL) {
    fprintf(stderr, "irep load error\n");
  }
  return irep;
}

mrb_state *global_mrb = NULL;

int
main(int argc, char **argv)
{
  mrb_state *vm = NULL;
  picorb_vm_init();

  int n = -1;
  struct _args args;
  picorb_value ARGV;
  mrc_irep *irep = NULL;

  n = parse_args(vm, argc, argv, &args);
  if (n == EXIT_FAILURE || (args.cmdline == NULL)) {
    cleanup(vm, &args);
    return n;
  }

  int ai = picorb_gc_arena_save(vm);

  ARGV = picorb_array_new(vm, args.argc);
  for (int i = 0; i < args.argc; i++) {
    char* utf8 = picorb_utf8_from_locale(args.argv[i], -1);
    if (utf8) {
      picorb_value str = picorb_string_new(vm, utf8, strlen(utf8));
      picorb_array_push(vm, ARGV, str);
      picorb_utf8_free(vm, utf8);
    }
  }
  picorb_define_const(vm, "ARGV", ARGV);
  picorb_value debug = picorb_bool_value(args.debug);
  picorb_define_global_const(vm, "$DEBUG", debug);

  /* Set $0 */
  const char *cmdline;
  if (args.fname) {
    cmdline = args.cmdline ? args.cmdline : "-";
  }
  else {
    cmdline = "-e";
  }
  picorb_value cmd = picorb_string_new(vm, cmdline, strlen(cmdline));
  picorb_define_global_const(vm, "$0", cmd);

  uint8_t *source = NULL;

  /* Load libraries */
#if defined(PICORB_VM_MRUBY)
#elif defined(PICORB_VM_MRUBYC)
  uint8_t *lib_mrb_list[args.libc];
  int lib_mrb_list_size = 0;
#endif

  mrc_ccontext *cc = mrc_ccontext_new(vm);
  if (args.verbose)
    cc->dump_result = TRUE;
  if (args.check_syntax)
    cc->no_exec = TRUE;

#if defined(PICORB_VM_MRUBYC)
  picoruby_load_model_by_name("machine"); // Includes Kernel.puts etc. for PicoRuby
#endif

  for (int i = 0; i < args.libc; i++) {
#if defined(PICORB_VM_MRUBY)
#elif defined(PICORB_VM_MRUBYC)
    if (picoruby_load_model_by_name(args.libv[i])) {
      /* built-in library */
      continue;
    } else
#endif
    if (access(args.libv[i], R_OK) != 0) {
      fprintf(stderr, "%s: Cannot open library file: %s\n", *argv, args.libv[i]);
      mrc_ccontext_free(cc);
      cleanup(vm, &args);
      exit(EXIT_FAILURE);
    }

    mrc_ccontext_filename(cc, args.libv[i]);
    irep = NULL;
    uint8_t *vm_code = NULL;

    if (picorb_mrb_p(args.libv[i])) {
      FILE *fp = fopen(args.libv[i], "rb");
      if (fp == NULL) {
        fprintf(stderr, "%s: Cannot open file: %s\n", *argv, args.libv[i]);
        mrc_ccontext_free(cc);
        cleanup(vm, &args);
        exit(EXIT_FAILURE);
      }
      fseek(fp, 0, SEEK_END);
      size_t size = ftell(fp);
      fseek(fp, 0, SEEK_SET);
      vm_code = (uint8_t *)picorb_alloc(vm, size);
      if (fread(vm_code, 1, size, fp) != size) {
        fprintf(stderr, "cannot read file: %s\n", args.libv[i]);
        exit(EXIT_FAILURE);
      }
      fclose(fp);
    }
    else {
      irep = picorb_load_rb_file_cxt(cc, args.libv[i], &source);
    }

    if (irep) {
      size_t vm_code_size = 0;
      int result;
      result = mrc_dump_irep(cc, irep, 0, &vm_code, &vm_code_size);
      if (result != MRC_DUMP_OK) {
        fprintf(stderr, "irep dump error: %d\n", result);
        exit(EXIT_FAILURE);
      }
    }

    if (vm_code) {
#if defined(PICORB_VM_MRUBY)
      n = mrb_lib_run(cc, irep);
      mrb_vm_ci_env_clear(vm, vm->c->cibase);
      // TODO GC irep
#elif defined(PICORB_VM_MRUBYC)
      lib_mrb_list[lib_mrb_list_size++] = vm_code;
      n = mrbc_lib_run(vm, vm_code);
      if (irep) mrc_irep_free(cc, irep);
#endif
      mrc_ccontext_cleanup_local_variables(cc);
    }
    if (source) {
      // TODO refactor
      picorb_free(vm, source);
      source = NULL;
    }
  }

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

  int tcb_list_size = 0;
#if defined(PICORB_VM_MRUBY)
  mrb_value tcb_list[taskc];
#elif defined(PICORB_VM_MRUBYC)
  uint8_t *task_mrb_list[taskc];
  int tasks_mrb_list_size = 0;
  mrbc_tcb *tcb_list[taskc];
#endif

  char *fnames[taskc];
  char *token = strtok((char *)cmdline, ",");
  int index = 0;
  while (token != NULL) {
    fnames[index] = picorb_alloc(vm, strlen(token) + 1);
    if (fnames[index] == NULL) {
      fprintf(stderr, "Failed to allocate memory");
      exit(EXIT_FAILURE);
    }
    strcpy(fnames[index], token);
    { /* If fnames[index] starts with "~/", replace it with absolute path
       * This happens when 1 < taskc
       *   User inputs : `ruby ~/a.rb,~/b.rb`
       *   Program gets: `ruby /home/USER/a.rb,~/b.rb`
       */
      if (fnames[index][0] == '~' && fnames[index][1] == '/') {
        char *home = getenv("HOME");
        if (home == NULL) {
          fprintf(stderr, "Failed to get HOME environment variable. You cannot use '~/' to specify a file path\n");
          exit(EXIT_FAILURE);
        }
        if (home) {
          char *tmp = picorb_alloc(vm, strlen(home) + strlen(fnames[index]) + 1);
          if (tmp) {
            strcpy(tmp, home);
            strcat(tmp, fnames[index] + 1);
            picorb_free(vm, fnames[index]);
            fnames[index] = tmp;
          }
        }
      }
    }
    token = strtok(NULL, ",");
    index++;
  }

  uint8_t *vm_code;
  size_t vm_code_size;
  for (int i = 0; i < taskc; i++) {
    /* set program file name */
    if (args.verbose) cc->dump_result = TRUE;
    if (args.check_syntax) cc->no_exec = TRUE;
    mrc_ccontext_filename(cc, fnames[i]);

    vm_code = NULL;
    vm_code_size = 0;
    irep = NULL;

    /* Load program */
    if (args.mrbfile || picorb_mrb_p(fnames[i])) {
      FILE *fp = fopen(fnames[i], "rb");
      if (fp == NULL) {
        fprintf(stderr, "cannot open file: %s\n", fnames[i]);
        exit(EXIT_FAILURE);
      }
      fseek(fp, 0, SEEK_END);
      vm_code_size = ftell(fp);
      fseek(fp, 0, SEEK_SET);
      vm_code = (uint8_t *)picorb_alloc(vm, vm_code_size);
      if (fread(vm_code, 1, vm_code_size, fp) != vm_code_size) {
        fprintf(stderr, "cannot read file: %s\n", fnames[i]);
        exit(EXIT_FAILURE);
      }
      fclose(fp);
    }
    else if (args.fname) {
      // TODO refactor
      source = picorb_alloc(vm, sizeof(uint8_t) * 2);
      source[0] = 0x0;
      source[1] = 0x0;
      irep = picorb_load_rb_file_cxt(cc, fnames[i], &source);
      if (irep == NULL) {
        fprintf(stderr, "irep load error\n");
        exit(EXIT_FAILURE);
      }
    }
    else {
      char* utf8 = picorb_utf8_from_locale(args.cmdline, -1);
      if (!utf8) abort();
      irep = mrc_load_string_cxt(cc, (const uint8_t **)&utf8, strlen(utf8));
      if (irep == NULL) {
        fprintf(stderr, "irep load error\n");
        exit(EXIT_FAILURE);
      }
      picorb_utf8_free(vm, utf8);
    }

    mrc_assert((vm_code && !irep) || (!vm_code && irep));

#if defined(PICORB_VM_MRUBY)
    if (!irep) {
      irep = mrb_read_irep(vm, vm_code);
    }
    mrb_value name = mrb_str_new_cstr(vm, "main");
    mrb_value task = mrc_create_task(cc, irep, name, mrb_nil_value(), mrb_obj_value(vm->top_self));
    if (mrb_nil_p(task)) {
      fprintf(stderr, "mrc_create_task failed\n");
      exit(EXIT_FAILURE);
    }
    tcb_list[tcb_list_size++] = task;
#elif defined(PICORB_VM_MRUBYC)
    if (!vm_code) {
      int result;
      result = mrc_dump_irep(cc, irep, 0, &vm_code, &vm_code_size);
      if (result != MRC_DUMP_OK) {
        fprintf(stderr, "irep dump error: %d\n", result);
        exit(EXIT_FAILURE);
      }
    }
    task_mrb_list[tasks_mrb_list_size++] = vm_code;
    if (irep) mrc_irep_free(cc, irep);
    if (source) mrc_free(cc, source);

    mrbc_tcb *tcb = mrbc_create_task(vm_code, NULL);
    if (!tcb) {
      fprintf(stderr, "mrbc_create_task failed\n");
      exit(EXIT_FAILURE);
    }
    tcb_list[tcb_list_size++] = tcb;
#endif

#if defined(PICORB_VM_MRUBY)
#elif defined(PICORB_VM_MRUBYC)
#endif

    if (args.check_syntax) {
      printf("Syntax OK: %s\n", fnames[i]);
    }
  }

  /* run tasks */
  if (!args.check_syntax) {
#if defined(PICORB_VM_MRUBY)
    mrb_value v = mrb_task_run(vm);
    mrb_vm_ci_env_clear(vm, vm->c->cibase);
    n = EXIT_SUCCESS;
    if (vm->exc) {
      MRB_EXC_CHECK_EXIT(vm, vm->exc);
      if (!mrb_undef_p(v)) {
        picorb_print_error(vm);
      }
      n = EXIT_FAILURE;
    }
    mrc_irep_free(cc, irep);
    if (source) mrc_free(cc, source);
#elif defined(PICORB_VM_MRUBYC)
    if (mrbc_run() != 0) {
      if (!picorb_undef_p(NULL)) {
        picorb_print_error(vm);
      }
      n = EXIT_FAILURE;
    }
#endif
    else if (args.check_syntax) {
      puts("Syntax OK");
    }
  }

  picorb_gc_arena_restore(vm, ai);
  mrc_ccontext_free(cc);

  for (int i = 0; i < taskc; i++)
    picorb_free(vm, fnames[i]);
#if defined(PICORB_VM_MRUBY)
  for (int i = 0; i < tcb_list_size; i++)
    mrb_gc_unregister(vm, tcb_list[i]);
#elif defined(PICORB_VM_MRUBYC)
  for (int i = 0; i < lib_mrb_list_size; i++)
    picorb_free(vm, lib_mrb_list[i]);
  for (int i = 0; i < tasks_mrb_list_size; i++)
    picorb_free(vm, task_mrb_list[i]);
  for (int i = 0; i < tcb_list_size; i++)
    mrbc_vm_close(&tcb_list[i]->vm);
#endif
  cleanup(vm, &args);

  return n;
}
