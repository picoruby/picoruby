/*
  Original source code from mruby/mrubyc:
    Copyright (C) 2015- Kyushu Institute of Technology.
    Copyright (C) 2015- Shimane IT Open-Innovation Center.
  Modified source code for picoruby/microruby:
    Copyright (C) 2025 HASUMI Hitoshi.

  This file is distributed under BSD 3-Clause License.
*/


#ifndef HAL_H_
#define HAL_H_

/***** Feature test switches ************************************************/
/***** System headers *******************************************************/
#include <unistd.h>
#include <picoruby.h>

MRB_BEGIN_DECL

/***** Local headers ********************************************************/
/***** Constant values ******************************************************/
/***** Macros ***************************************************************/
#ifndef MRB_SCHEDULER_EXIT
#define MRB_SCHEDULER_EXIT 1
#endif

#if !defined(MRB_TICK_UNIT)
#define MRB_TICK_UNIT_1_MS   1
#define MRB_TICK_UNIT_2_MS   2
#define MRB_TICK_UNIT_4_MS   4
#define MRB_TICK_UNIT_10_MS 10
// Configuring small value for MRB_TICK_UNIT may cause a decline of timer
// accuracy depending on kernel constant HZ and USER_HZ.
// For more information about it on `man 7 time`.
#define MRB_TICK_UNIT MRB_TICK_UNIT_4_MS
// Substantial timeslice value (millisecond) will be
// MRB_TICK_UNIT * MRB_TIMESLICE_TICK_COUNT (+ Jitter).
// MRB_TIMESLICE_TICK_COUNT must be natural number
// (recommended value is from 1 to 10).
#define MRB_TIMESLICE_TICK_COUNT 3
#endif


/***** Typedefs *************************************************************/
/***** Global variables *****************************************************/
/***** Function prototypes **************************************************/

void mrb_tick(void);

void hal_init(void);
void hal_enable_irq(void);
void hal_disable_irq(void);
#define hal_idle_cpu()    sleep(1) // maybe interrupt by SIGINT


MRB_END_DECL

#endif // ifndef MRBC_HAL_H_
