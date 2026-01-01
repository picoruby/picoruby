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
#include <mruby/array.h>
#include <mruby/hash.h>
#include "hal.h"
#include "task.h"

#include <mruby/gc.h>
#ifndef MAX_GC_STEPS_PER_TICK
  #define MAX_GC_STEPS_PER_TICK 5
#endif

/***** Macros ***************************************************************/
#ifndef MRB_SCHEDULER_EXIT
#define MRB_SCHEDULER_EXIT 0
#endif

#define scheduler_exit() do { \
    mrb_task_disable_irq(); \
    int __flag_exit = !q_ready_ && !q_waiting_ && !q_suspended_; \
    mrb_task_enable_irq(); \
    if (__flag_exit) return ret; \
  } while (0)

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

static void mrb_task_tcb_free(mrb_state *mrb, void *ptr);

struct mrb_data_type mrb_task_tcb_type = {
  "TCB", mrb_task_tcb_free
};

/***** Signal catching functions ********************************************/
/***** Functions ************************************************************/
//================================================================
/*! Insert task(TCB) to task queue

  @param  p_tcb	Pointer to target TCB

  Put the task (TCB) into a queue by each status.
  TCB must be free. (must not be in another queue)
  The queue is sorted in priority_preemption order.
  If the same priority_preemption value is in the TCB and queue,
  it will be inserted at the end of the same value in queue.
*/
static void
q_insert_task(mrb_state *mrb, mrb_tcb *p_tcb)
{
  // select target queue pointer.
  //                    status value = 0  1  2  3  4  5  6  7  8
  //                              /2   0, 0, 1, 1, 2, 2, 3, 3, 4
  static const uint8_t conv_tbl[] = { 0,    1,    2,    0,    3 };
  mrb_tcb **pp_q = &task_queues_[conv_tbl[p_tcb->status / 2]];

  // in case of insert on top.
  if ((*pp_q == NULL) || (p_tcb->priority_preemption < (*pp_q)->priority_preemption)) {
    p_tcb->next = *pp_q;
    *pp_q       = p_tcb;
    return;
  }

  // find insert point in sorted linked list.
  mrb_tcb *p = *pp_q;
  while (p->next != NULL) {
    if (p_tcb->priority_preemption < p->next->priority_preemption) break;
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
  mrb_tcb **pp_q = &task_queues_[conv_tbl[p_tcb->status / 2]];

  if (*pp_q == p_tcb) {
    *pp_q       = p_tcb->next;
    p_tcb->next = NULL;
    return;
  }

  mrb_tcb *p = *pp_q;
  while (p) {
    if (p->next == p_tcb) {
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
  for (mrb_tcb *t = q_ready_; t != NULL; t = t->next) {
    if (t->status == TASKSTATUS_RUNNING) switching_ = TRUE;
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
  if ((int32_t)(wakeup_tick_ - tick_) < 0) {
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
          t->status  = TASKSTATUS_READY;
          t->reason = TASKREASON_NONE;
          q_insert_task(mrb, t);
          task_switch = 1;
      } else if ((int32_t)(t->wakeup_tick - wakeup_tick_) < 0) {
        wakeup_tick_ = t->wakeup_tick;
      }
    }

    if (task_switch) preempt_running_task(mrb);
  }
}

//================================================================
/*! Remove context from c_list

  @param  mrb     mrb_state
  @param  c       context to remove
*/
static void
remove_context_from_list(mrb_state *mrb, struct mrb_context *c)
{
  if (!mrb->c_list || mrb->c_list_len == 0) return;

  mrb_task_disable_irq();

  int index = -1;
  for (int i = 0; i < mrb->c_list_len; i++) {
    if (mrb->c_list[i] == c) {
      index = i;
      break;
    }
  }

  if (0 <= index) {
    for (int i = index; i < mrb->c_list_len - 1; i++) {
      mrb->c_list[i] = mrb->c_list[i + 1];
    }
    mrb->c_list[mrb->c_list_len - 1] = NULL;
    mrb->c_list_len--;
  }

  mrb_task_enable_irq();
}

static void
cleanup_context(mrb_state *mrb, struct mrb_context *c)
{
  if (c->stbase) {
    mrb_free(mrb, c->stbase);
    c->stbase = NULL;
  }
  if (c->cibase) {
    mrb_free(mrb, c->cibase);
    c->cibase = NULL;
  }
}

static void
mrb_task_tcb_free(mrb_state *mrb, void *ptr)
{
  mrb_tcb *tcb = (mrb_tcb *)ptr;

  mrb_task_disable_irq();
  q_delete_task(mrb, tcb);
  mrb_task_enable_irq();

  cleanup_context(mrb, &tcb->c);

  if (tcb->irep && tcb->cc) {
    mrc_irep_free((mrc_ccontext *)tcb->cc, (mrc_irep *)tcb->irep);
    mrc_ccontext_free((mrc_ccontext *)tcb->cc);
  }

  mrb_free(mrb, tcb);
}


//================================================================
/*! create (allocate) TCB.

  @param  mrb           mrb_state
  @param  task_status   task initial status
  @param  priority      task priority
  @return pointer to TCB
*/
static mrb_tcb *
mrb_tcb_new(mrb_state *mrb, enum TASKSTATUS task_status, int priority)
{
  static uint8_t context_id = 0;

  mrb_tcb *tcb = (mrb_tcb *)mrb_calloc(mrb, 1, sizeof(mrb_tcb) + sizeof(struct mrb_context));
  if (!tcb) return NULL;  // ENOMEM
#if defined(PICORUBY_DEBUG)
  memcpy(tcb->type, "TCB", 4);
#endif
  tcb->priority = tcb->priority_preemption = priority;
  tcb->status = task_status;

  context_id++;
  tcb->context_id = context_id;

  tcb->irep = NULL;
  tcb->cc = NULL;

  return tcb;
}


#define TASK_STACK_INIT_SIZE 64
#define TASK_CI_INIT_SIZE 8

void
mrb_task_init_context(mrb_state *mrb, mrb_value task, struct RProc *proc)
{
  mrb_tcb *tcb = (mrb_tcb *)mrb_data_get_ptr(mrb, task, &mrb_task_tcb_type);
  struct mrb_context *c = &tcb->c;
  cleanup_context(mrb, c);
  size_t slen = TASK_STACK_INIT_SIZE;
  if (proc->body.irep->nregs > slen) {
    /* Prevent excessive stack allocation */
    if (1024 < proc->body.irep->nregs) {
      mrb_raise(mrb, E_RUNTIME_ERROR, "Task stack size too large");
    }
    slen += proc->body.irep->nregs;
  }

  c->stbase = (mrb_value*)mrb_malloc(mrb, slen*sizeof(mrb_value));
  c->stend = c->stbase + slen;
  {
    mrb_value *s = c->stbase;
    mrb_value *send = c->stend;
    *s = mrb_top_self(mrb);
    s++;
    while (s < send) {
      /* Add boundary check during initialization */
      if (c->stend <= s) {
        mrb_raise(mrb, E_RUNTIME_ERROR, "Stack boundary corruption during init");
      }
      SET_NIL_VALUE(*s);
      s++;
    }
  }

  static const mrb_callinfo ci_zero = { 0 };
  c->cibase = (mrb_callinfo *)mrb_malloc(mrb, TASK_CI_INIT_SIZE * sizeof(mrb_callinfo));
  c->ciend = c->cibase + TASK_CI_INIT_SIZE;

  mrb_task_reset_context(mrb, task);

  c->cibase[0] = ci_zero;

  MRB_PROC_SET_TARGET_CLASS(proc, mrb->object_class);
  mrb_vm_ci_target_class_set(c->ci, MRB_PROC_TARGET_CLASS(proc));
  mrb_vm_ci_proc_set(c->ci, proc);
  c->ci->stack = c->stbase;
}

void
mrb_task_reset_context(mrb_state *mrb, mrb_value task)
{
  struct mrb_context *c = &((mrb_tcb *)mrb_data_get_ptr(mrb, task, &mrb_task_tcb_type))->c;
  c->ci = c->cibase;
  c->status = MRB_TASK_CREATED;
  c->ci->u.target_class = mrb->object_class;
}


void
mrb_task_proc_set(mrb_state *mrb, mrb_value task, struct RProc *proc)
{
  mrb_tcb *tcb = (mrb_tcb *)mrb_data_get_ptr(mrb, task, &mrb_task_tcb_type);
  if (tcb->c.cibase && tcb->c.cibase->u.env) {
    struct REnv *e = mrb_vm_ci_env(tcb->c.cibase);
    if (e && MRB_ENV_LEN(e) < proc->body.irep->nlocals) {
      MRB_ENV_SET_LEN(e, proc->body.irep->nlocals);
    }
  }
  mrb_vm_ci_proc_set(tcb->c.ci, proc);
}


//================================================================
/*! Create a task specifying bytecode to be executed.

  @param  proc      pointer to RProc
  @param  tcb       Task control block with parameter, or NULL.
  @return mrb_value
*/
mrb_value
mrb_create_task(mrb_state *mrb, struct RProc *proc, mrb_value name, mrb_value priority, mrb_value top_self)
{
  if (mrb_nil_p(priority)) {
    priority = mrb_fixnum_value(MRB_TASK_DEFAULT_PRIORITY);
  } else if (!mrb_integer_p(priority)) {
    mrb_raise(mrb, E_TYPE_ERROR, "priority must be an Integer");
  }
  mrb_tcb *tcb = mrb_tcb_new(mrb, (enum TASKSTATUS)MRB_TASK_DEFAULT_STATUS, mrb_integer(priority));
  if (mrb_nil_p(name)) {
    name = mrb_str_new_lit(mrb, "(noname)");
  }

  struct RClass *klass = mrb_class_get_id(mrb, MRB_SYM(Task));
  mrb_value task = mrb_obj_value(Data_Wrap_Struct(mrb, klass, &mrb_task_tcb_type, tcb));
  mrb_iv_set(mrb, task, MRB_IVSYM(name), name);

  mrb_task_init_context(mrb, task, proc);
  tcb->task = task;
  mrb_gc_register(mrb, task);

  if (!mrb_nil_p(top_self)) {
    tcb->c.ci->stack[0] = top_self;
  }

  mrb_task_disable_irq();

  if (mrb->c_list_capa == 0) {
    struct mrb_context **new_list = (struct mrb_context **)mrb_malloc(mrb, sizeof(struct mrb_context *) * TASK_LIST_UNIT_SIZE);
    if (!new_list) {
      mrb_task_enable_irq();
      mrb_raise(mrb, E_RUNTIME_ERROR, "Failed to allocate context list");
    }
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
      if (!new_list) {
        mrb_task_enable_irq();
        mrb_raise(mrb, E_RUNTIME_ERROR, "Failed to reallocate context list");
      }
      mrb->c_list = new_list;
      mrb->c_list_capa = new_capa;
    }
    mrb->c_list[mrb->c_list_len] = &tcb->c;
    mrb->c_list_len++;
  }

  q_insert_task(mrb, tcb);
  if (tcb->status & TASKSTATUS_READY) preempt_running_task(mrb);

  mrb_task_enable_irq();

  return task;
}



//================================================================
/*! Delete a task.

  @param  tcb		Task control block.
  @return Pointer to mrb_tcb or NULL.
*/
mrb_bool
mrb_delete_task(mrb_state *mrb, mrb_tcb *tcb)
{
  if (tcb->status != TASKSTATUS_DORMANT) return FALSE;

  mrb_task_disable_irq();
  q_delete_task(mrb, tcb);
  mrb_task_enable_irq();

  //mrb_vm_close( &tcb->mrb );
  //mrb_free_context(mrb, &tcb->c);

  return TRUE;
}


//================================================================
/*! execute one task step (for WASM event loop integration)

  @return  mrb_true if tasks are still running, mrb_nil if no tasks
*/
mrb_value
mrb_task_run_once(mrb_state *mrb)
{
  mrb_tcb *tcb = q_ready_;
  if (tcb == NULL) {
    return mrb_nil_value();
  }

  /*
    run the task.
  */
  tcb->status = TASKSTATUS_RUNNING;
  mrb->c = &tcb->c;
  tcb->timeslice = MRB_TIMESLICE_TICK_COUNT;

//  if (!mrb_nil_p(tcb->pending_exception)) {
//    mrb_value exc = tcb->pending_exception;
//    tcb->pending_exception = mrb_nil_value();
//    mrb_exc_raise(mrb, exc);
//  }

  tcb->value = mrb_vm_exec(mrb, mrb->c->ci->proc, mrb->c->ci->pc);

  if (mrb->exc) {
    tcb->value = mrb_obj_value(mrb->exc);
    tcb->c.status = MRB_TASK_STOPPED;
  }
  switching_ = FALSE;

  /*
    did the task done?
  */
  if (tcb->c.status == MRB_TASK_STOPPED) {
    mrb_task_disable_irq();
    q_delete_task(mrb, tcb);
    tcb->status = TASKSTATUS_DORMANT;
    q_insert_task(mrb, tcb);
    mrb_task_enable_irq();

    // find task that called join.
    mrb_bool has_join_waiter = FALSE;
    mrb_task_disable_irq();
    mrb_tcb *tcb1 = q_waiting_;
    while (tcb1 != NULL) {
      mrb_tcb *next = tcb1->next;
      if (tcb1->reason == TASKREASON_JOIN && tcb1->tcb_join == tcb) {
        has_join_waiter = TRUE;
        q_delete_task(mrb, tcb1);
        tcb1->status = TASKSTATUS_READY;
        tcb1->reason = TASKREASON_NONE;
        q_insert_task(mrb, tcb1);
      }
      tcb1 = next;
    }
    for (mrb_tcb *tcb1 = q_suspended_; tcb1 != NULL; tcb1 = tcb1->next) {
      if (tcb1->reason == TASKREASON_JOIN && tcb1->tcb_join == tcb) {
        has_join_waiter = TRUE;
        tcb1->reason = TASKREASON_NONE;
      }
    }
    mrb_task_enable_irq();

    if (!has_join_waiter && !tcb->flag_permanence) {
      remove_context_from_list(mrb, &tcb->c);
      mrb_gc_unregister(mrb, tcb->task);
    }

    return mrb_true_value();
  }

  if (mrb->gc.state != MRB_GC_STATE_ROOT) {
    int gc_steps = 0;
    while (mrb->gc.state != MRB_GC_STATE_ROOT && gc_steps < MAX_GC_STEPS_PER_TICK)
    {
      mrb_incremental_gc(mrb);
      gc_steps++;
    }
  }

  /*
    Switch task.
  */
  if (tcb->status == TASKSTATUS_RUNNING) {
    mrb_task_disable_irq();
    q_delete_task(mrb, tcb);
    tcb->status = TASKSTATUS_READY;
    q_insert_task(mrb, tcb);
    mrb_task_enable_irq();
  }

  return mrb_true_value();
}

//================================================================
/*! execute

*/
mrb_value
mrb_tasks_run(mrb_state *mrb)
{
  mrb_value ret = mrb_nil_value();

#if MRB_SCHEDULER_EXIT
  scheduler_exit();
#else
  (void)ret;
#endif

  while (1) {
    mrb_tcb *tcb = q_ready_;
    if (tcb == NULL) {   // no task to run.
      hal_idle_cpu(mrb);
      continue;
    }

    /*
      run the task.
    */
    tcb->status = TASKSTATUS_RUNNING;   // to execute.
    mrb->c = &tcb->c;
    tcb->timeslice = MRB_TIMESLICE_TICK_COUNT;

    tcb->value = mrb_vm_exec(mrb, mrb->c->ci->proc, mrb->c->ci->pc);

    if (mrb->exc) {
      tcb->value = mrb_obj_value(mrb->exc);
      tcb->c.status = MRB_TASK_STOPPED;
    }
    switching_ = FALSE;
    /*
      did the task done?
    */
    if (tcb->c.status == MRB_TASK_STOPPED) {
      mrb_task_disable_irq();
      q_delete_task(mrb, tcb);
      tcb->status = TASKSTATUS_DORMANT;
      q_insert_task(mrb, tcb);
      mrb_task_enable_irq();

      // if(!tcb->flag_permanence) mrb_free_context(mrb, &tcb->c);
      if (mrb->exc) ret = mrb_obj_value(mrb->exc);

      // find task that called join.
      mrb_bool has_join_waiter = FALSE;
      mrb_task_disable_irq();
      mrb_tcb *tcb1 = q_waiting_;
      while (tcb1 != NULL) {
        mrb_tcb *next = tcb1->next;
        if (tcb1->reason == TASKREASON_JOIN && tcb1->tcb_join == tcb) {
          has_join_waiter = TRUE;
          q_delete_task(mrb, tcb1);
          tcb1->status = TASKSTATUS_READY;
          tcb1->reason = TASKREASON_NONE;
          q_insert_task(mrb, tcb1);
        }
        tcb1 = next;
      }
      for (mrb_tcb *tcb1 = q_suspended_; tcb1 != NULL; tcb1 = tcb1->next) {
        if (tcb1->reason == TASKREASON_JOIN && tcb1->tcb_join == tcb) {
          has_join_waiter = TRUE;
          tcb1->reason = TASKREASON_NONE;
        }
      }
      mrb_task_enable_irq();

      if (!has_join_waiter && !tcb->flag_permanence) {
        remove_context_from_list(mrb, &tcb->c);
        mrb_gc_unregister(mrb, tcb->task);
      }

#if MRB_SCHEDULER_EXIT
      scheduler_exit();
#endif
      continue;
    }

    if (mrb->gc.state != MRB_GC_STATE_ROOT) {
      int gc_steps = 0;
      while (mrb->gc.state != MRB_GC_STATE_ROOT && gc_steps < MAX_GC_STEPS_PER_TICK)
      {
        mrb_incremental_gc(mrb);
        gc_steps++;
      }
    }

    /*
      Switch task.
    */
    if (tcb->status == TASKSTATUS_RUNNING) {
      tcb->status = TASKSTATUS_READY;

      mrb_task_disable_irq();
      q_delete_task(mrb, tcb);       // insert task on queue last.
      q_insert_task(mrb, tcb);
      mrb_task_enable_irq();
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
  mrb_tcb *tcb = MRB2TCB(mrb);

  mrb_task_disable_irq();
  q_delete_task(mrb, tcb);
  tcb->status       = TASKSTATUS_WAITING;
  tcb->reason      = TASKREASON_SLEEP;
  tcb->wakeup_tick = tick_ + (ms / MRB_TICK_UNIT) + !!(ms % MRB_TICK_UNIT);

  if ((int32_t)(tcb->wakeup_tick - wakeup_tick_) < 0) {
    wakeup_tick_ = tcb->wakeup_tick;
  }

  q_insert_task(mrb, tcb);
  mrb_task_enable_irq();

  switching_ = TRUE;
}


//================================================================
/*! Suspend the task.

  @param  tcb       target task.
*/
void
mrb_suspend_task(mrb_state *mrb, mrb_value task)
{
  mrb_tcb *tcb = (mrb_tcb *)mrb_data_get_ptr(mrb, task, &mrb_task_tcb_type);
  if (tcb->status == TASKSTATUS_SUSPENDED) return;

  mrb_task_disable_irq();
  q_delete_task(mrb, tcb);
  tcb->status = TASKSTATUS_SUSPENDED;
  q_insert_task(mrb, tcb);
  mrb_task_enable_irq();

  switching_ = TRUE;
}


void
mrb_resume_task(mrb_state *mrb, mrb_value task)
{
  mrb_tcb *tcb = (mrb_tcb *)mrb_data_get_ptr(mrb, task, &mrb_task_tcb_type);
  if (tcb->status != TASKSTATUS_SUSPENDED) return;

  uint8_t flag_to_ready_status = (tcb->reason == 0);

  mrb_task_disable_irq();

  if (flag_to_ready_status) preempt_running_task(mrb);

  q_delete_task(mrb, tcb);
  tcb->status = flag_to_ready_status ? TASKSTATUS_READY : TASKSTATUS_WAITING;
  q_insert_task(mrb, tcb);

  mrb_task_enable_irq();

  if (tcb->reason & TASKREASON_SLEEP) {
    if ((int32_t)(tcb->wakeup_tick - wakeup_tick_) < 0) {
      wakeup_tick_ = tcb->wakeup_tick;
    }
  }
}

//void
//mrb_resume_task_with_raise(mrb_state *mrb, mrb_value task, mrb_value exc)
//{
//  mrb_tcb *tcb = (mrb_tcb *)mrb_data_get_ptr(mrb, task, &mrb_task_tcb_type);
//  tcb->pending_exception = exc;
//  mrb_resume_task(mrb, task);
//}

void
mrb_terminate_task(mrb_state *mrb, mrb_value task)
{
  mrb_tcb *tcb = (mrb_tcb *)mrb_data_get_ptr(mrb, task, &mrb_task_tcb_type);
  if (tcb->status == TASKSTATUS_DORMANT) return;

  mrb_task_disable_irq();
  q_delete_task(mrb, tcb);
  tcb->status = TASKSTATUS_DORMANT;
  q_insert_task(mrb, tcb);
  mrb_task_enable_irq();
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
    mrb_suspend_task(mrb, tcb->task);
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


/*================================================================
* Methods for Task class
*/

static mrb_value
mrb_task_s_new(mrb_state *mrb, mrb_value klass)
{
  mrb_value proc = mrb_nil_value();
  mrb_int kw_num = 2;
  mrb_int kw_required = 0;
  mrb_sym kw_names[] = { MRB_SYM(name), MRB_SYM(priority) };
  mrb_value kw_values[kw_num];
  mrb_kwargs kwargs = { kw_num, kw_required, kw_names, kw_values, NULL };
  mrb_get_args(mrb, ":&", &kwargs, &proc);
  if (mrb_undef_p(kw_values[0])) {
    kw_values[0] = mrb_nil_value();
  }
  if (mrb_undef_p(kw_values[1])) {
    kw_values[1] = mrb_nil_value();
  }

  struct RProc *proc_ptr = mrb_proc_ptr(proc);
  return mrb_create_task(mrb, proc_ptr, kw_values[0], kw_values[1], mrb_obj_value(mrb->object_class));
}

static mrb_value
mrb_task_s_list(mrb_state *mrb, mrb_value klass)
{
  mrb_tcb **queues = task_queues_;
  mrb_value list = mrb_ary_new(mrb);
  mrb_tcb *queue = queues[0];
  for (int i = 0; i < 4; i++) {
    queue = queues[i];
    while (queue) {
      mrb_value task = queue->task;
      mrb_ary_push(mrb, list, task);
      queue = queue->next;
    }
  }
  return list;
}

static mrb_value
mrb_task_s_pass(mrb_state *mrb, mrb_value klass)
{
  mrb_tcb *tcb = MRB2TCB(mrb);
  tcb->timeslice = 0;
  tcb->priority_preemption = 1;
  return mrb_nil_value();
}

static mrb_value
mrb_task_s_tick(mrb_state *mrb, mrb_value klass)
{
  return mrb_fixnum_value(tick_ * MRB_TICK_UNIT);
}

static mrb_value
mrb_task_s_current(mrb_state *mrb, mrb_value klass)
{
  /* Validate current context before using MRB2TCB */
  if (mrb->c == NULL) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "No current context");
  }

  mrb_tcb *tcb = MRB2TCB(mrb);
  return tcb->task;
}

static mrb_value
mrb_task_s_get(mrb_state *mrb, mrb_value klass)
{
  mrb_value name;
  mrb_get_args(mrb, "S", &name);
  mrb_tcb **queues = task_queues_;
  mrb_tcb *queue = queues[0];
  for (int i = 0; i < 4; i++) {
    queue = queues[i];
    while (queue) {
      mrb_value task_ivar_name = mrb_iv_get(mrb, queue->task, MRB_IVSYM(name));
      if (mrb_string_p(task_ivar_name)) {
        if (mrb_str_cmp(mrb, task_ivar_name, name) == 0) {
          return queue->task;
        }
      }
      if (mrb_str_cmp(mrb, queue->name, name) == 0) {
        return queue->task;
      }
      queue = queue->next;
    }
  }
  return mrb_nil_value();
}

mrb_bool
mrb_stop_task(mrb_state *mrb, mrb_value task)
{
  mrb_tcb *tcb = (mrb_tcb *)mrb_data_get_ptr(mrb, task, &mrb_task_tcb_type);
  if (tcb->c.status == MRB_TASK_STOPPED) {
    return FALSE; // already stopped
  }
  tcb->c.status = MRB_TASK_STOPPED;
  return TRUE;
}

mrb_value
mrb_task_value(mrb_state *mrb, mrb_value task)
{
  mrb_tcb *tcb = (mrb_tcb *)mrb_data_get_ptr(mrb, task, &mrb_task_tcb_type);
  return tcb->value;
}

mrb_value
mrb_task_status(mrb_state *mrb, mrb_value task)
{
  mrb_tcb *tcb = (mrb_tcb *)mrb_data_get_ptr(mrb, task, &mrb_task_tcb_type);

  const char *status_str;
  switch (tcb->status) {
    case TASKSTATUS_DORMANT:   status_str = "DORMANT";   break;
    case TASKSTATUS_READY:     status_str = "READY";     break;
    case TASKSTATUS_RUNNING:   status_str = "RUNNING";   break;
    case TASKSTATUS_WAITING:   status_str = "WAITING";   break;
    case TASKSTATUS_SUSPENDED: status_str = "SUSPENDED"; break;
    default:                   status_str = "UNKNOWN";   break;
  }

  mrb_value ret = mrb_str_new_cstr(mrb, status_str);

  if (tcb->status == TASKSTATUS_WAITING) {
    const char *reason_str;
    switch (tcb->reason) {
      case TASKREASON_SLEEP: reason_str = "SLEEP"; break;
      case TASKREASON_MUTEX: reason_str = "MUTEX"; break;
      case TASKREASON_JOIN:  reason_str = "JOIN";  break;
      default:               reason_str = "";      break;
    }
    mrb_str_cat_cstr(mrb, ret, reason_str);
  }

  return ret;
}

static mrb_tcb *
mrb_task_get_tcb(mrb_state *mrb, mrb_value self)
{
  mrb_tcb *tcb = (mrb_tcb *)mrb_data_get_ptr(mrb, self, &mrb_task_tcb_type);
  if (!tcb) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "Task not found");
  }
  return tcb;
}

static mrb_value
mrb_task_inspect(mrb_state *mrb, mrb_value self)
{
  mrb_tcb *tcb = mrb_task_get_tcb(mrb, self);
  mrb_value status = mrb_task_status(mrb, self);
  char buf[64];
  mrb_value name = mrb_iv_get(mrb, self, MRB_IVSYM(name));
  sprintf(buf, "#<Task:%p %s:%s>", (void *)tcb, RSTRING_PTR(name), RSTRING_PTR(status));
  return mrb_str_new_cstr(mrb, buf);
}

static mrb_value
mrb_task_priority(mrb_state *mrb, mrb_value self)
{
  mrb_tcb *tcb = mrb_task_get_tcb(mrb, self);
  return mrb_fixnum_value(tcb->priority);
}

static mrb_value
mrb_task_priority_set(mrb_state *mrb, mrb_value self)
{
  mrb_int priority;
  mrb_tcb *tcb = mrb_task_get_tcb(mrb, self);
  mrb_get_args(mrb, "i", &priority);
  tcb->priority = priority;
  return mrb_fixnum_value(tcb->priority);
}

static mrb_value
mrb_task_suspend(mrb_state *mrb, mrb_value self)
{
  mrb_suspend_task(mrb, self);
  return self;
}

static mrb_value
mrb_task_resume(mrb_state *mrb, mrb_value self)
{
  mrb_resume_task(mrb, self);
  return self;
}

static mrb_value
mrb_task_terminate(mrb_state *mrb, mrb_value self)
{
  mrb_terminate_task(mrb, self);
  return self;
}

/*
  Note: Circular join goes deadlock
*/
static mrb_value
mrb_task_join(mrb_state *mrb, mrb_value self)
{
  mrb_tcb *current_tcb = MRB2TCB(mrb);
  mrb_tcb *tcb_to_join = mrb_task_get_tcb(mrb, self);
  if (tcb_to_join == current_tcb) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "Cannot join to self");
  }
  if (tcb_to_join->status == TASKSTATUS_DORMANT) return self;

  mrb_task_disable_irq();
  q_delete_task(mrb, current_tcb);
  current_tcb->status = TASKSTATUS_WAITING;
  current_tcb->reason = TASKREASON_JOIN;
  current_tcb->tcb_join = tcb_to_join;
  q_insert_task(mrb, current_tcb);
  mrb_task_enable_irq();

  switching_ = TRUE;
  return self;
}

#if defined(UINTPTR_MAX)
#define MRB_UINTPTR(ptr) (uint32_t)(uintptr_t)ptr
#else
#define MRB_UINTPTR(ptr) (uint32_t)ptr
#endif

static mrb_value
mrb_stat_sub(mrb_state *mrb, const mrb_tcb *p_tcb)
{
  mrb_value array = mrb_ary_new(mrb);
  if (p_tcb == NULL) return array;
  for (const mrb_tcb *t = p_tcb; t; t = t->next) {
    mrb_value hash = mrb_hash_new(mrb);
    mrb_hash_set(mrb, hash, mrb_symbol_value(MRB_SYM(c_id)), mrb_fixnum_value(t->context_id));
    mrb_hash_set(mrb, hash, mrb_symbol_value(MRB_SYM(c_ptr)), mrb_fixnum_value(MRB_UINTPTR(&t->c)));
    mrb_hash_set(mrb, hash, mrb_symbol_value(MRB_SYM(c_status)), mrb_symbol_value(
          t->c.status == MRB_TASK_CREATED ? MRB_SYM(CREATED) :
          t->c.status == MRB_TASK_STOPPED ? MRB_SYM(STOPPED) :
                                            MRB_SYM(UNKNOWN))); // This should not happen

    mrb_hash_set(mrb, hash, mrb_symbol_value(MRB_SYM(ptr)), mrb_fixnum_value(MRB_UINTPTR(t)));
    mrb_hash_set(mrb, hash, mrb_symbol_value(MRB_SYM(name)), t->name);
    mrb_hash_set(mrb, hash, mrb_symbol_value(MRB_SYM(priority)), mrb_fixnum_value(t->priority_preemption));
    mrb_hash_set(mrb, hash, mrb_symbol_value(MRB_SYM(status)), mrb_symbol_value(
          t->status == TASKSTATUS_DORMANT   ? MRB_SYM(DORMANT) :
          t->status == TASKSTATUS_READY     ? MRB_SYM(READY) :
          t->status == TASKSTATUS_RUNNING   ? MRB_SYM(RUNNING) :
          t->status == TASKSTATUS_WAITING   ? MRB_SYM(WAITING) :
          t->status == TASKSTATUS_SUSPENDED ? MRB_SYM(SUSPENDED) :
                                              MRB_SYM(UNKNOWN))); // This should not happen
    mrb_hash_set(mrb, hash, mrb_symbol_value(MRB_SYM(reason)), mrb_symbol_value(
          t->reason == TASKREASON_NONE    ? MRB_SYM(NONE) :
          t->reason == TASKREASON_SLEEP   ? MRB_SYM(SLEEP) :
          t->reason == TASKREASON_JOIN    ? MRB_SYM(JOIN) :
          t->reason == TASKREASON_MUTEX   ? MRB_SYM(MUTEX) :
                                            MRB_SYM(UNKNOWN))); // This should not happen
    mrb_hash_set(mrb, hash, mrb_symbol_value(MRB_SYM(timeslice)), mrb_fixnum_value(t->timeslice));
    mrb_hash_set(mrb, hash, mrb_symbol_value(MRB_SYM(wakeup_tick)), mrb_fixnum_value(t->wakeup_tick));
    mrb_ary_push(mrb, array, hash);
  }
  return array;
}

static mrb_value
mrb_task_s_stat(mrb_state *mrb, mrb_value klass)
{
  mrb_value data = mrb_hash_new(mrb);
  mrb_task_disable_irq();
  mrb_hash_set(mrb, data, mrb_symbol_value(MRB_SYM(tick)), mrb_fixnum_value(tick_));
  mrb_hash_set(mrb, data, mrb_symbol_value(MRB_SYM(wakeup_tick)), mrb_fixnum_value(wakeup_tick_));
  mrb_hash_set(mrb, data, mrb_symbol_value(MRB_SYM(dormant)), mrb_stat_sub(mrb, q_dormant_));
  mrb_hash_set(mrb, data, mrb_symbol_value(MRB_SYM(ready)), mrb_stat_sub(mrb, q_ready_));
  mrb_hash_set(mrb, data, mrb_symbol_value(MRB_SYM(waiting)), mrb_stat_sub(mrb, q_waiting_));
  mrb_hash_set(mrb, data, mrb_symbol_value(MRB_SYM(suspended)), mrb_stat_sub(mrb, q_suspended_));
  mrb_task_enable_irq();

  struct RClass *class_Stat = mrb_class_get_under_id(mrb, mrb_class_ptr(klass), MRB_SYM(Stat));
  mrb_value stat = mrb_obj_new(mrb, class_Stat, 0, NULL);
  mrb_obj_iv_set(mrb, mrb_obj_ptr(stat), MRB_IVSYM(data), data);
  return stat;
}

void
mrb_picoruby_mruby_gem_init(mrb_state* mrb)
{
#ifdef ESP32_PLATFORM
  machine_hal_init(mrb);
#else
  hal_init(mrb);
#endif

  // initialize task queue.
  for (int i = 0; i < MRB_NUM_TASK_QUEUE; i++) {
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
  mrb_define_private_method_id(mrb, krn, MRB_SYM(sleep), mrb_sleep, MRB_ARGS_OPT(1));
  mrb_define_private_method_id(mrb, krn, MRB_SYM(sleep_ms), mrb_sleep_ms, MRB_ARGS_REQ(1));

  struct RClass *class_Task = mrb_define_class_id(mrb, MRB_SYM(Task), mrb->object_class);
  MRB_SET_INSTANCE_TT(class_Task, MRB_TT_CDATA);

  mrb_define_class_under_id(mrb, class_Task, MRB_SYM(Stat), mrb->object_class);

  mrb_define_class_method_id(mrb, class_Task, MRB_SYM(new), mrb_task_s_new, MRB_ARGS_KEY(2, 0)|MRB_ARGS_BLOCK());
  mrb_define_class_method_id(mrb, class_Task, MRB_SYM(stat), mrb_task_s_stat, MRB_ARGS_NONE());
  mrb_define_class_method_id(mrb, class_Task, MRB_SYM(current), mrb_task_s_current, MRB_ARGS_NONE());
  mrb_define_class_method_id(mrb, class_Task, MRB_SYM(get), mrb_task_s_get, MRB_ARGS_REQ(1));
  mrb_define_class_method_id(mrb, class_Task, MRB_SYM(list), mrb_task_s_list, MRB_ARGS_NONE());
  mrb_define_class_method_id(mrb, class_Task, MRB_SYM(pass), mrb_task_s_pass, MRB_ARGS_NONE());
  mrb_define_class_method_id(mrb, class_Task, MRB_SYM(tick), mrb_task_s_tick, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_Task, MRB_SYM(status), mrb_task_status, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_Task, MRB_SYM(inspect), mrb_task_inspect, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_Task, MRB_SYM(priority), mrb_task_priority, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_Task, MRB_SYM_E(priority), mrb_task_priority_set, MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, class_Task, MRB_SYM(suspend), mrb_task_suspend, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_Task, MRB_SYM(resume), mrb_task_resume, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_Task, MRB_SYM(terminate), mrb_task_terminate, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_Task, MRB_SYM(join), mrb_task_join, MRB_ARGS_NONE());
}

void
mrb_picoruby_mruby_gem_final(mrb_state* mrb)
{
}
