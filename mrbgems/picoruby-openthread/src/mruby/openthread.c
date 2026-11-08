#include <string.h>

#include "mruby.h"
#include "mruby/class.h"
#include "mruby/data.h"
#include "mruby/string.h"
#include "mruby/array.h"
#include "mruby/presym.h"

#include "../../include/openthread_glue.h"

/*
 * mruby half of the same glue as src/mrubyc/openthread.c. The port's
 * storage is static, so the Data pointer is a borrowed handle and the
 * type carries no free function -- there is nothing to release, and a
 * dfree would double-close the singleton socket.
 */
/* The port owns the storage, so collection closes the socket rather than
   freeing it -- otherwise the singleton stays open for the whole boot
   once a program drops its last reference. */
static void
ot_udp_dfree(mrb_state *mrb, void *ptr)
{
  if (ptr != NULL) {
    OT_udp_close((OT_udp_t *)ptr);
  }
}

static const struct mrb_data_type ot_udp_type = { "OpenThread::UDP", ot_udp_dfree };

/*
 * Role changes go into the Task::Queue the Ruby side hands over, exactly
 * as on the mruby/c arm -- mrblib calls __role_queue= and
 * __role_event_popped and must find them on both VMs. mruby-task
 * supplies Queue here; the gem depends on it when build.picoruby?.
 *
 * Bounded like picoruby-ble's event queue: a role that flaps while
 * nobody is listening must not grow the heap.
 */
#define OT_MAX_PENDING_EVENTS 8
static mrb_state *queue_mrb;
static mrb_value role_queue;
static uint8_t pending_role_events;

void
OT_push_role_change(int role)
{
  if (queue_mrb == NULL || mrb_nil_p(role_queue) ||
      OT_MAX_PENDING_EVENTS <= pending_role_events) {
    return;
  }
  mrb_funcall_id(queue_mrb, role_queue, MRB_SYM(push), 1,
                 mrb_fixnum_value(role));
  pending_role_events++;
}

static mrb_value
mrb_ot_set_role_queue(mrb_state *mrb, mrb_value self)
{
  mrb_value q;
  mrb_get_args(mrb, "o", &q);
  queue_mrb = mrb;
  role_queue = q;
  mrb_gc_register(mrb, q);   /* the C side keeps a reference */
  pending_role_events = 0;
  return mrb_nil_value();
}

static mrb_value
mrb_ot_role_event_popped(mrb_state *mrb, mrb_value self)
{
  if (0 < pending_role_events) pending_role_events--;
  return mrb_nil_value();
}

static mrb_value
mrb_ot_init(mrb_state *mrb, mrb_value self)
{
  if (!OT_init()) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "OpenThread init failed");
  }
  return mrb_true_value();
}

static mrb_value
mrb_ot_started_q(mrb_state *mrb, mrb_value self)
{
  return mrb_bool_value(OT_started());
}

static mrb_value
mrb_ot_set_dataset_tlvs(mrb_state *mrb, mrb_value self)
{
  const char *tlvs;
  mrb_int len;

  mrb_get_args(mrb, "s", &tlvs, &len);
  if (!OT_set_dataset_tlvs((const uint8_t *)tlvs, (size_t)len)) {
    mrb_raise(mrb, E_ARGUMENT_ERROR, "invalid Active Dataset TLVs");
  }
  return mrb_true_value();
}

static mrb_value
mrb_ot_set_dataset(mrb_state *mrb, mrb_value self)
{
  const char *key;
  mrb_int keylen, pan_id, channel;

  mrb_get_args(mrb, "sii", &key, &keylen, &pan_id, &channel);
  if (keylen != 16) {
    mrb_raise(mrb, E_ARGUMENT_ERROR, "network key must be 16 bytes");
  }
  if (!OT_set_dataset((const uint8_t *)key, (int)pan_id, (int)channel)) {
    mrb_raise(mrb, E_ARGUMENT_ERROR, "invalid dataset");
  }
  return mrb_true_value();
}

static mrb_value
mrb_ot_ip6_enable(mrb_state *mrb, mrb_value self)
{
  mrb_bool on = TRUE;
  mrb_get_args(mrb, "|b", &on);
  if (!OT_ip6_enable(on)) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "otIp6SetEnabled failed");
  }
  return mrb_true_value();
}

static mrb_value
mrb_ot_thread_enable(mrb_state *mrb, mrb_value self)
{
  mrb_bool on = TRUE;
  mrb_get_args(mrb, "|b", &on);
  if (!OT_thread_enable(on)) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "otThreadSetEnabled failed");
  }
  return mrb_true_value();
}

static mrb_value
mrb_ot_role(mrb_state *mrb, mrb_value self)
{
  return mrb_fixnum_value(OT_role());
}

static mrb_value
mrb_ot_eid(mrb_state *mrb, mrb_value self)
{
  char buf[OT_ADDR_STRLEN];
  if (!OT_eid(buf, sizeof(buf))) {
    return mrb_nil_value();
  }
  return mrb_str_new_cstr(mrb, buf);
}

static mrb_value
mrb_ot_addresses(mrb_state *mrb, mrb_value self)
{
  int n = OT_address_count();
  mrb_value ary = mrb_ary_new_capa(mrb, n);
  for (int i = 0; i < n; i++) {
    char buf[OT_ADDR_STRLEN];
    if (!OT_address_at(i, buf, sizeof(buf))) {
      continue;
    }
    mrb_ary_push(mrb, ary, mrb_str_new_cstr(mrb, buf));
  }
  return ary;
}

static OT_udp_t *
udp_handle(mrb_state *mrb, mrb_value self)
{
  OT_udp_t *sock = (OT_udp_t *)DATA_PTR(self);
  if (sock == NULL) {
    mrb_raise(mrb, E_IO_ERROR, "closed socket");
  }
  return sock;
}

static mrb_value
mrb_udp_initialize(mrb_state *mrb, mrb_value self)
{
  OT_udp_t *sock = OT_udp_open();
  if (sock == NULL) {
    mrb_raise(mrb, E_RUNTIME_ERROR,
              "cannot open a UDP socket (out of sockets, or Thread not started)");
  }
  mrb_data_init(self, sock, &ot_udp_type);
  return self;
}

static mrb_value
mrb_udp_bind(mrb_state *mrb, mrb_value self)
{
  mrb_value a, b = mrb_nil_value();
  mrb_int port;

  mrb_get_args(mrb, "o|o", &a, &b);
  /* bind(port) or bind(host, port); the host is accepted and ignored. */
  {
    mrb_value pv = mrb_nil_p(b) ? a : b;
    if (!mrb_integer_p(pv)) {
      mrb_raise(mrb, E_TYPE_ERROR, "port must be an Integer");
    }
    port = mrb_integer(pv);
  }
  if (!OT_udp_bind(udp_handle(mrb, self), (int)port)) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "otUdpBind failed");
  }
  return mrb_fixnum_value(0);
}

static mrb_value
mrb_udp_send(mrb_state *mrb, mrb_value self)
{
  const char *data, *host;
  mrb_int len, hostlen, flags, port;

  mrb_get_args(mrb, "sisi", &data, &len, &flags, &host, &hostlen, &port);
  (void)flags;
  (void)hostlen;
  int sent = OT_udp_sendto(udp_handle(mrb, self), data, (size_t)len,
                           host, (int)port);
  if (sent < 0) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "otUdpSend failed");
  }
  return mrb_fixnum_value(sent);
}

static mrb_value
mrb_udp_recvfrom_nonblock(mrb_state *mrb, mrb_value self)
{
  mrb_int maxlen = 512;
  mrb_get_args(mrb, "|i", &maxlen);
  if (maxlen <= 0) {
    mrb_raise(mrb, E_ARGUMENT_ERROR, "maxlen must be positive");
  }

  OT_udp_t *sock = udp_handle(mrb, self);

  /* Peek before consuming: recvfrom clears the slot, so raising on
     truncation afterwards would destroy the datagram it complains about. */
  int wire = OT_udp_peek_len(sock);
  if (wire == OT_RECV_CLOSED) {
    mrb_raise(mrb, E_IO_ERROR, "closed socket");
  }
  if (wire == OT_RECV_WOULD_BLOCK) {
    return mrb_nil_value();
  }
  if (wire > maxlen) {
    mrb_raisef(mrb, E_RUNTIME_ERROR,
               "datagram of %d bytes needs a larger maxlen than %d",
               wire, (int)maxlen);
  }

  mrb_value data = mrb_str_new(mrb, NULL, maxlen);
  char host[OT_ADDR_STRLEN];
  int port = 0;
  int n = OT_udp_recvfrom(sock, RSTRING_PTR(data), (size_t)maxlen,
                          host, sizeof(host), &port, NULL);
  if (n < 0) {
    return mrb_nil_value();     /* raced with a close, or drained meanwhile */
  }
  mrb_str_resize(mrb, data, n);

  mrb_value addr = mrb_ary_new_capa(mrb, 4);
  mrb_ary_push(mrb, addr, mrb_str_new_lit(mrb, "AF_INET6"));
  mrb_ary_push(mrb, addr, mrb_fixnum_value(port));
  mrb_ary_push(mrb, addr, mrb_str_new_cstr(mrb, host));
  mrb_ary_push(mrb, addr, mrb_str_new_cstr(mrb, host));

  mrb_value ret = mrb_ary_new_capa(mrb, 2);
  mrb_ary_push(mrb, ret, data);
  mrb_ary_push(mrb, ret, addr);
  return ret;
}

static mrb_value
mrb_udp_dropped(mrb_state *mrb, mrb_value self)
{
  OT_udp_t *sock = (OT_udp_t *)DATA_PTR(self);
  return mrb_fixnum_value(sock == NULL ? 0 : (mrb_int)OT_udp_dropped(sock));
}

static mrb_value
mrb_udp_close(mrb_state *mrb, mrb_value self)
{
  OT_udp_t *sock = (OT_udp_t *)DATA_PTR(self);
  if (sock != NULL) {
    OT_udp_close(sock);
    mrb_data_init(self, NULL, &ot_udp_type);
  }
  return mrb_nil_value();
}

void
mrb_picoruby_openthread_gem_init(mrb_state *mrb)
{
  struct RClass *mod = mrb_define_module_id(mrb, MRB_SYM(OpenThread));

  mrb_define_module_function_id(mrb, mod, MRB_SYM(__init), mrb_ot_init, MRB_ARGS_NONE());
  mrb_define_module_function_id(mrb, mod, MRB_SYM_Q(started), mrb_ot_started_q, MRB_ARGS_NONE());
  mrb_define_module_function_id(mrb, mod, MRB_SYM(__set_dataset_tlvs), mrb_ot_set_dataset_tlvs, MRB_ARGS_REQ(1));
  mrb_define_module_function_id(mrb, mod, MRB_SYM(__set_dataset), mrb_ot_set_dataset, MRB_ARGS_REQ(3));
  mrb_define_module_function_id(mrb, mod, MRB_SYM(__ip6_enable), mrb_ot_ip6_enable, MRB_ARGS_OPT(1));
  mrb_define_module_function_id(mrb, mod, MRB_SYM(__thread_enable), mrb_ot_thread_enable, MRB_ARGS_OPT(1));
  mrb_define_module_function_id(mrb, mod, MRB_SYM(__role), mrb_ot_role, MRB_ARGS_NONE());
  mrb_define_module_function_id(mrb, mod, MRB_SYM(eid), mrb_ot_eid, MRB_ARGS_NONE());
  mrb_define_module_function_id(mrb, mod, MRB_SYM(addresses), mrb_ot_addresses, MRB_ARGS_NONE());
  mrb_define_module_function_id(mrb, mod, MRB_SYM_E(__role_queue), mrb_ot_set_role_queue, MRB_ARGS_REQ(1));
  mrb_define_module_function_id(mrb, mod, MRB_SYM(__role_event_popped), mrb_ot_role_event_popped, MRB_ARGS_NONE());

  struct RClass *udp = mrb_define_class_under_id(mrb, mod, MRB_SYM(UDP), mrb->object_class);
  MRB_SET_INSTANCE_TT(udp, MRB_TT_CDATA);
  mrb_define_method_id(mrb, udp, MRB_SYM(initialize), mrb_udp_initialize, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, udp, MRB_SYM(bind), mrb_udp_bind, MRB_ARGS_ARG(1, 1));
  mrb_define_method_id(mrb, udp, MRB_SYM(send), mrb_udp_send, MRB_ARGS_REQ(4));
  mrb_define_method_id(mrb, udp, MRB_SYM(recvfrom_nonblock), mrb_udp_recvfrom_nonblock, MRB_ARGS_OPT(1));
  mrb_define_method_id(mrb, udp, MRB_SYM(close), mrb_udp_close, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, udp, MRB_SYM(dropped), mrb_udp_dropped, MRB_ARGS_NONE());
}

void
mrb_picoruby_openthread_gem_final(mrb_state *mrb)
{
}
