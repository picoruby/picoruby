/*
  Original source code from mrubyc/mrubyc:
    Copyright (C) 2015- Kyushu Institute of Technology.
    Copyright (C) 2015- Shimane IT Open-Innovation Center.
  Modified source code for picoruby/picoruby:
    Copyright (C) 2025 HASUMI Hitoshi.

  This file is distributed under BSD 3-Clause License.
*/

#include <picoruby.h>
#include <mruby/class.h>
#include <mruby/data.h>
#include <mruby/string.h>
#include "hal.h"
#include "task.h"

/***** Macros ***************************************************************/
#ifndef MRB_SCHEDULER_EXIT
#define MRB_SCHEDULER_EXIT 0
#endif

#define TASK_LIST_UNIT_SIZE 5
#define TASK_LIST_MAX_SIZE  100

#define MRB2TCB(mrb) ((mrb_tcb *)((uint8_t *)mrb->c - offsetof(mrb_tcb, c)))
#define MRB_MUTEX_TRACE(...) ((void)0)

#define task_queues_  mrb->task.queues
#define q_dormant_    (task_queues_[0])
#define q_ready_      (task_queues_[1])
#define q_waiting_    (task_queues_[2])
#define q_suspended_  (task_queues_[3])
#define tick_         mrb->task.tick
#define wakeup_tick_  mrb->task.wakeup_tick
#define switching_    mrb->task.switching


/***** Signal catching functions ********************************************/
/***** Functions ************************************************************/
//================================================================
/*! Insert task(TCB) to task queue

  @param  p_tcb	Pointer to target TCB

  Put the task (TCB) into a queue by each state.
  TCB must be free. (must not be in another queue)
  The queue is sorted in priority_preemption order.
  If the same priority_preemption value is in the TCB and queue,
  it will be inserted at the end of the same value in queue.
*/
static void
q_insert_task(mrb_state *mrb, mrb_tcb *p_tcb)
{
  // select target queue pointer.
  //                    state value = 0  1  2  3  4  5  6  7  8
  //                             /2   0, 0, 1, 1, 2, 2, 3, 3, 4
  static const uint8_t conv_tbl[] = { 0,    1,    2,    0,    3 };
  mrb_tcb **pp_q = &task_queues_[ conv_tbl[ p_tcb->state / 2 ]];

  // in case of insert on top.
  if((*pp_q == NULL) ||
     (p_tcb->priority_preemption < (*pp_q)->priority_preemption)) {
    p_tcb->next = *pp_q;
    *pp_q       = p_tcb;
    return;
  }

  // find insert point in sorted linked list.
  mrb_tcb *p = *pp_q;
  while( p->next != NULL ) {
    if( p_tcb->priority_preemption < p->next->priority_preemption ) break;
    p = p->next;
  }

  // insert tcb to queue.
  p_tcb->next = p->next;
  p->next     = p_tcb;
}


//================================================================
/*! Delete task(TCB) from task queue

  @param  p_tcb	Pointer to target TCB
*/
static void
q_delete_task(mrb_state *mrb, mrb_tcb *p_tcb)
{
  // select target queue pointer. (same as q_insert_task)
  static const uint8_t conv_tbl[] = { 0,    1,    2,    0,    3 };
  mrb_tcb **pp_q = &task_queues_[ conv_tbl[ p_tcb->state / 2 ]];

  if( *pp_q == p_tcb ) {
    *pp_q       = p_tcb->next;
    p_tcb->next = NULL;
    return;
  }

  mrb_tcb *p = *pp_q;
  while( p ) {
    if( p->next == p_tcb ) {
      p->next     = p_tcb->next;
      p_tcb->next = NULL;
      return;
    }

    p = p->next;
  }

  mrb_assert(!"Not found target task in queue.");
}



//================================================================
/*! preempt running task
*/
inline static void
preempt_running_task(mrb_state *mrb)
{
  for( mrb_tcb *t = q_ready_; t != NULL; t = t->next ) {
    if( t->state == TASKSTATE_RUNNING ) switching_ = TRUE;
  }
}


//================================================================
/*! Tick timer interrupt handler.

*/
void
mrb_tick(mrb_state *mrb)
{
  tick_++;

  // Decrease the time slice value for running tasks.
  mrb_tcb *tcb = q_ready_;
  if (tcb && 0 < tcb->timeslice) {
    tcb->timeslice--;
    if (tcb->timeslice == 0) {
      switching_ = TRUE;
    }
  }

  // Check the wakeup tick.
  if( (int32_t)(wakeup_tick_ - tick_) < 0 ) {
    int task_switch = 0;
    wakeup_tick_ = tick_ + (1 << 16);

    // Find a wake up task in waiting task queue.
    tcb = q_waiting_;
    while (tcb != NULL) {
      mrb_tcb *t = tcb;
      tcb = tcb->next;
      if (t->reason != TASKREASON_SLEEP) continue;

      if ((int32_t)(t->wakeup_tick - tick_) < 0) {
          q_delete_task(mrb, t);
          t->state  = TASKSTATE_READY;
          t->reason = 0;
          q_insert_task(mrb, t);
          task_switch = 1;
      } else if ((int32_t)(t->wakeup_tick - wakeup_tick_) < 0) {
        wakeup_tick_ = t->wakeup_tick;
      }
    }

    if (task_switch) preempt_running_task(mrb);
  }
}


static void
mrb_task_tcb_free(mrb_state *mrb, void *ptr) {
  mrb_gc_unregister(mrb, ((mrb_tcb *)ptr)->task);
}
struct mrb_data_type mrb_task_tcb_type = {
  "TCB", mrb_task_tcb_free
};

//================================================================
/*! create (allocate) TCB.

  @param  mrb           mrb_state
  @param  task_state    task initial state.
  @param  priority      task priority.
  @return pointer to TCB or NULL.

<b>Code example</b>
@code
  //  If you want specify default value, see below.
  //    task_state: MRB_TASK_DEFAULT_STATE
  //    priority:   MRB_TASK_DEFAULT_PRIORITY
  mrb_tcb *tcb;
  tcb = mrb_tcb_new( mrb, MRB_TASK_DEFAULT_STATE, MRB_TASK_DEFAULT_PRIORITY );
  mrb_create_task( byte_code, tcb );
@endcode
*/
mrb_tcb *
mrb_tcb_new(mrb_state *mrb, enum MrbTaskState task_state, int priority)
{
  mrb_tcb *tcb;

  unsigned int size = sizeof(mrb_tcb) + sizeof(struct mrb_context);
  tcb = mrb_malloc(mrb, size);
  if( !tcb ) return NULL;	// ENOMEM

  memset(tcb, 0, size);
#if defined(PICORUBY_DEBUG)
  memcpy( tcb->type, "TCB", 4 );
#endif
  tcb->priority = priority;
  tcb->state = task_state;

  struct RClass *class_Task = mrb_class_get(mrb, "Task");
  mrb_value task = mrb_obj_new(mrb, class_Task, 0, NULL);
  DATA_PTR(task) = tcb;
  DATA_TYPE(task) = &mrb_task_tcb_type;

  mrb_gc_register(mrb, task);

  tcb->task = task;
  return tcb;
}


#define TASK_STACK_INIT_SIZE 64
#define TASK_CI_INIT_SIZE 8

void
mrb_tcb_init_context(mrb_state *mrb, struct mrb_context *c, struct RProc *proc)
{
  size_t slen = TASK_STACK_INIT_SIZE;
  if (proc->body.irep->nregs > slen) {
    slen += proc->body.irep->nregs;
  }

  c->stbase = (mrb_value*)mrb_malloc(mrb, slen*sizeof(mrb_value));
  c->stend = c->stbase + slen;
  {
    mrb_value *s = c->stbase;
    mrb_value *send = c->stend;
    while (s < send) {
      SET_NIL_VALUE(*s);
      s++;
    }
  }

  static const mrb_callinfo ci_zero = { 0 };
  c->cibase = (mrb_callinfo *)mrb_malloc(mrb, TASK_CI_INIT_SIZE * sizeof(mrb_callinfo));
  c->ciend = c->cibase + TASK_CI_INIT_SIZE;
  c->ci = c->cibase;
  c->cibase[0] = ci_zero;

  MRB_PROC_SET_TARGET_CLASS(proc, mrb->object_class);
  mrb_vm_ci_target_class_set(c->ci, MRB_PROC_TARGET_CLASS(proc));
  mrb_vm_ci_proc_set(c->ci, proc);
  c->ci->stack = c->stbase;

  c->status = MRB_TASK_CREATED;
}


//================================================================
/*! Create a task specifying bytecode to be executed.

  @param  proc      pointer to RProc
  @param  tcb       Task control block with parameter, or NULL.
  @return Pointer to mrb_tcb or NULL.
*/
mrb_tcb *
mrb_create_task(mrb_state *mrb, struct RProc *proc, mrb_tcb *tcb, const char *name)
{
  static uint8_t context_id = 0;
  context_id++;

  if(!tcb) tcb = mrb_tcb_new(mrb, MRB_TASK_DEFAULT_STATE, MRB_TASK_DEFAULT_PRIORITY);
  if(!tcb) return NULL;	// ENOMEM

  tcb->context_id = context_id;
  tcb->priority_preemption = tcb->priority;
  size_t len = strlen(name);
  if (len > MRB_TASK_NAME_LEN) len = MRB_TASK_NAME_LEN;
  memcpy(tcb->name, name, len);
  tcb->name[len] = '\0';

  // TODO: assign CTX ID?
  mrb_tcb_init_context(mrb, &tcb->c, proc);

  if (mrb->c_list_capa == 0) {
    struct mrb_context **new_list = (struct mrb_context **)mrb_malloc(mrb, sizeof(struct mrb_context *) * TASK_LIST_UNIT_SIZE);
    if (!new_list) return NULL;
    mrb->c_list = new_list;
    mrb->c_list_capa = TASK_LIST_UNIT_SIZE;
    mrb->c_list[0] = mrb->root_c;
    mrb->c_list[1] = &tcb->c;
    mrb->c_list_len = 2;
  } else {
    if (mrb->c_list_capa <= mrb->c_list_len) {
      int new_capa = mrb->c_list_capa + TASK_LIST_UNIT_SIZE;
      if (TASK_LIST_MAX_SIZE < new_capa) {
        mrb_warn(mrb, "Too many tasks");
      }
      struct mrb_context **new_list = (struct mrb_context **)mrb_realloc(mrb, mrb->c_list, sizeof(struct mrb_context *) * (new_capa));
      if (!new_list) return NULL;
      mrb->c_list = new_list;
      mrb->c_list_capa = new_capa;
    }
    mrb->c_list[mrb->c_list_len] = &tcb->c;
    mrb->c_list_len++;
  }

  hal_disable_irq();
  q_insert_task(mrb, tcb);
  if (tcb->state & TASKSTATE_READY) preempt_running_task(mrb);
  hal_enable_irq();

  return tcb;
}


void
mrb_tcb_free(mrb_state *mrb, mrb_tcb *tcb)
{
  // TODO
}

//================================================================
/*! Delete a task.

  @param  tcb		Task control block.
  @return Pointer to mrb_tcb or NULL.
*/
int
mrb_delete_task(mrb_state *mrb, mrb_tcb *tcb)
{
  if( tcb->state != TASKSTATE_DORMANT )  return -1;

  hal_disable_irq();
  q_delete_task(mrb, tcb);
  hal_enable_irq();

  //mrb_vm_close( &tcb->mrb );
  //mrb_free_context(mrb, &tcb->c);

  return 0;
}


//================================================================
/*! execute

*/
mrb_value
mrb_tasks_run(mrb_state *mrb)
{
  mrb_value ret = mrb_nil_value();

#if MRB_SCHEDULER_EXIT
  if( !q_ready_ && !q_waiting_ && !q_suspended_ ) return ret;
#else
  (void)ret;
#endif

  while( 1 ) {
    mrb_tcb *tcb = q_ready_;
    if( tcb == NULL ) {		// no task to run.
      hal_idle_cpu(mrb);
      continue;
    }

    /*
      run the task.
    */
    tcb->state = TASKSTATE_RUNNING;   // to execute.
    mrb->c = &tcb->c;
    tcb->timeslice = MRB_TIMESLICE_TICK_COUNT;
    tcb->value = mrb_vm_exec(mrb, mrb->c->ci->proc, mrb->c->ci->pc);

    if (mrb->exc) {
      tcb->value = mrb_obj_value(mrb->exc);
      mrb->exc = NULL;
      tcb->c.status = MRB_TASK_STOPPED;
    }
    switching_ = FALSE;
    /*
      did the task done?
    */
    if (tcb->c.status == MRB_TASK_STOPPED) {
      hal_disable_irq();
      q_delete_task(mrb, tcb);
      tcb->state = TASKSTATE_DORMANT;
      q_insert_task(mrb, tcb);
      hal_enable_irq();

     // if(!tcb->flag_permanence) mrb_free_context(mrb, &tcb->c);
      if(mrb->exc) ret = mrb_obj_value(mrb->exc);

      // find task that called join.
      for( mrb_tcb *tcb1 = q_waiting_; tcb1 != NULL; tcb1 = tcb1->next ) {
        if( tcb1->reason == TASKREASON_JOIN && tcb1->tcb_join == tcb ) {
          hal_disable_irq();
          q_delete_task(mrb, tcb1);
          tcb1->state = TASKSTATE_READY;
          tcb1->reason = 0;
          q_insert_task(mrb, tcb1);
          hal_enable_irq();
        }
      }
      for( mrb_tcb *tcb1 = q_suspended_; tcb1 != NULL; tcb1 = tcb1->next ) {
        if( tcb1->reason == TASKREASON_JOIN && tcb1->tcb_join == tcb ) {
          tcb1->reason = 0;
        }
      }

#if MRB_SCHEDULER_EXIT
      if( !q_ready_ && !q_waiting_ && !q_suspended_ ) return ret;
#endif
      continue;
    }

    /*
      Switch task.
    */
    if( tcb->state == TASKSTATE_RUNNING ) {
      tcb->state = TASKSTATE_READY;

      hal_disable_irq();
      q_delete_task(mrb, tcb);       // insert task on queue last.
      q_insert_task(mrb, tcb);
      hal_enable_irq();
    }
    continue;
  }
}


//================================================================
/*! sleep for a specified number of milliseconds.

  @param  tcb     target task.
  @param  ms      sleep milliseconds.
*/
static void
sleep_ms_impl(mrb_state *mrb, mrb_int ms)
{
  //mrb_tcb *tcb = MRB2TCB(mrb);
  //mrb_tcb *tcb = (mrb_tcb *)((uint8_t *)mrb->c + sizeof(mrb_tcb));
  mrb_tcb *tcb = q_ready_;

  hal_disable_irq();
  q_delete_task(mrb, tcb);
  tcb->state       = TASKSTATE_WAITING;
  tcb->reason      = TASKREASON_SLEEP;
  tcb->wakeup_tick = tick_ + (ms / MRB_TICK_UNIT) + !!(ms % MRB_TICK_UNIT);

  if( (int32_t)(tcb->wakeup_tick - wakeup_tick_) < 0 ) {
    wakeup_tick_ = tcb->wakeup_tick;
  }

  q_insert_task(mrb, tcb);
  hal_enable_irq();

  switching_ = TRUE;
}


//================================================================
/*! Suspend the task.

  @param  tcb       target task.
*/
void
mrb_suspend_task(mrb_state *mrb, mrb_tcb *tcb)
{
  if( tcb->state == TASKSTATE_SUSPENDED ) return;

  hal_disable_irq();
  q_delete_task(mrb, tcb);
  tcb->state = TASKSTATE_SUSPENDED;
  q_insert_task(mrb, tcb);
  hal_enable_irq();

  switching_ = TRUE;
}


void
mrb_resume_task(mrb_state *mrb, mrb_tcb *tcb)
{
  if (tcb->state != TASKSTATE_SUSPENDED) return;

  uint8_t flag_to_ready_state = (tcb->reason == 0);

  hal_disable_irq();

  if (flag_to_ready_state) preempt_running_task(mrb);

  q_delete_task(mrb, tcb);
  tcb->state = flag_to_ready_state ? TASKSTATE_READY : TASKSTATE_WAITING;
  q_insert_task(mrb, tcb);

  hal_enable_irq();

  if (tcb->reason & TASKREASON_SLEEP) {
    if ((int32_t)(tcb->wakeup_tick - wakeup_tick_) < 0) {
      wakeup_tick_ = tcb->wakeup_tick;
    }
  }
}

void
mrb_terminate_task(mrb_state *mrb, mrb_tcb *tcb)
{
  if (tcb->state == TASKSTATE_DORMANT) return;

  hal_disable_irq();
  q_delete_task(mrb, tcb);
  tcb->state = TASKSTATE_DORMANT;
  q_insert_task(mrb, tcb);
  hal_enable_irq();

  //tcb->flag_preemption = 1;
  mrb->c->status = MRB_TASK_STOPPED;
}

//================================================================
/*! (method) sleep for a specified number of seconds (CRuby compatible)

*/
static mrb_value
mrb_sleep(mrb_state *mrb, mrb_value self)
{
  //mrb_tcb *tcb = MRB2TCB(mrb);
  mrb_tcb *tcb = q_ready_;

  if (mrb_get_argc(mrb) == 0) {
    mrb_suspend_task(mrb, tcb);
    return mrb_nil_value(); // Should be sec actually slept
  }

  mrb_value sec = mrb_get_arg1(mrb);

  switch(mrb_type(sec)) {
  case MRB_TT_INTEGER:
  {
    sleep_ms_impl(mrb, mrb_integer(sec) * 1000);
    break;
  }
#if !defined(MRB_NO_FLOAT)
  case MRB_TT_FLOAT:
  {
    sleep_ms_impl(mrb, (mrb_int)(mrb_float(sec) * 1000));
    break;
  }
#endif
  default:
#if !defined(MRB_NO_FLOAT)
    mrb_raisef(mrb, E_TYPE_ERROR, "integer or float required but %S", sec);
#else
    mrb_raisef(mrb, E_TYPE_ERROR, "integer required but %S", sec);
#endif
  }
  return sec;
}


//================================================================
/*! (method) sleep for a specified number of milliseconds.

*/
static mrb_value
mrb_sleep_ms(mrb_state *mrb, mrb_value self)
{
  mrb_value ms = mrb_get_arg1(mrb);
  if (mrb_type(ms) != MRB_TT_INTEGER) {
    mrb_raisef(mrb, E_TYPE_ERROR, "integer required but %S", ms);
  }
  sleep_ms_impl(mrb, mrb_integer(ms));
  return ms;
}

static mrb_value
mrb_task_s_current(mrb_state *mrb, mrb_value klass)
{
  mrb_tcb *tcb = MRB2TCB(mrb);
  return tcb->task;
}

static mrb_value
mrb_task_suspend(mrb_state *mrb, mrb_value self)
{
  mrb_tcb *tcb = (mrb_tcb *)mrb_data_get_ptr(mrb, self, &mrb_task_tcb_type);
  if (tcb == NULL) {
    mrb_raise(mrb, E_ARGUMENT_ERROR, "task does not have tcb");
  }
  mrb_suspend_task(mrb, tcb);
  return mrb_nil_value();
}

mrb_value
stat_sub(mrb_state *mrb, const mrb_tcb *p_tcb)
{
  mrb_value str = mrb_str_new_lit(mrb, "");
  if (p_tcb == NULL) return str;

  char buf[32];

  for (const mrb_tcb *t = p_tcb; t; t = t->next) {
    snprintf(buf, sizeof(buf), "%02d:%08x %-8.8s ", t->context_id,
#if defined(UINTPTR_MAX)
                (uint32_t)(uintptr_t)t,
#else
                (uint32_t)t,
#endif
                t->name[0] ? t->name : "(noname)");
    mrb_str_cat_cstr(mrb, str, buf);
  }
  mrb_str_cat_lit(mrb, str, "\n");

  // mrb_context address
  for (const mrb_tcb *t = p_tcb; t; t = t->next) {
    snprintf(buf, sizeof(buf), " c:%08x          ",
#if defined(UINTPTR_MAX)
        (uint32_t)(uintptr_t)&t->c);
#else
        (uint32_t)&t->c);
#endif
    mrb_str_cat_cstr(mrb, str, buf);
  }
  mrb_str_cat_lit(mrb, str, "\n");


  // task priority, state.
  //  st:SsRr
  //     ^ suspended -> S:suspended
  //      ^ waiting  -> s:sleep m:mutex J:join (uppercase is suspend state)
  //       ^ ready   -> R:ready
  //        ^ running-> r:running
  for (const mrb_tcb *t = p_tcb; t; t = t->next) {
    snprintf(buf, sizeof(buf), " pri:%3d", t->priority_preemption);
    mrb_str_cat_cstr(mrb, str, buf);
    mrb_tcb t1 = *t;               // Copy the value at this timing.
    snprintf(buf, sizeof(buf), " st:%c%c%c%c     ",
      (t1.state & TASKSTATE_SUSPENDED) ? 'S' : '-',
      (t1.state & TASKSTATE_SUSPENDED) ? ("-SM!J"[t1.reason]) :
      (t1.state & TASKSTATE_WAITING)   ? ("!sm!j"[t1.reason]) : '-',
      (t1.state & TASKSTATE_READY)     ? 'R' : '-',
      (t1.state & 0x01)                ? 'r' : '-');
    mrb_str_cat_cstr(mrb, str, buf);
  }
  mrb_str_cat_lit(mrb, str, "\n");

  // timeslice, mrb_context->status, wakeup tick
  for (const mrb_tcb *t = p_tcb; t; t = t->next) {
    snprintf(buf, sizeof(buf), " ts:%-2d lv:%d ", t->timeslice,
      t->c.status == MRB_TASK_CREATED ? 1 : 0);
    mrb_str_cat_cstr(mrb, str, buf);
    if (t->reason & TASKREASON_SLEEP) {
      snprintf(buf, sizeof(buf), "w:%-6d ", t->wakeup_tick);
      mrb_str_cat_cstr(mrb, str, buf);
    } else {
      mrb_str_cat_lit(mrb, str, "w:--     ");
    }
  }
  mrb_str_cat_lit(mrb, str, "\n");
  return str;
}

static mrb_value
mrb_task_s_stat(mrb_state *mrb, mrb_value klass)
{
  mrb_value result = mrb_str_new_lit(mrb, "");
  int ai = mrb_gc_arena_save(mrb);
  hal_disable_irq();
  mrb_str_cat_str(mrb, result, mrb_format(mrb, "<< tick_ = %d, wakeup_tick_ = %d >>\n", tick_, wakeup_tick_));
  mrb_str_cat_cstr(mrb, result, "<<< DORMANT >>>\n");
  mrb_str_cat_str(mrb, result, stat_sub(mrb, q_dormant_));
  mrb_str_cat_cstr(mrb, result, "<<< READY >>>\n");
  mrb_str_cat_str(mrb, result, stat_sub(mrb, q_ready_));
  mrb_str_cat_cstr(mrb, result, "<<< WAITING >>>\n");
  mrb_str_cat_str(mrb, result, stat_sub(mrb, q_waiting_));
  mrb_str_cat_cstr(mrb, result, "<<< SUSPENDED >>>\n");
  mrb_str_cat_str(mrb, result, stat_sub(mrb, q_suspended_));
  hal_enable_irq();
  mrb_gc_arena_restore(mrb, ai);
  return result;
}

void
mrb_picoruby_mruby_gem_init(mrb_state* mrb)
{
  hal_init(mrb);

  // initialize task queue.
  for( int i = 0; i < MRB_NUM_TASK_QUEUE; i++ ) {
    task_queues_[i] = NULL;
  }
  switching_ = FALSE;
  // initialize tick.
  wakeup_tick_ = (1 << 16); // no significant meaning.
  tick_ = 0;
  // register mrb_sleep
  struct RClass *krn = mrb->kernel_module;
  if (!krn) {
    krn = mrb_define_module_id(mrb, MRB_SYM(Kernel));
    mrb->kernel_module = krn;
  }
  mrb_define_method_id(mrb, krn, MRB_SYM(sleep), mrb_sleep, MRB_ARGS_OPT(1));
  mrb_define_method_id(mrb, krn, MRB_SYM(sleep_ms), mrb_sleep_ms, MRB_ARGS_REQ(1));

  struct RClass *class_Task = mrb_define_class_id(mrb, MRB_SYM(Task), mrb->object_class);
  MRB_SET_INSTANCE_TT(class_Task, MRB_TT_CDATA);

  mrb_define_class_method_id(mrb, class_Task, MRB_SYM(current), mrb_task_s_current, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_Task, MRB_SYM(suspend), mrb_task_suspend, MRB_ARGS_NONE());
  mrb_define_class_method_id(mrb, class_Task, MRB_SYM(stat), mrb_task_s_stat, MRB_ARGS_NONE());
}

void
mrb_picoruby_mruby_gem_final(mrb_state* mrb)
{
}
