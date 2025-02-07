#ifdef DISABLE_MRUBY

#ifndef MRUBY_H
#define MRUBY_H

typedef void mrb_state;
int mrb_gc_arena_save(mrb_state*);
void mrb_gc_arena_restore(mrb_state*, int);
void mrb_state_atexit(mrb_state*, void*);

#endif /* MRUBY_H */

#endif /* DISABLE_MRUBY */
