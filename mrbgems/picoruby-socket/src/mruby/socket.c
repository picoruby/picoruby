#include "../../include/socket.h"
#include "mruby/presym.h"
#include "mruby/class.h"
#include "mruby/data.h"
#include "mruby/variable.h"
#ifdef PICO_CYW43_ARCH_POLL
#include "task.h"
#endif

/* Socket type constants (matching socket.h) */
#if defined(PICORB_PLATFORM_POSIX)
#include <sys/socket.h>
#else
#define SOCK_STREAM 1
#define SOCK_DGRAM  2
#endif

const struct mrb_data_type mrb_socket_type = {
  "Socket", mrb_socket_free,
};

#ifdef PICO_CYW43_ARCH_POLL
typedef struct {
  mrb_state *mrb;
  mrb_value queue;
  void *request;
} mrb_dns_resolver;

static void
mrb_dns_resolver_notify(void *arg)
{
  mrb_dns_resolver *resolver = (mrb_dns_resolver *)arg;
  if (!resolver) return;
  mrb_task_queue_push(resolver->mrb, resolver->queue, mrb_true_value());
}

static void
mrb_dns_resolver_free(mrb_state *mrb, void *ptr)
{
  mrb_dns_resolver *resolver = (mrb_dns_resolver *)ptr;
  if (!resolver) return;
  if (resolver->request) Net_dns_abandon(resolver->request);
  mrb_free(mrb, resolver);
}

static const struct mrb_data_type mrb_dns_resolver_type = {
  "Socket::DNSResolver", mrb_dns_resolver_free,
};

static mrb_value
mrb_dns_resolver_initialize(mrb_state *mrb, mrb_value self)
{
  const char *name;
  mrb_get_args(mrb, "z", &name);

  struct RClass *task_class = mrb_class_get_id(mrb, MRB_SYM(Task));
  struct RClass *queue_class = mrb_class_get_under_id(mrb, task_class, MRB_SYM(Queue));
  mrb_value queue = mrb_obj_new(mrb, queue_class, 0, NULL);

  mrb_dns_resolver *resolver = (mrb_dns_resolver *)mrb_malloc(mrb, sizeof(*resolver));
  resolver->mrb = mrb;
  resolver->queue = queue;
  resolver->request = Net_dns_start(name, mrb_dns_resolver_notify, resolver);
  if (!resolver->request) {
    mrb_free(mrb, resolver);
    mrb_raise(mrb, E_RUNTIME_ERROR, Net_get_last_error());
  }

  mrb_data_init(self, resolver, &mrb_dns_resolver_type);
  mrb_iv_set(mrb, self, MRB_IVSYM(event_queue), queue);
  return self;
}

static mrb_value
mrb_dns_resolver_status(mrb_state *mrb, mrb_value self)
{
  mrb_dns_resolver *resolver = (mrb_dns_resolver *)
    mrb_data_get_ptr(mrb, self, &mrb_dns_resolver_type);
  return mrb_fixnum_value(Net_dns_status(resolver->request));
}

static mrb_value
mrb_dns_resolver_address(mrb_state *mrb, mrb_value self)
{
  mrb_dns_resolver *resolver = (mrb_dns_resolver *)
    mrb_data_get_ptr(mrb, self, &mrb_dns_resolver_type);
  char address[40];

  if (Net_dns_get_address(resolver->request, address, sizeof(address)) != 0) {
    return mrb_nil_value();
  }
  return mrb_str_new_cstr(mrb, address);
}

static mrb_value
mrb_dns_resolver_release(mrb_state *mrb, mrb_value self)
{
  mrb_dns_resolver *resolver = (mrb_dns_resolver *)
    mrb_data_get_ptr(mrb, self, &mrb_dns_resolver_type);
  if (resolver->request) {
    Net_dns_release(resolver->request);
    resolver->request = NULL;
  }
  return mrb_nil_value();
}

static mrb_value
mrb_dns_resolver_abandon(mrb_state *mrb, mrb_value self)
{
  mrb_dns_resolver *resolver = (mrb_dns_resolver *)
    mrb_data_get_ptr(mrb, self, &mrb_dns_resolver_type);
  if (resolver->request) {
    Net_dns_abandon(resolver->request);
    resolver->request = NULL;
  }
  return mrb_nil_value();
}

static void
mrb_init_dns_resolver(mrb_state *mrb)
{
  struct RClass *resolver = mrb_define_class_id(
    mrb, MRB_SYM(SocketDNSResolver), mrb->object_class);
  MRB_SET_INSTANCE_TT(resolver, MRB_TT_DATA);
  mrb_define_method_id(mrb, resolver, MRB_SYM(initialize),
                       mrb_dns_resolver_initialize, MRB_ARGS_REQ(1));
  mrb_define_private_method_id(mrb, resolver, MRB_SYM(__status),
                               mrb_dns_resolver_status, MRB_ARGS_NONE());
  mrb_define_private_method_id(mrb, resolver, MRB_SYM(__address),
                               mrb_dns_resolver_address, MRB_ARGS_NONE());
  mrb_define_private_method_id(mrb, resolver, MRB_SYM(__release),
                               mrb_dns_resolver_release, MRB_ARGS_NONE());
  mrb_define_private_method_id(mrb, resolver, MRB_SYM(__abandon),
                               mrb_dns_resolver_abandon, MRB_ARGS_NONE());
}
#endif

/* Data type for sockets (shared by TCP and UDP) */
void
mrb_socket_free(mrb_state *mrb, void *ptr)
{
  if (ptr) {
    picorb_socket_t *sock = (picorb_socket_t *)ptr;
    if (!sock->closed) {
      /* Close socket based on socket type */
      if (sock->socktype == SOCK_DGRAM) {
        UDPSocket_close(mrb, sock);
      } else {
        TCPSocket_close(mrb, sock);
      }
    }
    mrb_free(mrb, sock);
  }
}

/* Initialize gem */
void
mrb_picoruby_socket_gem_init(mrb_state *mrb)
{
  struct RClass *basic_socket_class;

  /* BasicSocket class */
  basic_socket_class = mrb_define_class_id(mrb, MRB_SYM(BasicSocket), mrb->object_class);

#ifdef PICO_CYW43_ARCH_POLL
  mrb_init_dns_resolver(mrb);
#endif

  /* Initialize each socket type */
  tcp_socket_init(mrb, basic_socket_class);
  udp_socket_init(mrb, basic_socket_class);
  tcp_server_init(mrb, basic_socket_class);
  ssl_socket_init(mrb, basic_socket_class);
}

void
mrb_picoruby_socket_gem_final(mrb_state *mrb)
{
  /* Nothing to do */
}
