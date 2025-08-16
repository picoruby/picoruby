/*
  Original source code from mruby/mrubyc:
    Copyright (C) 2015- Kyushu Institute of Technology.
    Copyright (C) 2015- Shimane IT Open-Innovation Center.
  Modified source code for picoruby/microruby:
    Copyright (C) 2025 HASUMI Hitoshi.

  This file is distributed under BSD 3-Clause License.
*/


/***** Feature test switches ************************************************/
/***** System headers *******************************************************/
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/time.h>


/***** Local headers ********************************************************/
#include "hal.h"


/***** Constat values *******************************************************/
/***** Macros ***************************************************************/
/***** Typedefs *************************************************************/
/***** Function prototypes **************************************************/
/***** Local variables ******************************************************/
static sigset_t sigset_, sigset2_;

#if defined(PICORB_VM_MRUBY)
static mrb_state *mrb_;
#elif defined(PICORB_VM_MRUBYC)
typedef void mrb_state;
#define mrb_tick(mrb) mrbc_tick()
#define hal_init(mrb) hal_init()
#define MRB_TICK_UNIT MRBC_TICK_UNIT
#endif

/***** Global variables *****************************************************/
/***** Signal catching functions ********************************************/
//================================================================
/*!@brief
  alarm signal handler

*/
static void
sig_alarm(int dummy)
{
  (void)dummy;
  mrb_tick(mrb_);
}


/***** Local functions ******************************************************/
/***** Global functions *****************************************************/

//================================================================
/*!@brief
  initialize

*/
void
hal_init(mrb_state *mrb)
{
#if defined(PICORB_VM_MRUBY)
  mrb_ = mrb;
#endif
  sigemptyset(&sigset_);
  sigaddset(&sigset_, SIGALRM);

  // タイマー用シグナル準備
  struct sigaction sa;
  sa.sa_handler = sig_alarm;
  sa.sa_flags   = SA_RESTART;
  sa.sa_mask    = sigset_;
  sigaction(SIGALRM, &sa, 0);

  // タイマー設定
  struct itimerval tval;
  int sec  = 0;
  int usec = MRB_TICK_UNIT * 1000;
  tval.it_interval.tv_sec  = sec;
  tval.it_interval.tv_usec = usec;
  tval.it_value.tv_sec     = sec;
  tval.it_value.tv_usec    = usec;
  setitimer(ITIMER_REAL, &tval, 0);
}


//================================================================
/*!@brief
  enable interrupt

*/
void
mrb_task_enable_irq(void)
{
  sigprocmask(SIG_SETMASK, &sigset2_, 0);
}


//================================================================
/*!@brief
  disable interrupt

*/
void
mrb_task_disable_irq(void)
{
  sigprocmask(SIG_BLOCK, &sigset_, &sigset2_);
}
