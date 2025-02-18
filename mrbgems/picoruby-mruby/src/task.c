/*
  Original source code from mruby/mrubyc:
    Copyright (C) 2015- Kyushu Institute of Technology.
    Copyright (C) 2015- Shimane IT Open-Innovation Center.
  Modified source code for picoruby/microruby:
    Copyright (C) 2025 HASUMI Hitoshi.

  This file is distributed under BSD 3-Clause License.
*/

#include <picoruby.h>
#include "hal.h"
#include "task.h"

/***** Macros ***************************************************************/
#ifndef MRB_SCHEDULER_EXIT
#define MRB_SCHEDULER_EXIT 0
#endif

#define MRB2TCB(mrb) ((mrb_tcb *)((uint8_t *)mrb->c - offsetof(mrb_tcb, c)))
#define MRB_MUTEX_TRACE(...) ((void)0)


/***** Typedefs *************************************************************/
/***** Function prototypes **************************************************/
/***** Local variables ******************************************************/
#define NUM_TASK_QUEUE 4
static mrb_tcb *task_queue_[NUM_TASK_QUEUE];
#define q_dormant_   (task_queue_[0])
#define q_ready_     (task_queue_[1])
#define q_waiting_   (task_queue_[2])
#define q_suspended_ (task_queue_[3])
static volatile uint32_t tick_;
static volatile uint32_t wakeup_tick_ = (1 << 16); // no significant meaning.


/***** Global variables *****************************************************/
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
q_insert_task(mrb_tcb *p_tcb)
{
  // select target queue pointer.
  //                    state value = 0  1  2  3  4  5  6  7  8
  //                             /2   0, 0, 1, 1, 2, 2, 3, 3, 4
  static const uint8_t conv_tbl[] = { 0,    1,    2,    0,    3 };
  mrb_tcb **pp_q = &task_queue_[ conv_tbl[ p_tcb->state / 2 ]];

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
q_delete_task(mrb_tcb *p_tcb)
{
  // select target queue pointer. (same as q_insert_task)
  static const uint8_t conv_tbl[] = { 0,    1,    2,    0,    3 };
  mrb_tcb **pp_q = &task_queue_[ conv_tbl[ p_tcb->state / 2 ]];

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
preempt_running_task(void)
{
  for( mrb_tcb *t = q_ready_; t != NULL; t = t->next ) {
    if( t->state == TASKSTATE_RUNNING ) t->mrb->task_switch = 1;
  }
}


//================================================================
/*! Tick timer interrupt handler.

*/
void
mrb_tick(void)
{
  tick_++;

  // Decrease the time slice value for running tasks.
  mrb_tcb *tcb = q_ready_;
  if( (tcb != NULL) && (tcb->timeslice != 0) ) {
    tcb->timeslice--;
    if( tcb->timeslice == 0 ) tcb->mrb->task_switch = 1;
  }

  // Check the wakeup tick.
  if( (int32_t)(wakeup_tick_ - tick_) < 0 ) {
    int task_switch = 0;
    wakeup_tick_ = tick_ + (1 << 16);

    // Find a wake up task in waiting task queue.
    tcb = q_waiting_;
    while( tcb != NULL ) {
      mrb_tcb *t = tcb;
      tcb = tcb->next;
      if( t->reason != TASKREASON_SLEEP ) continue;

      if( (int32_t)(t->wakeup_tick - tick_) < 0 ) {
        q_delete_task(t);
        t->state  = TASKSTATE_READY;
        t->reason = 0;
        q_insert_task(t);
        task_switch = 1;
      } else if( (int32_t)(t->wakeup_tick - wakeup_tick_) < 0 ) {
        wakeup_tick_ = t->wakeup_tick;
      }
    }

    if( task_switch ) preempt_running_task();
  }
}


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
static mrb_tcb *
mrb_tcb_new(mrb_state *mrb, enum MrbTaskState task_state, int priority )
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

  tcb->mrb = mrb;

  return tcb;
}


#define TASK_STACK_INIT_SIZE 64
#define TASK_CI_INIT_SIZE 8

//================================================================
/*! Create a task specifying bytecode to be executed.

  @param  proc      pointer to RProc
  @param  tcb       Task control block with parameter, or NULL.
  @return Pointer to mrb_tcb or NULL.
*/
mrb_tcb *
mrb_create_task(mrb_state *mrb, const struct RProc *proc, mrb_tcb *tcb)
{
  if(!tcb) tcb = mrb_tcb_new( mrb, MRB_TASK_DEFAULT_STATE, MRB_TASK_DEFAULT_PRIORITY );
  if(!tcb) return NULL;	// ENOMEM

  tcb->priority_preemption = tcb->priority;

  // TODO: assign CTX ID?
  { // TODO: separate function
    struct mrb_context *c = &tcb->c;
    size_t slen = TASK_STACK_INIT_SIZE;
    if (proc->body.irep->nregs > slen) {
      slen += proc->body.irep->nregs;
    }
    c->stbase = (mrb_value*)mrb_malloc(mrb, slen*sizeof(mrb_value));
    c->stend = c->stbase + slen;

    {
      mrb_value *s = c->stbase + 1;
      mrb_value *send = c->stend;

      while (s < send) {
        SET_NIL_VALUE(*s);
        s++;
      }
    }

    /* copy receiver from a block */
    //c->stbase[0] = mrb->c->ci->stack[0];

    /* initialize callinfo stack */
    static const mrb_callinfo ci_zero = { 0 };
    c->cibase = (mrb_callinfo*)mrb_malloc(mrb, TASK_CI_INIT_SIZE * sizeof(mrb_callinfo));
    c->ciend = c->cibase + TASK_CI_INIT_SIZE;
    c->ci = c->cibase;
    c->cibase[0] = ci_zero;

    /* adjust return callinfo */
    mrb_callinfo *ci = c->ci;
    mrb_vm_ci_target_class_set(ci, MRB_PROC_TARGET_CLASS(proc));
    mrb_vm_ci_proc_set(ci, proc);
    //mrb_field_write_barrier(mrb, (struct RBasic*)f, (struct RBasic*)proc);
    ci->stack = c->stbase;
    ci[1] = ci[0];
    c->ci++;                      /* push dummy callinfo */

    //c->fib = f;
    //c->status = MRB_FIBER_CREATED;
  }

  hal_disable_irq();
  q_insert_task(tcb);
  if( tcb->state & TASKSTATE_READY ) preempt_running_task();
  hal_enable_irq();

  return tcb;
}


//================================================================
/*! Delete a task.

  @param  tcb		Task control block.
  @return Pointer to mrb_tcb or NULL.
*/
int
mrb_delete_task(mrb_tcb *tcb)
{
  if( tcb->state != TASKSTATE_DORMANT )  return -1;

  hal_disable_irq();
  q_delete_task(tcb);
  hal_enable_irq();

  //mrb_vm_close( &tcb->mrb );
  mrb_free_context(tcb->mrb, &tcb->c);

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
      hal_idle_cpu();
      continue;
    }

    /*
      run the task.
    */
    tcb->state = TASKSTATE_RUNNING;   // to execute.
    tcb->timeslice = MRB_TIMESLICE_TICK_COUNT;

    //int ret_vm_run = mrbc_vm_run(&tcb->mrb);
    mrb_value ret_vm_run = mrb_vm_exec(mrb, tcb->c.cibase->proc, tcb->c.cibase->proc->body.irep->iseq);
    (void)ret_vm_run; // TODO
    mrb->task_switch = 0;

    /*
      did the task done?
    */
    if (tcb->c.task_stop || mrb->exc) {
      hal_disable_irq();
      q_delete_task(tcb);
      tcb->state = TASKSTATE_DORMANT;
      q_insert_task(tcb);
      hal_enable_irq();

      if(!tcb->flag_permanence) mrb_free_context(mrb, &tcb->c);
      if(mrb->exc) ret = mrb_obj_value(mrb->exc);

      // find task that called join.
      for( mrb_tcb *tcb1 = q_waiting_; tcb1 != NULL; tcb1 = tcb1->next ) {
        if( tcb1->reason == TASKREASON_JOIN && tcb1->tcb_join == tcb ) {
          hal_disable_irq();
          q_delete_task(tcb1);
          tcb1->state = TASKSTATE_READY;
          tcb1->reason = 0;
          q_insert_task(tcb1);
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
      q_delete_task(tcb);       // insert task on queue last.
      q_insert_task(tcb);
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
void
sleep_ms(mrb_state *mrb, mrb_int ms)
{
  mrb_tcb *tcb = MRB2TCB(mrb);

  hal_disable_irq();
  q_delete_task(tcb);
  tcb->state       = TASKSTATE_WAITING;
  tcb->reason      = TASKREASON_SLEEP;
  tcb->wakeup_tick = tick_ + (ms / MRB_TICK_UNIT) + !!(ms % MRB_TICK_UNIT);

  if( (int32_t)(tcb->wakeup_tick - wakeup_tick_) < 0 ) {
    wakeup_tick_ = tcb->wakeup_tick;
  }

  q_insert_task(tcb);
  hal_enable_irq();

  tcb->mrb->task_switch = 1;
}


//================================================================
/*! Suspend the task.

  @param  tcb       target task.
*/
void
mrb_suspend_task(mrb_tcb *tcb)
{
  if( tcb->state == TASKSTATE_SUSPENDED ) return;

  hal_disable_irq();
  q_delete_task(tcb);
  tcb->state = TASKSTATE_SUSPENDED;
  q_insert_task(tcb);
  hal_enable_irq();

  tcb->mrb->task_switch = 1;
}


//================================================================
/*! (method) sleep for a specified number of seconds (CRuby compatible)

*/
static mrb_value
mrb_sleep(mrb_state *mrb, mrb_value self)
{
  mrb_tcb *tcb = MRB2TCB(mrb);

  if (mrb_get_argc(mrb) == 0) {
    mrb_suspend_task(tcb);
    return mrb_nil_value(); // Should be sec actually slept
  }

  mrb_value sec = mrb_get_arg1(mrb);

  switch(mrb_type(sec)) {
  case MRB_TT_INTEGER:
  {
    sleep_ms(mrb, mrb_integer(sec) * 1000);
    break;
  }
#if !defined(MRB_NO_FLOAT)
  case MRB_TT_FLOAT:
  {
    sleep_ms(mrb, (mrb_int)(mrb_integer(sec) * 1000));
    break;
  }
#endif
  default:
    mrb_raisef(mrb, E_TYPE_ERROR, "integer or float required but %S", sec);
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
  sleep_ms(mrb, mrb_integer(ms));
  return ms;
}


void
mrb_init_rrt0(mrb_state *mrb)
{
  hal_init();

  // initialize task queue.
  for( int i = 0; i < NUM_TASK_QUEUE; i++ ) {
    task_queue_[i] = NULL;
  }
  // initialize tick.
  tick_ = 0;
  // register mrb_sleep
  struct RClass *krn = mrb->kernel_module;
  if (!krn) {
    krn = mrb_define_module_id(mrb, MRB_SYM(Kernel));
    mrb->kernel_module = krn;
  }
  mrb_define_method(mrb, krn, "sleep", mrb_sleep, MRB_ARGS_OPT(1));
  mrb_define_method(mrb, krn, "sleep_ms", mrb_sleep_ms, MRB_ARGS_REQ(1));
}
