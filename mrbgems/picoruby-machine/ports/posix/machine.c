#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <signal.h>
#include <time.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>
#include <poll.h>

#ifdef __APPLE__
#include <uuid/uuid.h>
#endif

#include "../../include/machine.h"
#include "../../include/hal.h"
#include "../../../picoruby-io-console/include/io-console.h"

/*-------------------------------------
 *
 * stdin reader thread + ring buffer
 *
 * On bare-metal targets, the UART/CDC RX interrupt asynchronously
 * intercepts Ctrl-C (0x03) in cooked mode and raises a pseudo-SIGINT
 * (see picorb_hal_stdin_push in ports/rp2040/machine.c). POSIX has no
 * such interrupt: stdin is a blocking fd. We emulate the RX interrupt
 * with a dedicated reader thread that performs the only read() on fd 0,
 * intercepts Ctrl-C/Ctrl-Z, and feeds the rest into a ring buffer that
 * picorb_hal_getchar() drains without blocking. This keeps the signal
 * machinery (Machine.check_signal, sandbox.rb, pipeline.rb) identical to
 * the microcontroller path.
 *
 * Note: terminal canonical mode, echo and line editing are handled by
 * the OS terminal driver, so unlike the rp2040 port we do not reimplement
 * them here.
 *
 *------------------------------------*/

#ifndef PICORB_STDIN_BUFFER_SIZE
#define PICORB_STDIN_BUFFER_SIZE 1024
#endif

static uint8_t stdin_buf[PICORB_STDIN_BUFFER_SIZE];
static int stdin_head = 0;
static int stdin_tail = 0;
static pthread_mutex_t stdin_mutex = PTHREAD_MUTEX_INITIALIZER;
static volatile bool stdin_eof = false;

static bool
ringbuffer_push(uint8_t ch)
{
  bool pushed = false;
  pthread_mutex_lock(&stdin_mutex);
  int next = (stdin_head + 1) % PICORB_STDIN_BUFFER_SIZE;
  if (next != stdin_tail) {
    stdin_buf[stdin_head] = ch;
    stdin_head = next;
    pushed = true;
  }
  pthread_mutex_unlock(&stdin_mutex);
  return pushed;
}

static bool
ringbuffer_pop(uint8_t *ch)
{
  bool popped = false;
  pthread_mutex_lock(&stdin_mutex);
  if (stdin_head != stdin_tail) {
    *ch = stdin_buf[stdin_tail];
    stdin_tail = (stdin_tail + 1) % PICORB_STDIN_BUFFER_SIZE;
    popped = true;
  }
  pthread_mutex_unlock(&stdin_mutex);
  return popped;
}

static bool
ringbuffer_empty(void)
{
  pthread_mutex_lock(&stdin_mutex);
  bool empty = (stdin_head == stdin_tail);
  pthread_mutex_unlock(&stdin_mutex);
  return empty;
}

bool
picorb_hal_stdin_push(uint8_t ch)
{
  /* Only intercept signal chars in cooked mode (mirror rp2040). In raw
   * mode, pass all bytes through for binary data and the shell's own
   * line editor. */
  if (!io_raw_q()) {
    if (ch == 3) {
      sigint_status = MACHINE_SIGINT_RECEIVED;
      return true;  /* signal consumed, not an overflow */
    }
    if (ch == 26) {
      sigint_status = MACHINE_SIGTSTP_RECEIVED;
      return true;
    }
  }
  return ringbuffer_push(ch);
}

static void *
stdin_reader(void *arg)
{
  (void)arg;
  /* Keep the scheduler tick (SIGALRM) and terminal signals on the main
   * VM thread; this thread must only block in read(). */
  sigset_t mask;
  sigemptyset(&mask);
  sigaddset(&mask, SIGALRM);
  sigaddset(&mask, SIGINT);
  sigaddset(&mask, SIGTSTP);
  pthread_sigmask(SIG_BLOCK, &mask, NULL);

  while (true) {
    /* poll() waits for readability regardless of O_NONBLOCK, which the
     * line editor toggles on fd 0 (io_raw_bang). A plain blocking read()
     * would instead busy-spin on EAGAIN whenever the editor is active. */
    struct pollfd pfd;
    pfd.fd = STDIN_FILENO;
    pfd.events = POLLIN;
    int pr = poll(&pfd, 1, -1);
    if (pr < 0) {
      if (errno == EINTR) continue;
      stdin_eof = true;
      break;
    }
    if (pr == 0) continue;

    uint8_t ch;
    ssize_t n = read(STDIN_FILENO, &ch, 1);
    if (n == 1) {
      /* Retry while the ring buffer is full so no input byte is lost. */
      while (!picorb_hal_stdin_push(ch)) {
        struct timespec ts = {0, 1000000}; // 1ms
        nanosleep(&ts, NULL);
      }
    } else if (n == 0) {
      stdin_eof = true; // real EOF (closed pipe / Ctrl-D at the OS layer)
      break;
    } else {
      if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) continue;
      stdin_eof = true;
      break;
    }
  }
  return NULL;
}

static pthread_once_t reader_once = PTHREAD_ONCE_INIT;

static void
start_reader(void)
{
  pthread_t tid;
  if (pthread_create(&tid, NULL, stdin_reader, NULL) == 0) {
    pthread_detach(tid);
  }
}

static inline void
ensure_reader(void)
{
  pthread_once(&reader_once, start_reader);
}

int
picorb_hal_getchar(void)
{
  ensure_reader();
  if (sigint_status == MACHINE_SIGINT_RECEIVED) {
    sigint_status = MACHINE_SIG_NONE;
    return 3; // Ctrl-C
  }
  if (sigint_status == MACHINE_SIGTSTP_RECEIVED) {
    sigint_status = MACHINE_SIG_NONE;
    return 26; // Ctrl-Z
  }
  uint8_t ch;
  if (ringbuffer_pop(&ch)) {
    return (int)ch;
  }
  if (stdin_eof) {
    return HAL_GETCHAR_EOF;
  }
  return HAL_GETCHAR_NODATA;
}

int
picorb_hal_read_available(void)
{
  ensure_reader();
  if (!ringbuffer_empty()) {
    return 1;
  }
  if (sigint_status == MACHINE_SIGINT_RECEIVED ||
      sigint_status == MACHINE_SIGTSTP_RECEIVED) {
    return 1;
  }
  return stdin_eof ? 1 : 0;
}

void
Machine_tud_task(void)
{
}

bool
Machine_tud_mounted_q(void)
{
  return true;
}

void
Machine_delay_ms(uint32_t ms)
{
}

void
Machine_busy_wait_ms(uint32_t ms)
{
  struct timespec ts;
  ts.tv_sec = ms / 1000;
  ts.tv_nsec = (ms % 1000) * 1000000;
  nanosleep(&ts, NULL);
}

void
Machine_busy_wait_us(uint32_t us)
{
  struct timespec ts;
  ts.tv_sec = us / 1000000;
  ts.tv_nsec = (us % 1000000) * 1000;
  nanosleep(&ts, NULL);
}

void
Machine_sleep(uint32_t seconds)
{
}

bool
Machine_get_unique_id(char *id_str)
{
#ifdef __APPLE__
  uuid_t uuid;
  struct timespec timeout = {0, 0};
  if (gethostuuid(uuid, &timeout) == 0) {
    /* Convert 16-byte UUID to 32-character hex string */
    for (int i = 0; i < 16; i++) {
      sprintf(&id_str[i * 2], "%02x", uuid[i]);
    }
    id_str[32] = '\0';
    return true;
  }
  perror("Failed to get host UUID");
  return false;
#else
  FILE *fp = fopen("/etc/machine-id", "r");
  if (fp) {
    if (fgets(id_str, 33, fp) == NULL) {
      perror("Failed to read /etc/machine-id");
      fclose(fp);
      return false;
    }
    fclose(fp);
    return true;
  }
  perror("Failed to open /etc/machine-id");
  return false;
#endif
}

uint32_t
Machine_stack_usage(void)
{
  return 0;
}

void
Machine_exit(int status)
{
  exit(status);
}

void
Machine_reboot(void)
{
  exit_status = MACHINE_EXIT_REBOOT;
  exit(MACHINE_EXIT_REBOOT);
}

uint64_t
Machine_uptime_us(void)
{
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (uint64_t)ts.tv_sec * 1000000ULL + ts.tv_nsec / 1000;
}
