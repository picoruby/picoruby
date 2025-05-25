/*
 * picoruby-psg/include/psg.h
 */

#ifndef PSG_DEFINED_H_
#define PSG_DEFINED_H_

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Lock‑free single‑producer / single‑consumer ring buffer for PSG packets
 *   ‑ queue length   : 512 slots (configurable via BITS macro)
 *   ‑ producer (core‑0) : Ruby / Sequencer / LiveMixer
 *   ‑ consumer (core‑1) : ISR side of PSG engine
 *
 * Each slot carries an entire PSG command packet (up to 64‑bit).
 * Exactly one writer and one reader are assumed — no other locking required.
 */
typedef enum {
  PSG_PKT_REG_WRITE = 0,
  PSG_PKT_LFO_SET   = 1,  // ch, depth, rate
  PSG_PKT_CH_MUTE   = 2,  // ch, 0/1
  PSG_PKT_PAN_SET   = 3   // ch, bal(0-15)
} psg_opcode_t;

/* ----- packet layout -------------------------------------------------- */
/* 8-byte slot, naturally aligned to 4 bytes */
typedef struct __attribute__((packed, aligned(4))) {
  uint16_t tick;    // 0–65535 ms
  uint8_t  op;      // psg_opcode_t
  uint8_t  reg;     // 0–13 or channel idx for LFO_SET
  uint8_t  val;     // main parameter
  uint8_t  arg;     // sub parameter (rate_hi/depth/shape etc.)
} psg_packet_t;

#ifndef PSG_PACKET_QUEUE_BITS
#define PSG_PACKET_QUEUE_BITS 9 // 2^9 = 512
#endif

#define PSG_PACKET_QUEUE_LEN  (1u << PSG_PACKET_QUEUE_BITS)
#define PSG_PACKET_QUEUE_MASK (PSG_PACKET_QUEUE_LEN - 1)

/* ----- ring buffer descriptor ---------------------------------------- */
typedef struct {
  volatile uint16_t head;  // writer cursor (next free)
  volatile uint16_t tail;  // reader cursor (next valid)
  psg_packet_t buf[PSG_PACKET_QUEUE_LEN];
} psg_ringbuf_t;



#define SAMPLE_RATE       22050
#define CHIP_CLOCK        2000000   // 2 MHz
#define PWM_BITS          12
#define MAX_SAMPLE_WIDTH  ((1u << PWM_BITS) - 1)

// Callback
bool PSG_audio_cb(void );

// Ring buffer
bool PSG_rb_peak(psg_packet_t *out);
void PSG_rb_pop(void);

// Cross core critical section
typedef uint32_t psg_cs_token_t;
psg_cs_token_t PSG_enter_critical(void);
void PSG_exit_critical(psg_cs_token_t token);

// Tick
extern volatile uint16_t g_tick_ms;
void PSG_tick_start_core1(uint8_t p1, uint8_t p2, uint8_t p3, uint8_t p4);
void PSG_tick_stop_core1(void);
void PSG_tick_1ms(void);

// Packet dispatcher
void PSG_process_packet(const psg_packet_t *pkt);


/*
 * Output driver API
 */
typedef enum { PSG_DRV_PWM, PSG_DRV_DAC } psg_drv_t;

typedef struct {
  void (*init)(uint8_t p1, uint8_t p2, uint8_t p3, uint8_t p4);
  void (*start)(void);
  void (*stop)(void);
  void (*write)(uint16_t l, uint16_t r);
} psg_output_api_t;

/* one global pointer, switched at runtime */
extern const psg_output_api_t *psg_drv;
extern const psg_output_api_t psg_drv_pwm;

/* generic wrappers (inline for zero-cost) */
static inline void PSG_output_init(uint8_t p1, uint8_t p2, uint8_t p3, uint8_t p4) { psg_drv->init(p1,p2,p3,p4);  }
static inline void PSG_output_start(void) { psg_drv->start(); }
static inline void PSG_output_stop(void)  { psg_drv->stop();  }
static inline void PSG_output_write(uint16_t l, uint16_t r) { psg_drv->write(l, r); }
bool PSG_select_driver(psg_drv_t id);   /* returns false if unsupported */

;
#ifdef __cplusplus
}
#endif

#endif /* PSG_DEFINED_H_ */
