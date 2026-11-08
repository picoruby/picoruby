/*
 * nRF52 port: OpenThread over the on-chip 802.15.4 radio.
 *
 * This is the only translation unit in the gem that sees
 * OpenThread's own headers. Everything the VM bindings need is reduced to the
 * plain C in ../../include/openthread_glue.h, for the same reason
 * picoruby-ble keeps BTstack out of its own header: src/ is compiled by
 * rake from a build config with no OpenThread include paths, while this
 * file is compiled by the product Makefile, which has them.
 *
 * The stack is a hard singleton -- otInstanceInitSingle, with
 * OPENTHREAD_CONFIG_MULTIPLE_INSTANCE_ENABLE off -- so the module-level
 * Ruby API is a faithful reflection of the platform rather than a
 * simplification.
 *
 * Concurrency: OpenThread ships no locking and its API is documented as
 * not thread-safe. That requirement is met here for free rather than by
 * a mutex: otTaskletsProcess/otSysProcessDrivers run from the mruby/c
 * scheduler hook, which executes in thread context at the top of the
 * run loop, and every otXxx call reached from Ruby runs in that same
 * single VM context. The receive callback below therefore also runs in
 * VM context, not in an ISR.
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include <openthread/instance.h>
#include <openthread/thread.h>
#include <openthread/ip6.h>
#include <openthread/udp.h>
#include <openthread/message.h>
#include <openthread/dataset.h>
#include <openthread/tasklet.h>
#include <openthread/platform/entropy.h>
#include <openthread/platform/logging.h>

#include "openthread-system.h"

#include "../../include/openthread_glue.h"
#include "../../../picoruby-rng/include/rng.h"
#include "../../../picoruby-machine/include/hal.h"

/* The glue enum has to keep step with otDeviceRole; it exists only so
   the VM bindings need not include <openthread/thread.h>. */
_Static_assert((int)OT_ROLE_DISABLED == (int)OT_DEVICE_ROLE_DISABLED, "role");
_Static_assert((int)OT_ROLE_DETACHED == (int)OT_DEVICE_ROLE_DETACHED, "role");
_Static_assert((int)OT_ROLE_CHILD    == (int)OT_DEVICE_ROLE_CHILD,    "role");
_Static_assert((int)OT_ROLE_ROUTER   == (int)OT_DEVICE_ROLE_ROUTER,   "role");
_Static_assert((int)OT_ROLE_LEADER   == (int)OT_DEVICE_ROLE_LEADER,   "role");

static otInstance *instance;

/*
 * Received datagrams are copied out of the otMessage and parked here.
 *
 * The copy is not avoidable: otUdpReceive does not transfer ownership
 * ("This callback does not transfer ownership of aMessage"), the stack
 * frees the message as soon as the callback returns, and otMessage is a
 * chained buffer with no contiguous-payload accessor anyway. One
 * memcpy in and one out is still fewer than the three the rp2040
 * picoruby-socket port makes.
 *
 * Fixed slots rather than a byte ring: bounded, auditable, and the
 * original length is recorded so a caller can tell a truncated
 * datagram from a short one instead of silently getting less than was
 * sent.
 */
/* Raise from a build config if the application needs deeper queueing or
   larger datagrams; picoruby-irq guards IRQ_MAX_SOURCES the same way. */
#ifndef OT_UDP_RX_SLOTS
#define OT_UDP_RX_SLOTS 2
#endif
#ifndef OT_UDP_RX_MAX
#define OT_UDP_RX_MAX   512
#endif

typedef struct {
  uint8_t  data[OT_UDP_RX_MAX];
  uint16_t len;           /* bytes actually stored */
  uint16_t original_len;  /* bytes on the wire; > len means truncated */
  uint16_t peer_port;
  char     peer[OT_ADDR_STRLEN];
  bool     used;
} ot_udp_slot_t;

struct OT_udp {
  otUdpSocket   socket;
  ot_udp_slot_t slot[OT_UDP_RX_SLOTS];
  uint8_t       head;      /* next slot to hand out */
  uint8_t       tail;      /* next slot to fill */
  uint32_t      dropped;   /* datagrams lost because every slot was full */
  bool          open;
};

/*
 * Sockets are allocated on open, not reserved.
 *
 * A file-static socket would cost its slots -- OT_UDP_RX_SLOTS *
 * OT_UDP_RX_MAX plus metadata, about 1 kB -- in .bss whether or not a
 * program ever opens one, which is the opposite of frugal on a part
 * whose attribute table is also competing for RAM. Allocating on demand
 * costs nothing until used, and lets a node run more than one port:
 * CoAP alongside an application socket is an ordinary Thread
 * arrangement, not an exotic one.
 *
 * The count is bounded so a leak shows up as a refused open rather than
 * as a heap that quietly consumes the stack.
 */
#ifndef OT_UDP_MAX_SOCKETS
#define OT_UDP_MAX_SOCKETS 2
#endif
static uint8_t udp_open_count;

static void
addr_to_string(const otIp6Address *addr, char *buf, size_t len)
{
  otIp6AddressToString(addr, buf, (uint16_t)len);
}

static void
udp_receive(void *context, otMessage *message, const otMessageInfo *info)
{
  OT_udp_t *sock = (OT_udp_t *)context;
  if (sock == NULL || !sock->open) {
    return;
  }

  ot_udp_slot_t *slot = &sock->slot[sock->tail];
  if (slot->used) {
    sock->dropped++;   /* nobody drained; the newest datagram is lost */
    return;
  }

  uint16_t offset = otMessageGetOffset(message);
  uint16_t length = otMessageGetLength(message) - offset;

  slot->original_len = length;
  if (length > OT_UDP_RX_MAX) {
    length = OT_UDP_RX_MAX;
  }
  otMessageRead(message, offset, slot->data, length);
  slot->len = length;
  slot->peer_port = info->mPeerPort;
  addr_to_string(&info->mPeerAddr, slot->peer, sizeof(slot->peer));
  slot->used = true;

  sock->tail = (uint8_t)((sock->tail + 1) % OT_UDP_RX_SLOTS);
  /* Deliberately no free: the stack owns the message. */
}

/*
 * Registered once, from OT_init. The mruby/c scheduler hook runs this at
 * the top of every run-loop iteration, in thread context, which is what
 * makes the whole no-locking argument above hold. The contract is
 * "cheap, never sleep, never re-enter the scheduler"; both calls below
 * poll and return.
 */
/*
 * Waking Ruby on a role change rather than making it ask.
 *
 * otSetStateChangedCallback fires from otTaskletsProcess -- i.e. from
 * the scheduler hook, in VM context -- so pushing straight into a
 * Task::Queue here is safe and needs no ISR-side latch. This is what
 * lets OpenThread#wait_attached block on the queue instead of polling a
 * predicate every 100 ms.
 */
static void
ot_state_changed(otChangedFlags flags, void *context)
{
  (void)context;
  if (flags & OT_CHANGED_THREAD_ROLE) {
    OT_push_role_change((int)otThreadGetDeviceRole(instance));
  }
}

/*
 * Set by otSysEventSignalPending, which the radio driver calls when it
 * leaves work for otSysProcessDrivers -- from its ISRs, and also from
 * thread context when a sleep or energy-detect request has to be
 * retried. The ISR case wakes the idle hook by itself; the retry case
 * raises no interrupt, so without this flag the core would sleep on it
 * until the next tick. Cleared before the drivers run so a signal raised
 * while they run is not lost.
 */
static volatile bool driver_event_pending;

void
otSysEventSignalPending(void)
{
  driver_event_pending = true;
}

static void
ot_scheduler_service(void *ud)
{
  (void)ud;
  if (instance == NULL) {
    return;
  }
  driver_event_pending = false;
  otTaskletsProcess(instance);
  otSysProcessDrivers(instance);
}

void
OT_process(void)
{
  ot_scheduler_service(NULL);
}

bool
OT_work_pending(void)
{
  return instance != NULL &&
         (driver_event_pending || otTaskletsArePending(instance));
}

bool
OT_init(void)
{
  if (instance != NULL) {
    return true;   /* idempotent: the stack is a singleton */
  }
  otSysInit(0, NULL);
  instance = otInstanceInitSingle();
  if (instance == NULL) {
    return false;
  }
  otSetStateChangedCallback(instance, ot_state_changed, NULL);
  picorb_scheduler_service_add(ot_scheduler_service, NULL);
  return true;
}

bool
OT_started(void)
{
  return instance != NULL;
}

bool
OT_set_dataset_tlvs(const uint8_t *tlvs, size_t len)
{
  otOperationalDatasetTlvs dataset;

  if (instance == NULL || tlvs == NULL) {
    return false;
  }
  if (len == 0 || len > sizeof(dataset.mTlvs)) {
    return false;
  }
  memcpy(dataset.mTlvs, tlvs, len);
  dataset.mLength = (uint8_t)len;
  return otDatasetSetActiveTlvs(instance, &dataset) == OT_ERROR_NONE;
}

/*
 * Build the Active Dataset from the three fields a program actually
 * has to supply, and hand it over as TLVs.
 *
 * The TLV form rather than otOperationalDataset: that struct signals
 * field presence through a separate mComponents sub-struct of booleans,
 * and a binding that forgets one gets the field silently ignored. TLVs
 * have no such trap -- what is written is what is set.
 */
#define OT_TLV_CHANNEL         0
#define OT_TLV_PANID           1
#define OT_TLV_EXTENDED_PANID  2
#define OT_TLV_NETWORK_NAME    3
#define OT_TLV_NETWORK_KEY     5
#define OT_TLV_ACTIVE_TIMESTAMP 14

/*
 * ActiveDatasetManager::IsCommissioned requires NetworkKey, NetworkName,
 * ExtendedPanId, PanId and Channel (dataset_manager.cpp), and IsComplete
 * additionally requires a valid network timestamp -- without an Active
 * Timestamp TLV a node that is first up never forms a network, it just
 * sits detached. otDatasetSetActiveTlvs does not check any of that: its
 * validation is per-TLV bounds only, so an incomplete dataset is stored
 * and reported as success. Emit the full required set.
 */
#define OT_DEFAULT_NETWORK_NAME "picoruby"

bool
OT_set_dataset(const uint8_t key[16], int pan_id, int channel)
{
  uint8_t tlvs[64];
  size_t n = 0;

  if (key == NULL) {
    return false;
  }

  /* Seconds = 1, ticks = 0, not authoritative. Any valid timestamp will
     do for a node forming its own network; a node joining an existing
     one is handed the leader's dataset and overwrites this. */
  tlvs[n++] = OT_TLV_ACTIVE_TIMESTAMP;
  tlvs[n++] = 8;
  tlvs[n++] = 0; tlvs[n++] = 0; tlvs[n++] = 0;
  tlvs[n++] = 0; tlvs[n++] = 0; tlvs[n++] = 1;
  tlvs[n++] = 0; tlvs[n++] = 0;
  if (channel >= 0) {
    if (channel < 11 || 26 < channel) {
      return false;    /* 2.4 GHz 802.15.4 channels */
    }
    tlvs[n++] = OT_TLV_CHANNEL;
    tlvs[n++] = 3;
    tlvs[n++] = 0;                             /* channel page 0 */
    tlvs[n++] = 0;
    tlvs[n++] = (uint8_t)channel;
  }
  if (pan_id >= 0) {
    if (pan_id > 0xFFFF) {
      return false;
    }
    tlvs[n++] = OT_TLV_PANID;
    tlvs[n++] = 2;
    tlvs[n++] = (uint8_t)(pan_id >> 8);        /* big endian, per spec */
    tlvs[n++] = (uint8_t)(pan_id & 0xFF);
  }
  /* Derived from the PAN ID so two boards configured alike agree, and so
     the value is stable across reboots. */
  tlvs[n++] = OT_TLV_EXTENDED_PANID;
  tlvs[n++] = 8;
  tlvs[n++] = 0xde; tlvs[n++] = 0xad; tlvs[n++] = 0x00; tlvs[n++] = 0xbe;
  tlvs[n++] = 0xef; tlvs[n++] = 0x00;
  tlvs[n++] = (uint8_t)((pan_id >= 0 ? pan_id : 0) >> 8);
  tlvs[n++] = (uint8_t)((pan_id >= 0 ? pan_id : 0) & 0xFF);

  {
    const char *name = OT_DEFAULT_NETWORK_NAME;
    size_t name_len = strlen(name);
    tlvs[n++] = OT_TLV_NETWORK_NAME;
    tlvs[n++] = (uint8_t)name_len;
    memcpy(&tlvs[n], name, name_len);
    n += name_len;
  }

  tlvs[n++] = OT_TLV_NETWORK_KEY;
  tlvs[n++] = 16;
  memcpy(&tlvs[n], key, 16);
  n += 16;

  return OT_set_dataset_tlvs(tlvs, n);
}

bool
OT_ip6_enable(bool on)
{
  return instance != NULL && otIp6SetEnabled(instance, on) == OT_ERROR_NONE;
}

bool
OT_thread_enable(bool on)
{
  /* otThreadSetEnabled returns OT_ERROR_INVALID_STATE unless the netif
     is already up, which is why Ruby's #start enables IPv6 first. */
  return instance != NULL && otThreadSetEnabled(instance, on) == OT_ERROR_NONE;
}

int
OT_role(void)
{
  if (instance == NULL) {
    return OT_ROLE_DISABLED;
  }
  return (int)otThreadGetDeviceRole(instance);
}

bool
OT_eid(char *buf, size_t len)
{
  if (instance == NULL || buf == NULL || len < OT_ADDR_STRLEN) {
    return false;
  }
  addr_to_string(otThreadGetMeshLocalEid(instance), buf, len);
  return true;
}

int
OT_address_count(void)
{
  int count = 0;

  if (instance == NULL) {
    return 0;
  }
  for (const otNetifAddress *a = otIp6GetUnicastAddresses(instance);
       a != NULL; a = a->mNext) {
    count++;
  }
  return count;
}

bool
OT_address_at(int index, char *buf, size_t len)
{
  int i = 0;

  if (instance == NULL || buf == NULL || len < OT_ADDR_STRLEN || index < 0) {
    return false;
  }
  for (const otNetifAddress *a = otIp6GetUnicastAddresses(instance);
       a != NULL; a = a->mNext, i++) {
    if (i == index) {
      addr_to_string(&a->mAddress, buf, len);
      return true;
    }
  }
  return false;
}

OT_udp_t *
OT_udp_open(void)
{
  if (instance == NULL || udp_open_count >= OT_UDP_MAX_SOCKETS) {
    return NULL;
  }
  OT_udp_t *sock = calloc(1, sizeof(*sock));
  if (sock == NULL) {
    return NULL;
  }
  if (otUdpOpen(instance, &sock->socket, udp_receive, sock) != OT_ERROR_NONE) {
    free(sock);
    return NULL;
  }
  sock->open = true;
  udp_open_count++;
  return sock;
}

bool
OT_udp_bind(OT_udp_t *sock, int port)
{
  otSockAddr addr;

  if (instance == NULL || sock == NULL || !sock->open) {
    return false;
  }
  if (port < 0 || 65535 < port) {
    return false;
  }
  memset(&addr, 0, sizeof(addr));   /* unspecified address: any */
  addr.mPort = (uint16_t)port;
  return otUdpBind(instance, &sock->socket, &addr, OT_NETIF_THREAD_HOST)
           == OT_ERROR_NONE;
}

int
OT_udp_sendto(OT_udp_t *sock, const void *data, size_t len,
              const char *host, int port)
{
  otMessageInfo info;
  otMessage *message;

  if (instance == NULL || sock == NULL || !sock->open || host == NULL) {
    return -1;
  }
  if (port < 0 || 65535 < port) {
    return -1;
  }
  memset(&info, 0, sizeof(info));
  if (otIp6AddressFromString(host, &info.mPeerAddr) != OT_ERROR_NONE) {
    return -1;    /* IPv6 literal only: Thread has no IPv4 and this port
                     enables no DNS client */
  }
  info.mPeerPort = (uint16_t)port;

  message = otUdpNewMessage(instance, NULL);
  if (message == NULL) {
    return -1;
  }
  if (otMessageAppend(message, data, (uint16_t)len) != OT_ERROR_NONE) {
    otMessageFree(message);
    return -1;
  }
  /* otUdpSend takes ownership only when it returns OT_ERROR_NONE; on
     every other path the caller still owns the message. */
  if (otUdpSend(instance, &sock->socket, message, &info) != OT_ERROR_NONE) {
    otMessageFree(message);
    return -1;
  }
  return (int)len;
}

int
OT_udp_recvfrom(OT_udp_t *sock, void *buf, size_t maxlen,
                char *host, size_t hostlen, int *port, int *original_len)
{
  if (sock == NULL || !sock->open) {
    return OT_RECV_CLOSED;
  }

  ot_udp_slot_t *slot = &sock->slot[sock->head];
  if (!slot->used) {
    return OT_RECV_WOULD_BLOCK;
  }

  size_t n = slot->len;
  if (n > maxlen) {
    n = maxlen;
  }
  memcpy(buf, slot->data, n);
  if (host != NULL && hostlen > 0) {
    size_t hn = strlen(slot->peer);
    if (hn >= hostlen) {
      hn = hostlen - 1;
    }
    memcpy(host, slot->peer, hn);
    host[hn] = '\0';
  }
  if (port != NULL) {
    *port = slot->peer_port;
  }
  if (original_len != NULL) {
    *original_len = slot->original_len;
  }

  slot->used = false;
  sock->head = (uint8_t)((sock->head + 1) % OT_UDP_RX_SLOTS);
  return (int)n;
}

int
OT_udp_peek_len(OT_udp_t *sock)
{
  if (sock == NULL || !sock->open) {
    return OT_RECV_CLOSED;
  }
  const ot_udp_slot_t *slot = &sock->slot[sock->head];
  return slot->used ? (int)slot->original_len : OT_RECV_WOULD_BLOCK;
}

uint32_t
OT_udp_dropped(OT_udp_t *sock)
{
  return (sock == NULL) ? 0 : sock->dropped;
}

/*
 * Frees the handle. Both VM bindings drop their pointer in the same
 * breath -- #close and the destructor each null it -- so the caller
 * cannot reach freed storage; udp_handle raises IOError on the null
 * instead.
 */
void
OT_udp_close(OT_udp_t *sock)
{
  if (instance == NULL || sock == NULL || !sock->open) {
    return;
  }
  otUdpClose(instance, &sock->socket);
  sock->open = false;
  if (udp_open_count > 0) {
    udp_open_count--;
  }
  free(sock);
}

/*
 * picoruby-rng's weak hook.
 *
 * OpenThread's entropy driver keeps NRF_RNG free-running behind a pool
 * and stops it only when that pool is full, so a second driver poking
 * START/STOP on the same peripheral can leave it stopped with an empty
 * pool -- a state otPlatEntropyGet cannot recover from, because it
 * restarts the generator only on the branch where the pool was
 * non-empty. Answering from the pool instead removes the contention
 * rather than trying to arbitrate it.
 *
 * Before otSysInit the peripheral is nobody's, so the weak default (a
 * direct read) is still what runs.
 */
bool
rng_platform_random_bytes(uint8_t *out, size_t len)
{
  size_t done = 0;

  if (instance == NULL || out == NULL || len == 0) {
    return false;
  }
  /* otPlatEntropyGet takes a uint16_t length. A larger request cast
     down would fill only its low part and report success, handing the
     rest of the buffer -- uninitialised heap -- back as random bytes.
     Note there is no spin budget on this path: otPlatEntropyGet waits
     on its pool without a bound, as the vendor driver does. */
  while (done < len) {
    size_t chunk = len - done;
    if (chunk > UINT16_MAX) {
      chunk = UINT16_MAX;
    }
    if (otPlatEntropyGet(out + done, (uint16_t)chunk) != OT_ERROR_NONE) {
      return false;
    }
    done += chunk;
  }
  return true;
}

/*
 * Platform obligations OpenThread links against but this product does
 * not use.
 *
 * The transport quartet belongs to the CLI/NCP serial front end: this
 * port drives OpenThread from Ruby, so there is nothing to attach. They
 * are defined rather than excluded because the platform library
 * references them unconditionally.
 */
void nrf5TransportInit(bool aPseudoReset) { (void)aPseudoReset; }
void nrf5TransportDeinit(bool aPseudoReset) { (void)aPseudoReset; }
void nrf5TransportProcess(void) {}
bool nrf5TransportPseudoResetRequired(void) { return false; }

/* Defined for completeness rather than necessity: with OT_LOG_OUTPUT=NONE
   nothing references it and --gc-sections drops it from the image
   entirely. Kept so a build that turns logging on links, and pointed at
   nowhere rather than at the USB CDC console, which the shell owns. */
void
otPlatLog(otLogLevel level, otLogRegion region, const char *format, ...)
{
  (void)level;
  (void)region;
  (void)format;
}
