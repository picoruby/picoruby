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
  PSG_PKT_REG_WRITE     = 0,
  PSG_PKT_LFO_SET       = 1,  // ch, depth, rate
  PSG_PKT_CH_MUTE       = 2,  // ch, 0/1
  PSG_PKT_PAN_SET       = 3,  // ch, bal(0-15)
  PSG_PKT_TIMBRE_SET    = 4,  // ch, timbre
  PSG_PKT_LEGATO_SET    = 5   // ch, legato
} psg_opcode_t;

/* ----- packet layout -------------------------------------------------- */
/* 8-byte slot, naturally aligned to 4 bytes */
typedef struct __attribute__((packed, aligned(4))) {
  uint32_t tick;    // 0-4294967295 ms (32-bit for 49 days)
  uint8_t  op;      // psg_opcode_t
  uint8_t  reg;     // 0–13 or channel idx for LFO_SET
  uint8_t  val;     // main parameter
  uint8_t  arg;     // sub parameter (rate_hi/depth/shape etc.)
} psg_packet_t;

#ifndef PSG_PACKET_QUEUE_BITS
#define PSG_PACKET_QUEUE_BITS 8 // 2^8 = 256 packets
#endif

#define PSG_PACKET_QUEUE_LEN  (1u << PSG_PACKET_QUEUE_BITS)
#define PSG_PACKET_QUEUE_MASK (PSG_PACKET_QUEUE_LEN - 1)

/* ----- ring buffer descriptor ---------------------------------------- */
typedef struct {
  volatile uint16_t head;  // writer cursor (next free)
  volatile uint16_t tail;  // reader cursor (next valid)
  psg_packet_t *buf;
} psg_ringbuf_t;

typedef enum {
  PSG_TIMBRE_SQUARE = 0,  // square wave
  PSG_TIMBRE_TRIANGLE,    // triangle wave
  PSG_TIMBRE_SAWTOOTH,    // sawtooth wave
  PSG_TIMBRE_INVSAWTOOTH, // inverted sawtooth wave
} psg_timbre_t;

typedef struct {
  uint16_t tone_period[3];   // R0–5  (12-bit)
  uint8_t  noise_period;     // R6
  uint8_t  mixer;            // R7
  uint8_t  volume[3];        // R8–10
  uint16_t envelope_period;  // R11–12
  uint8_t  envelope_shape;   // R13
} psg_regs_t;

/*
                          | B7 | B6 | B5 | B4 | B3 | B2 | B1 | B0 |
R0  TR A tone period      |             LSB (0-255)               |
R1  TR A tone period      |-------------------|     MSB (0-15)    |
R2  TR B tone period      |             LSB (0-255)               |
R3  TR A tone period      |-------------------|     MSB (0-15)    |
R4  TR C tone period      |             LSB (0-255)               |
R5  TR A tone period      |-------------------|     MSB (0-15)    |
R6  Noise period (0-31)   |--------------|        5 bit NP        |
R7  Mixer (0-63)          |(IOB,IOA)| C  | B  | A  | C  | B  | A  |
     0: on, 1: off                  ^----noise-----^-----tone-----^
R8  TR A volume (0-15)    |--------------| M  | L3 | L2 | L1 | L0 | # If M=1,
R9  TR B volume (0-15)    |--------------| M  | L3 | L2 | L1 | L0 | # volume value is ignored
R10 TR C volume (0-15)    |--------------| M  | L3 | L2 | L1 | L0 | # and envelope is used instead
R11 Envelope period       |             LSB (0-255)               |
R12 Envelope period MSB   |             MSB (0-255)               |
R13 Envelope shape (0-15) |-------------------| E3 | E2 | E1 | E0 |
     E3: 0=continue,  1=stop          E2: 0=attack, 1=release
     E1: 0=alternate, 1=sawtooth      E0: 0=hold,   1=repeat
     R13: B3 B2 B1 B0
           0  0  x  x  _＼___________

           0  1  x  x  _／___________

           1  0  0  0  _＼＼＼＼＼＼＼

           1  0  0  1  _＼___________

           1  0  1  0  _＼／＼／＼／＼

           1  0  1  1  _＼￣￣￣￣￣￣

           1  1  0  0  _／／／／／／／

           1  1  0  1  _／￣￣￣￣￣￣

           1  1  1  0  _／＼／＼／＼／

           1  1  1  1  _／___________
                        ^^
                        Period
*/


typedef struct {
  psg_regs_t r;
  uint32_t tone_inc[3];      // 32.32 fixed-point number
  uint32_t tone_phase[3];
  uint32_t noise_shift;
  uint32_t noise_cnt;
  uint16_t env_cnt[3];       // 24-bit counter: 1step = 1 audio sample
  // envelope state machine
  uint8_t  env_level[3];     // 0..15
  uint8_t  env_dir[3];       // 0=down, 1=up
  bool     env_running[3];   // false -> stop
  // Masked bit for performance
  uint8_t  env_continue[3];
  uint8_t  env_attack[3];
  uint8_t  env_alternate[3];
  uint8_t  env_hold[3];
  // Whether to reset envelope on next updating tone_period
  bool  legato[3];
  // LFO
  uint16_t lfo_phase[3];   /* 0..65535 (wrap) */
  uint16_t lfo_inc[3];     /* Δphase per 1 ms tick */
  uint8_t  lfo_depth[3];   /* depth in cent (0..127) */
  // Mute
  uint8_t  mute_mask;      /* bit0=A bit1=B bit2=C */
  // pan
  uint8_t pan[3];
  // tone type
  psg_timbre_t timbre[3];
} psg_t;

#define SAMPLE_RATE       22050
#define CHIP_CLOCK        2000000   // 2 MHz
#define PWM_BITS          12
#define MAX_SAMPLE_WIDTH  ((1u << PWM_BITS) - 1)  // 4095

// Callback
bool PSG_audio_cb(void);
void PSG_render_block(uint32_t *dst, uint32_t samples);

// Ring buffer
bool PSG_rb_peek(psg_packet_t *out);
void PSG_rb_pop(void);
bool PSG_rb_push(const psg_packet_t *p);

// Cross core critical section
typedef uint32_t psg_cs_token_t;
psg_cs_token_t PSG_enter_critical(void);
void PSG_exit_critical(psg_cs_token_t token);

// Tick
void PSG_tick_start_core1(uint8_t p1, uint8_t p2);
void PSG_tick_stop_core1(void);
void PSG_tick_1ms(void);

// Packet dispatcher
void PSG_process_packet(const psg_packet_t *pkt);


/*
 * Output driver API
 */
typedef enum { PSG_DRV_PWM, PSG_DRV_DAC } psg_drv_t;

typedef struct {
  void (*init)(uint8_t p1, uint8_t p2);
  void (*start)(void);
  void (*stop)(void);
  void (*write)(uint16_t l, uint16_t r);
} psg_output_api_t;

/* one global pointer, switched at runtime */
extern const psg_output_api_t *psg_drv;
extern const psg_output_api_t psg_drv_pwm;
extern const psg_output_api_t psg_drv_mcp4922;
//extern const psg_output_api_t psg_drv_usbaudio;
//void audio_task(void); // For USB audio

/* Audio sampling buffer */
#define BUF_SAMPLES   256
#define BUF_MASK      (BUF_SAMPLES - 1)
extern uint32_t pcm_buf[BUF_SAMPLES]; // 32-bit stereo samples
extern volatile uint32_t wr_idx; // only core0 writes
extern volatile uint32_t rd_idx; // only core1 writes

/* PSG global state */
extern psg_t psg;
extern psg_ringbuf_t rb;

#ifdef __cplusplus
}
#endif

#endif /* PSG_DEFINED_H_ */
