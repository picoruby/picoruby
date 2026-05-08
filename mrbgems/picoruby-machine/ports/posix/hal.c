/*
  Original source code from mruby/mrubyc:
    Copyright (C) 2015- Kyushu Institute of Technology.
    Copyright (C) 2015- Shimane IT Open-Innovation Center.
  Modified source code for picoruby/femtoruby:
    Copyright (C) 2025 HASUMI Hitoshi.

  This file is distributed under BSD 3-Clause License.
*/


/***** Feature test switches ************************************************/
/***** System headers *******************************************************/
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/time.h>
#include <unistd.h>


/***** Local headers ********************************************************/
#include "hal.h"


/***** Constat values *******************************************************/
/***** Macros ***************************************************************/
/***** Typedefs *************************************************************/
/***** Function prototypes **************************************************/
/***** Local variables ******************************************************/
#ifndef __EMSCRIPTEN__
static sigset_t sigset_, sigset2_;
#endif

#if defined(PICORB_VM_MRUBY)
static mrb_state *mrb_;
void picorb_tick(mrb_state *mrb);
#elif defined(PICORB_VM_MRUBYC)
typedef void mrb_state;
#define MRB_TICK_UNIT MRBC_TICK_UNIT
#endif

/***** Global variables *****************************************************/
/***** Signal catching functions ********************************************/
//================================================================
/*!@brief
  alarm signal handler

*/


/***** Local functions ******************************************************/
/***** Global functions *****************************************************/

//================================================================
/*!@brief
  initialize

*/
#ifndef __EMSCRIPTEN__
static void
sig_alarm(int dummy)
{
  (void)dummy;
#if defined(PICORB_VM_MRUBY)
  picorb_tick(mrb_);
#elif defined(PICORB_VM_MRUBYC)
  picorb_tick();
#endif
}
#endif

#if defined(PICORB_VM_MRUBY)
void
picorb_hal_init(mrb_state *mrb)
{
  mrb_ = mrb;
#else
void
picorb_hal_init(void)
{
#endif
#ifndef __EMSCRIPTEN__
  sigemptyset(&sigset_);
  sigaddset(&sigset_, SIGALRM);

  struct sigaction sa;
  sa.sa_handler = sig_alarm;
  sa.sa_flags   = SA_RESTART;
  sa.sa_mask    = sigset_;
  sigaction(SIGALRM, &sa, 0);

  struct itimerval tval;
  int sec  = 0;
  int usec = MRB_TICK_UNIT * 1000;
  tval.it_interval.tv_sec  = sec;
  tval.it_interval.tv_usec = usec;
  tval.it_value.tv_sec     = sec;
  tval.it_value.tv_usec    = usec;
  setitimer(ITIMER_REAL, &tval, 0);
#endif
}

#if defined(PICORB_VM_MRUBYC)
void picorb_hal_enable_irq(void)
{
#ifndef __EMSCRIPTEN__
  sigprocmask(SIG_SETMASK, &sigset2_, 0);
#endif
}

void picorb_hal_disable_irq(void)
{
#ifndef __EMSCRIPTEN__
  sigprocmask(SIG_BLOCK, &sigset_, &sigset2_);
#endif
}

void
picorb_hal_idle_cpu(void){
  sleep(1);
}
#endif

int
picorb_hal_write(int fd, const void *buf, int nbytes)
{
  return (int)write(1, buf, nbytes);
}


//================================================================
/*!@brief
  enable interrupt

*/
void
picorb_enable_irq(void)
{
#ifndef __EMSCRIPTEN__
  sigprocmask(SIG_SETMASK, &sigset2_, 0);
#endif
}


//================================================================
/*!@brief
  disable interrupt

*/
void
picorb_disable_irq(void)
{
#ifndef __EMSCRIPTEN__
  sigprocmask(SIG_BLOCK, &sigset_, &sigset2_);
#endif
}


//================================================================
/*!@brief
  abort program

*/
void
picorb_hal_abort(const char *s)
{
  if (s) {
    picorb_hal_write(1, s, strlen(s));
  }
  exit(1);
}

