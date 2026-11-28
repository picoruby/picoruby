#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "picoruby.h"
#ifdef PICO_CYW43_ARCH_POLL
#include "c_task_queue.h"

mrbc_value
picorb_task_queue_new(mrbc_vm *vm)
{
  mrbc_value queue = mrbc_instance_new(vm, MRBC_CLASS(Task_Queue), 0);

  mrbc_value items = mrbc_array_new(vm, 0);
  mrbc_instance_setiv(&queue, mrbc_str_to_symid("@items"), &items);
  mrbc_decref(&items);

  mrbc_value closed = mrbc_false_value();
  mrbc_instance_setiv(&queue, mrbc_str_to_symid("@closed"), &closed);

  return queue;
}

void
picorb_task_queue_notify(mrbc_vm *vm, void *queue_ptr, bool *pending)
{
  (void)vm;
  if (!queue_ptr || !pending || *pending) return;
  mrbc_value event = mrbc_true_value();
  if (mrbc_task_queue_push((mrbc_value *)queue_ptr, &event) ==
      MRBC_TASK_QUEUE_PUSH_OK) {
    *pending = true;
  }
}

bool
picorb_task_queue_attach(mrbc_vm *vm, void *self_ptr, void **queue_ptr)
{
  mrbc_value *self = (mrbc_value *)self_ptr;
  mrbc_value queue = picorb_task_queue_new(vm);
  mrbc_instance_setiv(self, mrbc_str_to_symid("event_queue"), &queue);
  *queue_ptr = picorb_alloc(vm, sizeof(mrbc_value));
  if (!*queue_ptr) {
    mrbc_decref(&queue);
    return false;
  }
  *(mrbc_value *)*queue_ptr = queue;
  mrbc_decref(&queue);
  return true;
}

bool
picorb_socket_attach_event_queue(mrbc_vm *vm, void *self_ptr,
                                  picorb_socket_t *sock)
{
  if (!picorb_task_queue_attach(vm, self_ptr, &sock->event_queue)) return false;
  sock->vm = vm;
  return true;
}

void
picorb_socket_notify_readable(picorb_socket_t *sock)
{
  if (!sock) return;
  picorb_task_queue_notify((mrbc_vm *)sock->vm, sock->event_queue,
                           &sock->event_pending);
}

typedef struct {
  mrbc_value queue;
  void *request;
} mrbc_dns_resolver;

static void
mrbc_dns_resolver_notify(void *arg)
{
  mrbc_dns_resolver *resolver = (mrbc_dns_resolver *)arg;
  if (!resolver) return;
  mrbc_value event = mrbc_true_value();
  mrbc_task_queue_push(&resolver->queue, &event);
}

static void
mrbc_dns_resolver_free(mrbc_value *self)
{
  mrbc_dns_resolver *resolver = (mrbc_dns_resolver *)self->instance->data;
  if (!resolver) return;
  if (resolver->request) Net_dns_abandon(resolver->request);
}

static void
c_dns_resolver_new(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (argc != 1 || GET_ARG(1).tt != MRBC_TT_STRING) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "host must be a String");
    return;
  }

  const char *host = (const char *)GET_ARG(1).string->data;
  mrbc_value self = mrbc_instance_new(vm, v->cls, sizeof(mrbc_dns_resolver));
  mrbc_dns_resolver *resolver = (mrbc_dns_resolver *)self.instance->data;
  resolver->request = NULL;
  resolver->queue = picorb_task_queue_new(vm);
  mrbc_instance_setiv(&self, mrbc_str_to_symid("event_queue"), &resolver->queue);
  mrbc_decref(&resolver->queue);
  resolver->request = Net_dns_start(host, mrbc_dns_resolver_notify, resolver);
  if (!resolver->request) {
    mrbc_decref(&self);
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), Net_get_last_error());
    return;
  }
  SET_RETURN(self);
}

static mrbc_dns_resolver *
get_dns_resolver(mrbc_value *v)
{
  return (mrbc_dns_resolver *)v[0].instance->data;
}

static void
c_dns_resolver_status(mrbc_vm *vm, mrbc_value *v, int argc)
{
  mrbc_dns_resolver *resolver = get_dns_resolver(v);
  SET_INT_RETURN(Net_dns_status(resolver->request));
}

static void
c_dns_resolver_address(mrbc_vm *vm, mrbc_value *v, int argc)
{
  mrbc_dns_resolver *resolver = get_dns_resolver(v);
  char address[40];

  if (Net_dns_get_address(resolver->request, address, sizeof(address)) != 0) {
    SET_NIL_RETURN();
    return;
  }
  SET_RETURN(mrbc_string_new_cstr(vm, address));
}

static void
c_dns_resolver_release(mrbc_vm *vm, mrbc_value *v, int argc)
{
  mrbc_dns_resolver *resolver = get_dns_resolver(v);
  if (resolver->request) {
    Net_dns_release(resolver->request);
    resolver->request = NULL;
  }
  SET_NIL_RETURN();
}

static void
c_dns_resolver_abandon(mrbc_vm *vm, mrbc_value *v, int argc)
{
  mrbc_dns_resolver *resolver = get_dns_resolver(v);
  if (resolver->request) {
    Net_dns_abandon(resolver->request);
    resolver->request = NULL;
  }
  SET_NIL_RETURN();
}

static void
mrbc_dns_resolver_init(mrbc_vm *vm)
{
  mrbc_class *resolver = mrbc_define_class(vm, "SocketDNSResolver", mrbc_class_object);
  mrbc_define_destructor(resolver, mrbc_dns_resolver_free);
  mrbc_define_method(vm, resolver, "new", c_dns_resolver_new);
  mrbc_define_method(vm, resolver, "__status", c_dns_resolver_status);
  mrbc_define_method(vm, resolver, "__address", c_dns_resolver_address);
  mrbc_define_method(vm, resolver, "__release", c_dns_resolver_release);
  mrbc_define_method(vm, resolver, "__abandon", c_dns_resolver_abandon);
}
#endif

/* Socket type constants (matching socket.h) */
#if defined(PICORB_PLATFORM_POSIX)
#include <sys/socket.h>
#include <netinet/in.h>
#else
#define SOCK_STREAM 1
#define SOCK_DGRAM  2
#endif

/* Socket::Constants: the platform's values on POSIX, lwIP's elsewhere
 * (lwIP's sockets.h is compiled out when LWIP_SOCKET is 0) */
#ifndef AF_UNSPEC
#define AF_UNSPEC   0
#endif
#ifndef AF_INET
#define AF_INET     2
#endif
#ifndef AF_INET6
#define AF_INET6    10
#endif
#ifndef PF_UNSPEC
#define PF_UNSPEC   AF_UNSPEC
#endif
#ifndef PF_INET
#define PF_INET     AF_INET
#endif
#ifndef PF_INET6
#define PF_INET6    AF_INET6
#endif
#ifndef SOCK_RAW
#define SOCK_RAW    3
#endif
#ifndef IPPROTO_IP
#define IPPROTO_IP  0
#endif
#ifndef IPPROTO_TCP
#define IPPROTO_TCP 6
#endif
#ifndef IPPROTO_UDP
#define IPPROTO_UDP 17
#endif

/* Socket < BasicSocket, with the constants in Socket::Constants and on
 * Socket itself (mruby/c has no include to reach them through) */
static void
mrbc_socket_constants_init(mrbc_vm *vm, mrbc_class *class_BasicSocket)
{
  mrbc_class *class_Socket = mrbc_define_class(vm, "Socket", class_BasicSocket);
  mrbc_class *class_Constants = mrbc_define_module_under(vm, class_Socket, "Constants");
#define SOCKET_CONST(name) do { \
  mrbc_value v = mrbc_integer_value(name); \
  mrbc_set_class_const(class_Socket, mrbc_str_to_symid(#name), &v); \
  mrbc_set_class_const(class_Constants, mrbc_str_to_symid(#name), &v); \
} while (0)
  SOCKET_CONST(AF_UNSPEC);
  SOCKET_CONST(AF_INET);
  SOCKET_CONST(AF_INET6);
  SOCKET_CONST(PF_UNSPEC);
  SOCKET_CONST(PF_INET);
  SOCKET_CONST(PF_INET6);
  SOCKET_CONST(SOCK_STREAM);
  SOCKET_CONST(SOCK_DGRAM);
  SOCKET_CONST(SOCK_RAW);
  SOCKET_CONST(IPPROTO_IP);
  SOCKET_CONST(IPPROTO_TCP);
  SOCKET_CONST(IPPROTO_UDP);
#undef SOCKET_CONST
}

void
mrbc_socket_free(mrbc_value *self)
{
  //void *data = self->instance->data;
  socket_wrapper_t *wrapper = (socket_wrapper_t *)self->instance->data;
  if (!wrapper) {
    return;
  }

  /* Get socket pointer (always stored as pointer now) */
  picorb_socket_t *sock = wrapper->ptr;

  if (!sock) {
    return;
  }

  if (!sock->closed) {
    /* Close socket based on socket type */
    if (sock->socktype == SOCK_DGRAM) {
      UDPSocket_close(wrapper->vm, sock);
    } else {
      TCPSocket_close(wrapper->vm, sock);
    }
  }

  /* Free the allocated socket structure */
  picorb_free(wrapper->vm, sock);
}

void
mrbc_socket_init(mrbc_vm *vm)
{
  mrbc_class *class_BasicSocket = mrbc_define_class(vm, "BasicSocket", mrbc_class_object);
  mrbc_socket_constants_init(vm, class_BasicSocket);

#ifdef PICO_CYW43_ARCH_POLL
  mrbc_dns_resolver_init(vm);
#endif

  tcp_socket_init(vm, class_BasicSocket);
  udp_socket_init(vm, class_BasicSocket);
  ssl_socket_init(vm, class_BasicSocket);
  tcp_server_init(vm, class_BasicSocket);
}
