#include <stdio.h>
#include <stdint.h>
#include "picoruby/debug.h"

/* ARM Cortex-M fault handler with detailed debugging information */
/* Uses printf for UART output since USB may not work during fault */

typedef struct {
  uint32_t r0;
  uint32_t r1;
  uint32_t r2;
  uint32_t r3;
  uint32_t r12;
  uint32_t lr;
  uint32_t pc;
  uint32_t psr;
} stack_frame_t;

static inline uint32_t
get_MSP(void)
{
  uint32_t result;
  __asm volatile("MRS %0, msp" : "=r"(result));
  return result;
}

static inline uint32_t
get_PSP(void)
{
  uint32_t result;
  __asm volatile("MRS %0, psp" : "=r"(result));
  return result;
}

void
__attribute__((naked)) HardFault_Handler(void)
{
  __asm volatile(
    "MRS r0, msp\n"
    "MRS r1, psp\n"
    "MOV r2, lr\n"
    "B hardfault_handler_c\n"
  );
}

void
hardfault_handler_c(uint32_t msp, uint32_t psp, uint32_t exc_return)
{
  volatile uint32_t cfsr = *(volatile uint32_t*)0xe000ed28;
  volatile uint32_t hfsr = *(volatile uint32_t*)0xe000ed2c;
  volatile uint32_t dfsr = *(volatile uint32_t*)0xe000ed30;
  volatile uint32_t afsr = *(volatile uint32_t*)0xe000ed3c;
  volatile uint32_t mmar = *(volatile uint32_t*)0xe000ed34;
  volatile uint32_t bfar = *(volatile uint32_t*)0xe000ed38;

  stack_frame_t *frame;
  if (exc_return & 0x4) {
    frame = (stack_frame_t *)psp;
  } else {
    frame = (stack_frame_t *)msp;
  }

  printf("\n=== HARDFAULT ===\n");
  printf("EXC_RETURN = 0x%08lx\n", exc_return);
  printf("MSP = 0x%08lx\n", msp);
  printf("PSP = 0x%08lx\n", psp);
  printf("Using %s\n", (exc_return & 0x4) ? "PSP" : "MSP");

  if (frame != NULL) {
    printf("\nStack Frame:\n");
    printf("PC  = 0x%08lx\n", frame->pc);
    printf("LR  = 0x%08lx\n", frame->lr);
    printf("PSR = 0x%08lx\n", frame->psr);
    printf("R0  = 0x%08lx\n", frame->r0);
    printf("R1  = 0x%08lx\n", frame->r1);
    printf("R2  = 0x%08lx\n", frame->r2);
    printf("R3  = 0x%08lx\n", frame->r3);
    printf("R12 = 0x%08lx\n", frame->r12);
  } else {
    printf("ERROR: Stack frame is NULL!\n");
  }

  printf("\nFault Registers:\n");
  printf("CFSR = 0x%08lx\n", cfsr);
  printf("HFSR = 0x%08lx\n", hfsr);
  printf("DFSR = 0x%08lx\n", dfsr);
  printf("AFSR = 0x%08lx\n", afsr);

  if (cfsr & 0x0080) printf("MMAR = 0x%08lx (valid)\n", mmar);
  if (cfsr & 0x8000) printf("BFAR = 0x%08lx (valid)\n", bfar);

  printf("\nFault Status:\n");
  if (cfsr & 0x0001) printf("- IACCVIOL: Instruction access violation\n");
  if (cfsr & 0x0002) printf("- DACCVIOL: Data access violation\n");
  if (cfsr & 0x0008) printf("- MUNSTKERR: MemManage fault on unstacking\n");
  if (cfsr & 0x0010) printf("- MSTKERR: MemManage fault on stacking\n");
  if (cfsr & 0x0020) printf("- MLSPERR: MemManage fault during FP lazy state\n");
  if (cfsr & 0x0100) printf("- IBUSERR: Instruction bus error\n");
  if (cfsr & 0x0200) printf("- PRECISERR: Precise data bus error\n");
  if (cfsr & 0x0400) printf("- IMPRECISERR: Imprecise data bus error\n");
  if (cfsr & 0x0800) printf("- UNSTKERR: BusFault on unstacking\n");
  if (cfsr & 0x1000) printf("- STKERR: BusFault on stacking\n");
  if (cfsr & 0x2000) printf("- LSPERR: BusFault during FP lazy state\n");
  if (cfsr & 0x010000) printf("- UNDEFINSTR: Undefined instruction\n");
  if (cfsr & 0x020000) printf("- INVSTATE: Invalid state\n");
  if (cfsr & 0x040000) printf("- INVPC: Invalid PC\n");
  if (cfsr & 0x080000) printf("- NOCP: No coprocessor\n");
  if (cfsr & 0x1000000) printf("- UNALIGNED: Unaligned access\n");
  if (cfsr & 0x2000000) printf("- DIVBYZERO: Divide by zero\n");

  if (hfsr & 0x40000000) printf("- FORCED: Forced HardFault\n");
  if (hfsr & 0x00000002) printf("- VECTTBL: Vector table read fault\n");

  if (frame != NULL && frame->pc == 0x00000000) {
    printf("\n!!! NULL function pointer call detected !!!\n");
  } else if (frame != NULL && frame->pc < 0x10000000) {
    printf("\n!!! Invalid code address !!!\n");
  } else if (frame != NULL && frame->pc >= 0x20000000 && frame->pc < 0x30000000) {
    printf("\n!!! Executing from RAM (possible corruption) !!!\n");
  }

  printf("\n");

  while (1) {
    __asm volatile("bkpt #0");
  }
}
