/*
  Original source code from mrubyc/mrubyc:
    Copyright (C) 2015- Kyushu Institute of Technology.
    Copyright (C) 2015- Shimane IT Open-Innovation Center.
  Modified source code for picoruby/picoruby:
    Copyright (C) 2025 HASUMI Hitoshi.

  This file is distributed under BSD 3-Clause License.
*/

#ifndef MRUBY_TASK_H
#define MRUBY_TASK_H

typedef struct RTcb mrb_tcb;

#include <picoruby.h>

MRB_BEGIN_DECL

//================================================
/*!@brief
  Task state

 (Bit mapped)
  0 0 0 0 0 0 0 0
          ^ ^ ^ ^
          | | | Running
          | | Ready
          | Waiting
          Suspended
*/
enum MrbTaskState {
  TASKSTATE_DORMANT   = 0x00,	//!< Domant
  TASKSTATE_READY     = 0x02,	//!< Ready
  TASKSTATE_RUNNING   = 0x03,	//!< Running
  TASKSTATE_WAITING   = 0x04,	//!< Waiting
  TASKSTATE_SUSPENDED = 0x08,	//!< Suspended
};

enum MrbTaskReason {
  TASKREASON_SLEEP = 0x01,
  TASKREASON_MUTEX = 0x02,
  TASKREASON_JOIN  = 0x04,
};

static const int MRB_TASK_DEFAULT_PRIORITY = 128;
static const int MRB_TASK_DEFAULT_STATE = TASKSTATE_READY;

#if !defined(MRB_TASK_NAME_LEN)
#define MRB_TASK_NAME_LEN 15
#endif


/***** Macros ***************************************************************/
/***** Typedefs *************************************************************/

struct RMutex;

//================================================
/*!@brief
  Task control block
*/
typedef struct RTcb {
#if defined(PICORUBY_DEBUG)
  uint8_t type[4];              //!< set "TCB\0" for debug.
#endif
  struct RTcb *next;            //!< daisy chain in task queue.
  uint8_t priority;             //!< task priority. initial value.
  uint8_t priority_preemption;  //!< task priority. effective value.
  volatile uint8_t timeslice;   //!< time slice counter.
  uint8_t state;                //!< task state. defined in MrbcTaskState.
  uint8_t reason;               //!< sub state. defined in MrbcTaskReason.
  char name[MRB_TASK_NAME_LEN+1]; //!< task name (optional)

  union {
    uint32_t wakeup_tick;       //!< wakeup time for sleep state.
    struct RMutex *mutex;
  };
  const struct RTcb *tcb_join;  //!< joined task.

  uint8_t flag_permanence;
  uint8_t context_id;
  mrb_value task;
  mrb_value value;
  struct mrb_context c; // Each TCB has its own context

} mrb_tcb;


//================================================
/*!@brief
  Mutex
*/
typedef struct RMutex {
  volatile int lock;
  struct RTcb *tcb;
} mrb_mutex;

#define MRB_MUTEX_INITIALIZER { 0 }


/***** Global variables *****************************************************/
/***** Function prototypes **************************************************/
void mrb_tick(mrb_state *mrb);
mrb_tcb *mrb_tcb_new(mrb_state *mrb, enum MrbTaskState task_state, int priority);
void mrb_tcb_init_context(mrb_state *mrb, struct mrb_context *c, struct RProc *proc);
mrb_tcb *mrb_create_task(mrb_state *mrb, struct RProc *proc, mrb_tcb *tcb, const char *name);
int mrb_delete_task(mrb_state *mrb, mrb_tcb *tcb);
//void mrb_set_task_name(mrb_tcb *tcb, const char *name);
//mrb_tcb *mrb_find_task(const char *name);
//int mrb_start_task(mrb_tcb *tcb);
mrb_value mrb_tasks_run(mrb_state *mrb); // <- int mrbc_run(void);
//void mrb_relinquish(mrb_tcb *tcb);
//void mrb_change_priority(mrb_tcb *tcb, int priority);
void mrb_suspend_task(mrb_state *mrb, mrb_tcb *tcb);
void mrb_resume_task(mrb_state *mrb, mrb_tcb *tcb);
void mrb_terminate_task(mrb_state *mrb, mrb_tcb *tcb);
//void mrb_join_task(mrb_tcb *tcb, const mrb_tcb *tcb_join);
//mrb_mutex *mrb_mutex_init(mrb_mutex *mutex);
//int mrb_mutex_lock(mrb_mutex *mutex, mrb_tcb *tcb);
//int mrb_mutex_unlock(mrb_mutex *mutex, mrb_tcb *tcb);
//int mrb_mutex_trylock(mrb_mutex *mutex, mrb_tcb *tcb);
//void mrb_cleanup(void);
////void mrb_init(void *heap_ptr, unsigned int size);
//void pq(const mrb_tcb *p_tcb);
//void pqall(void);

void mrb_tcb_free(mrb_state *mrb, mrb_tcb *tcb);

MRB_END_DECL

#endif /* MRUBY_TASK_H */
