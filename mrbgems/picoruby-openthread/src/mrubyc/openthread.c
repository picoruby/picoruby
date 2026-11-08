#include <mrubyc.h>
#include <alloc.h>
#include <string.h>

#include "../../include/openthread_glue.h"

/*
 * The Ruby-visible half. Everything here is plain glue: argument
 * checking, String <-> buffer, and turning the port's booleans into
 * exceptions. The stack itself is behind ../../include/openthread_glue.h,
 * so this file compiles without any OpenThread include path -- which is
 * required, because rake builds it from a config that has none.
 */

/*
 * Role changes are delivered through a Task::Queue so a waiting task is
 * parked rather than spinning on a predicate. Bounded like
 * picoruby-ble's event queue: a role that flaps while nobody is
 * listening must not grow the heap.
 */
#define OT_MAX_PENDING_EVENTS 8
static mrbc_value role_queue;
static uint8_t pending_role_events;

void
OT_push_role_change(int role)
{
  if (role_queue.tt == MRBC_TT_NIL ||
      OT_MAX_PENDING_EVENTS <= pending_role_events) {
    return;
  }
  mrbc_value event = mrbc_integer_value(role);
  if (mrbc_task_queue_push(&role_queue, &event) == MRBC_TASK_QUEUE_PUSH_OK) {
    pending_role_events++;
  }
}

/* OpenThread.__role_queue = q -- the Ruby side owns the object. */
static void
c_ot_set_role_queue(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (argc != 1) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "expected a Task::Queue");
    return;
  }
  mrbc_incref(&GET_ARG(1));
  role_queue = GET_ARG(1);
  pending_role_events = 0;
  SET_NIL_RETURN();
}

static void
c_ot_role_event_popped(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (0 < pending_role_events) pending_role_events--;
  SET_NIL_RETURN();
}

static void
c_ot_init(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (!OT_init()) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "OpenThread init failed");
    return;
  }
  SET_TRUE_RETURN();
}

static void
c_ot_started_q(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (OT_started()) SET_TRUE_RETURN(); else SET_FALSE_RETURN();
}

/* OpenThread.__set_dataset_tlvs(String) */
static void
c_ot_set_dataset_tlvs(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (argc != 1 || GET_ARG(1).tt != MRBC_TT_STRING) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "expected a TLV String");
    return;
  }
  mrbc_value tlvs = GET_ARG(1);
  if (!OT_set_dataset_tlvs((const uint8_t *)tlvs.string->data,
                           (size_t)tlvs.string->size)) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "invalid Active Dataset TLVs");
    return;
  }
  SET_TRUE_RETURN();
}

/* OpenThread.__set_dataset(key16, pan_id, channel) -- -1 means "omit" */
static void
c_ot_set_dataset(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (argc != 3 || GET_ARG(1).tt != MRBC_TT_STRING ||
      GET_ARG(2).tt != MRBC_TT_INTEGER || GET_ARG(3).tt != MRBC_TT_INTEGER) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong arguments");
    return;
  }
  mrbc_value key = GET_ARG(1);
  if (key.string->size != 16) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "network key must be 16 bytes");
    return;
  }
  if (!OT_set_dataset((const uint8_t *)key.string->data,
                      (int)GET_ARG(2).i, (int)GET_ARG(3).i)) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "invalid dataset");
    return;
  }
  SET_TRUE_RETURN();
}

static void
c_ot_ip6_enable(mrbc_vm *vm, mrbc_value *v, int argc)
{
  bool on = (argc == 1) ? (GET_ARG(1).tt != MRBC_TT_FALSE &&
                           GET_ARG(1).tt != MRBC_TT_NIL) : true;
  if (!OT_ip6_enable(on)) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "otIp6SetEnabled failed");
    return;
  }
  SET_TRUE_RETURN();
}

static void
c_ot_thread_enable(mrbc_vm *vm, mrbc_value *v, int argc)
{
  bool on = (argc == 1) ? (GET_ARG(1).tt != MRBC_TT_FALSE &&
                           GET_ARG(1).tt != MRBC_TT_NIL) : true;
  if (!OT_thread_enable(on)) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "otThreadSetEnabled failed");
    return;
  }
  SET_TRUE_RETURN();
}

static void
c_ot_role(mrbc_vm *vm, mrbc_value *v, int argc)
{
  SET_INT_RETURN(OT_role());
}

static void
c_ot_eid(mrbc_vm *vm, mrbc_value *v, int argc)
{
  char buf[OT_ADDR_STRLEN];
  if (!OT_eid(buf, sizeof(buf))) {
    SET_NIL_RETURN();
    return;
  }
  mrbc_value s = mrbc_string_new_cstr(vm, buf);
  SET_RETURN(s);
}

static void
c_ot_addresses(mrbc_vm *vm, mrbc_value *v, int argc)
{
  int n = OT_address_count();
  mrbc_value ary = mrbc_array_new(vm, n);
  for (int i = 0; i < n; i++) {
    char buf[OT_ADDR_STRLEN];
    if (!OT_address_at(i, buf, sizeof(buf))) {
      continue;
    }
    mrbc_value s = mrbc_string_new_cstr(vm, buf);
    mrbc_array_push(&ary, &s);
  }
  SET_RETURN(ary);
}

/*
 * UDP. The socket handle lives in the instance's data pointer; the port
 * keeps the storage, so there is nothing to free here and no layout for
 * the two sides to disagree about.
 */
/* instance->data is storage, not a pointer field (it is a flexible
   array member), so the handle is kept *in* it. */
static OT_udp_t *
udp_handle(mrbc_vm *vm, mrbc_value *v)
{
  OT_udp_t *sock = *(OT_udp_t **)v->instance->data;
  if (sock == NULL) {
    mrbc_raise(vm, MRBC_CLASS(IOError), "closed socket");
  }
  return sock;
}

static void
c_udp_new(mrbc_vm *vm, mrbc_value *v, int argc)
{
  OT_udp_t *sock = OT_udp_open();
  if (sock == NULL) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError),
               "cannot open a UDP socket (out of sockets, or Thread not started)");
    return;
  }
  mrbc_value obj = mrbc_instance_new(vm, v->cls, sizeof(OT_udp_t *));
  *(OT_udp_t **)obj.instance->data = sock;
  SET_RETURN(obj);
}

static void
c_udp_bind(mrbc_vm *vm, mrbc_value *v, int argc)
{
  /* bind(port) or bind(host, port); the host is accepted and ignored --
     a Thread node has no address to choose between before it attaches. */
  int port;
  if (argc == 1 && GET_ARG(1).tt == MRBC_TT_INTEGER) {
    port = (int)GET_ARG(1).i;
  } else if (argc == 2 && GET_ARG(1).tt == MRBC_TT_STRING &&
             GET_ARG(2).tt == MRBC_TT_INTEGER) {
    port = (int)GET_ARG(2).i;
  } else {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "bind(port) or bind(host, port)");
    return;
  }
  OT_udp_t *sock = udp_handle(vm, v);
  if (sock == NULL) return;
  if (!OT_udp_bind(sock, port)) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "otUdpBind failed");
    return;
  }
  SET_INT_RETURN(0);
}

/* send(data, flags, host, port) -- CRuby's 4-arg UDPSocket#send */
static void
c_udp_send(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (argc != 4 || GET_ARG(1).tt != MRBC_TT_STRING ||
      GET_ARG(3).tt != MRBC_TT_STRING || GET_ARG(4).tt != MRBC_TT_INTEGER) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "send(data, flags, host, port)");
    return;
  }
  OT_udp_t *sock = udp_handle(vm, v);
  if (sock == NULL) return;
  mrbc_value data = GET_ARG(1);
  mrbc_value host = GET_ARG(3);
  int sent = OT_udp_sendto(sock, data.string->data, (size_t)data.string->size,
                           (const char *)host.string->data, (int)GET_ARG(4).i);
  if (sent < 0) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "otUdpSend failed");
    return;
  }
  SET_INT_RETURN(sent);
}

/* recvfrom_nonblock(maxlen) -> [data, [af, port, host, host]] | nil */
static void
c_udp_recvfrom_nonblock(mrbc_vm *vm, mrbc_value *v, int argc)
{
  int maxlen = 512;
  if (argc >= 1) {
    if (GET_ARG(1).tt != MRBC_TT_INTEGER) {
      mrbc_raise(vm, MRBC_CLASS(ArgumentError), "recvfrom_nonblock(maxlen)");
      return;
    }
    maxlen = (int)GET_ARG(1).i;
  }
  if (maxlen <= 0) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "maxlen must be positive");
    return;
  }
  OT_udp_t *sock = udp_handle(vm, v);
  if (sock == NULL) return;

  /* Ask before taking: recvfrom clears the slot, so a truncation raised
     after the fact would have already thrown the datagram away. */
  int wire = OT_udp_peek_len(sock);
  if (wire == OT_RECV_CLOSED) {
    mrbc_raise(vm, MRBC_CLASS(IOError), "closed socket");
    return;
  }
  if (wire == OT_RECV_WOULD_BLOCK) {
    SET_NIL_RETURN();
    return;
  }
  if (wire > maxlen) {
    mrbc_raisef(vm, MRBC_CLASS(RuntimeError),
                "datagram of %d bytes needs a larger maxlen than %d",
                wire, maxlen);
    return;   /* the datagram stays queued; retry with a bigger buffer */
  }

  uint8_t *buf = mrbc_alloc(vm, (unsigned int)maxlen);
  if (buf == NULL) {
    mrbc_raise(vm, MRBC_CLASS(NoMemoryError), "out of memory");
    return;
  }
  char host[OT_ADDR_STRLEN];
  int port = 0;
  int n = OT_udp_recvfrom(sock, buf, (size_t)maxlen, host, sizeof(host),
                          &port, NULL);
  if (n < 0) {
    mrbc_free(vm, buf);
    SET_NIL_RETURN();      /* raced with a close, or drained meanwhile */
    return;
  }

  mrbc_value data = mrbc_string_new(vm, buf, n);
  mrbc_free(vm, buf);

  /* Same shape as picoruby-socket's UDPSocket, except the family is
     AF_INET6 -- Thread has no IPv4. */
  mrbc_value addr = mrbc_array_new(vm, 4);
  mrbc_value af   = mrbc_string_new_cstr(vm, "AF_INET6");
  mrbc_value p    = mrbc_integer_value(port);
  mrbc_value h1   = mrbc_string_new_cstr(vm, host);
  mrbc_value h2   = mrbc_string_new_cstr(vm, host);
  mrbc_array_push(&addr, &af);
  mrbc_array_push(&addr, &p);
  mrbc_array_push(&addr, &h1);
  mrbc_array_push(&addr, &h2);

  mrbc_value ret = mrbc_array_new(vm, 2);
  mrbc_array_push(&ret, &data);
  mrbc_array_push(&ret, &addr);
  SET_RETURN(ret);
}

static void
c_udp_dropped(mrbc_vm *vm, mrbc_value *v, int argc)
{
  OT_udp_t *sock = *(OT_udp_t **)v->instance->data;
  SET_INT_RETURN(sock == NULL ? 0 : (int)OT_udp_dropped(sock));
}

/*
 * Closed on collection as well as on #close. Without this the socket --
 * which the port keeps as a singleton -- would stay open for the rest of
 * the boot after a program dropped its last reference, and every later
 * UDP.new would raise.
 */
static void
udp_destructor(mrbc_value *v)
{
  OT_udp_t *sock = *(OT_udp_t **)v->instance->data;
  if (sock != NULL) {
    OT_udp_close(sock);
    *(OT_udp_t **)v->instance->data = NULL;
  }
}

static void
c_udp_close(mrbc_vm *vm, mrbc_value *v, int argc)
{
  OT_udp_t *sock = *(OT_udp_t **)v->instance->data;
  if (sock != NULL) {
    OT_udp_close(sock);
    *(OT_udp_t **)v->instance->data = NULL;
  }
  SET_NIL_RETURN();
}

void
mrbc_openthread_init(mrbc_vm *vm)
{
  mrbc_class *mod = mrbc_define_module(vm, "OpenThread");

  mrbc_define_method(vm, mod, "__init",             c_ot_init);
  mrbc_define_method(vm, mod, "started?",           c_ot_started_q);
  mrbc_define_method(vm, mod, "__set_dataset_tlvs", c_ot_set_dataset_tlvs);
  mrbc_define_method(vm, mod, "__set_dataset",      c_ot_set_dataset);
  mrbc_define_method(vm, mod, "__ip6_enable",       c_ot_ip6_enable);
  mrbc_define_method(vm, mod, "__thread_enable",    c_ot_thread_enable);
  mrbc_define_method(vm, mod, "__role",             c_ot_role);
  mrbc_define_method(vm, mod, "eid",                c_ot_eid);
  mrbc_define_method(vm, mod, "addresses",          c_ot_addresses);
  mrbc_define_method(vm, mod, "__role_queue=",      c_ot_set_role_queue);
  mrbc_define_method(vm, mod, "__role_event_popped", c_ot_role_event_popped);

  mrbc_class *udp = mrbc_define_class_under(vm, mod, "UDP", mrbc_class_object);
  mrbc_define_method(vm, udp, "new",                c_udp_new);
  mrbc_define_method(vm, udp, "bind",               c_udp_bind);
  mrbc_define_method(vm, udp, "send",               c_udp_send);
  mrbc_define_method(vm, udp, "recvfrom_nonblock",  c_udp_recvfrom_nonblock);
  mrbc_define_method(vm, udp, "close",              c_udp_close);
  mrbc_define_method(vm, udp, "dropped",            c_udp_dropped);
  mrbc_define_destructor(udp, udp_destructor);
}
