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
  Task status

 (Bit mapped)
  0 0 0 0 0 0 0 0
          ^ ^ ^ ^
          | | | Running
          | | Ready
          | Waiting
          Suspended
*/
enum TASKSTATUS {
  TASKSTATUS_DORMANT   = 0x00,	//!< Dormant
  TASKSTATUS_READY     = 0x02,	//!< Ready
  TASKSTATUS_RUNNING   = 0x03,	//!< Running
  TASKSTATUS_WAITING   = 0x04,	//!< Waiting
  TASKSTATUS_SUSPENDED = 0x08,	//!< Suspended
};

enum TASKREASON {
  TASKREASON_NONE  = 0x00,
  TASKREASON_SLEEP = 0x01,
  TASKREASON_MUTEX = 0x02,
  TASKREASON_JOIN  = 0x04,
};

static const int MRB_TASK_DEFAULT_PRIORITY = 128;
static const int MRB_TASK_DEFAULT_STATUS = TASKSTATUS_READY;


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
  uint8_t status;               //!< task status. defined in TASKSTATUS
  uint8_t reason;               //!< sub status. defined in TASKREASON
  mrb_value name;               //!< task name (optional)

  union {
    uint32_t wakeup_tick;       //!< wakeup time for sleep status.
    struct RMutex *mutex;
  };
  const struct RTcb *tcb_join;  //!< joined task.

  uint8_t flag_permanence;
  uint8_t context_id;
  mrb_value task;
  mrb_value value;
//  mrb_value pending_exception;

  const void *irep;
  void *cc;

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
extern struct mrb_data_type mrb_task_tcb_type;

/***** Function prototypes **************************************************/
void mrb_tick(mrb_state *mrb);
void mrb_task_init_context(mrb_state *mrb, mrb_value task, struct RProc *proc);
void mrb_task_reset_context(mrb_state *mrb, mrb_value task);
void mrb_task_proc_set(mrb_state *mrb, mrb_value task, struct RProc *proc);
mrb_value mrb_task_status(mrb_state *mrb, mrb_value task);
mrb_value mrb_task_value(mrb_state *mrb, mrb_value task);
mrb_bool mrb_stop_task(mrb_state *mrb, mrb_value task);
mrb_value mrb_create_task(mrb_state *mrb, struct RProc *proc, mrb_value name, mrb_value priority, mrb_value top_self);
mrb_bool mrb_delete_task(mrb_state *mrb, mrb_tcb *tcb);
mrb_value mrb_task_run_once(mrb_state *mrb); // for WASM event loop integration
mrb_value mrb_tasks_run(mrb_state *mrb); // <- int mrbc_run(void);
void mrb_suspend_task(mrb_state *mrb, mrb_value task);
void mrb_resume_task(mrb_state *mrb, mrb_value task);
//void mrb_resume_task_with_raise(mrb_state *mrb, mrb_value task, mrb_value exc);
void mrb_terminate_task(mrb_state *mrb, mrb_value task);
/* TODO
mrb_value mrb_mutex_new(mrb_state *mrb);
mrb_bool mrb_mutex_lock(mrb_value mutex, mrb_value task);
mrb_bool mrb_mutex_unlock(mrb_value mutex, mrb_value task);
mrb_bool mrb_mutex_trylock(mrb_value mutex, mrb_value task);
*/

MRB_END_DECL

#endif /* MRUBY_TASK_H */
