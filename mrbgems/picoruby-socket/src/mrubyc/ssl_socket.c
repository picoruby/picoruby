#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "picoruby.h"

typedef struct {
  picorb_ssl_socket_t *ptr;
  picorb_state *vm;
} ssl_socket_wrapper_t;

static void
mrbc_ssl_socket_free(mrbc_value *self)
{
  ssl_socket_wrapper_t *wrapper = (ssl_socket_wrapper_t *)self->instance->data;
  if (wrapper->ptr && !SSLSocket_closed(wrapper->vm, wrapper->ptr)) {
    SSLSocket_close(wrapper->vm, wrapper->ptr);
    wrapper->ptr = NULL;
  }
}

typedef struct {
  picorb_ssl_context_t *ptr;
  picorb_state *vm;
} ssl_context_wrapper_t;

static void
mrbc_ssl_context_free(mrbc_value *self)
{
  ssl_context_wrapper_t *wrapper = (ssl_context_wrapper_t *)self->instance->data;
  if (wrapper->ptr) {
    SSLContext_free(wrapper->vm, wrapper->ptr);
    wrapper->ptr = NULL;
  }
}


/*
 * SSLSocket.new(tcp_socket, ssl_context) - wraps existing TCPSocket with SSL
 */
static void
c_ssl_socket_new(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (argc != 2) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments (expected 2)");
    return;
  }

  mrbc_value tcp_socket_obj = GET_ARG(1);
  mrbc_value ssl_context_obj = GET_ARG(2);

  if (tcp_socket_obj.tt != MRBC_TT_OBJECT) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "first argument must be a TCPSocket");
    return;
  }

  void *data = tcp_socket_obj.instance->data;
  picorb_socket_t **sock_ptr = (picorb_socket_t **)data;
  picorb_socket_t *tcp_sock = *sock_ptr;

  if (!tcp_sock) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "TCPSocket is not initialized");
    return;
  }

  const char *hostname = TCPSocket_remote_host(vm, tcp_sock);
  int port = TCPSocket_remote_port(vm, tcp_sock);

  if (!hostname) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "TCPSocket must have remote_host");
    return;
  }
  if (port < 0) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "TCPSocket must have remote_port");
    return;
  }

  ssl_context_wrapper_t *ctx_wrapper = (ssl_context_wrapper_t *)ssl_context_obj.instance->data;
  if (!ctx_wrapper->ptr) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "SSLContext argument is required");
    return;
  }

  /* Close the original TCP socket to avoid resource leak */
  if (!TCPSocket_closed(vm, tcp_sock)) {
    TCPSocket_close(vm, tcp_sock);
  }

  /* Create instance with wrapper structure */
  mrbc_value instance = mrbc_instance_new(vm, v->cls, sizeof(ssl_socket_wrapper_t));
  ssl_socket_wrapper_t *wrapper = (ssl_socket_wrapper_t *)instance.instance->data;
  wrapper->vm = vm;

  /* Create SSL socket */
  wrapper->ptr = SSLSocket_create(vm, ctx_wrapper->ptr);
  if (!wrapper->ptr) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "failed to create SSL socket");
    return;
  }

  /* Set hostname and port */
  if (!SSLSocket_set_hostname(vm, wrapper->ptr, hostname)) {
    SSLSocket_close(vm, wrapper->ptr);
    wrapper->ptr = NULL;
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "failed to set hostname");
    return;
  }

  if (!SSLSocket_set_port(vm, wrapper->ptr, port)) {
    SSLSocket_close(vm, wrapper->ptr);
    wrapper->ptr = NULL;
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "failed to set port");
    return;
  }

  /* Store references for GC protection */
  mrbc_instance_setiv(&instance, mrbc_str_to_symid("@tcp_socket"), &tcp_socket_obj);
  mrbc_instance_setiv(&instance, mrbc_str_to_symid("@ssl_context"), &ssl_context_obj);

  SET_RETURN(instance);
}

/*
 * SSLSocket.open(hostname, port, ssl_context) - connect directly without TCPSocket
 */
static void
c_ssl_socket_open(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (argc != 3) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments (expected 3)");
    return;
  }

  mrbc_value hostname_arg = GET_ARG(1);
  mrbc_value port_arg = GET_ARG(2);
  mrbc_value ssl_context_obj = GET_ARG(3);

  if (hostname_arg.tt != MRBC_TT_STRING) {
    mrbc_raise(vm, MRBC_CLASS(TypeError), "hostname must be a String");
    return;
  }
  if (port_arg.tt != MRBC_TT_INTEGER) {
    mrbc_raise(vm, MRBC_CLASS(TypeError), "port must be an Integer");
    return;
  }

  const char *hostname = (const char *)hostname_arg.string->data;
  int port = (int)port_arg.i;

  ssl_context_wrapper_t *ctx_wrapper = (ssl_context_wrapper_t *)ssl_context_obj.instance->data;
  if (!ctx_wrapper->ptr) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "SSLContext argument is required");
    return;
  }

  /* Create instance with wrapper structure */
  mrbc_value instance = mrbc_instance_new(vm, v->cls, sizeof(ssl_socket_wrapper_t));
  ssl_socket_wrapper_t *wrapper = (ssl_socket_wrapper_t *)instance.instance->data;

  /* Create SSL socket */
  wrapper->ptr = SSLSocket_create(vm, ctx_wrapper->ptr);
  if (!wrapper->ptr) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "failed to create SSL socket");
    return;
  }

  /* Set hostname and port */
  if (!SSLSocket_set_hostname(vm, wrapper->ptr, hostname)) {
    SSLSocket_close(vm, wrapper->ptr);
    wrapper->ptr = NULL;
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "failed to set hostname");
    return;
  }

  if (!SSLSocket_set_port(vm, wrapper->ptr, port)) {
    SSLSocket_close(vm, wrapper->ptr);
    wrapper->ptr = NULL;
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "failed to set port");
    return;
  }

  /* Perform SSL handshake */
  if (!SSLSocket_connect(vm, wrapper->ptr)) {
    SSLSocket_close(vm, wrapper->ptr);
    wrapper->ptr = NULL;
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "SSL connection failed");
    return;
  }

  /* Store reference for GC protection */
  mrbc_instance_setiv(&instance, mrbc_str_to_symid("@ssl_context"), &ssl_context_obj);

  SET_RETURN(instance);
}

/*
 * ssl_socket.connect -> self
 */
static void
c_ssl_socket_connect(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (argc != 0) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments");
    return;
  }

  /* Get SSL socket pointer from wrapper */
  ssl_socket_wrapper_t *wrapper = (ssl_socket_wrapper_t *)v[0].instance->data;
  if (!wrapper->ptr) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "SSL socket is not initialized");
    return;
  }

  /* Perform SSL handshake */
  if (!SSLSocket_connect(vm, wrapper->ptr)) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "SSL handshake failed");
    return;
  }
}

/*
 * ssl_socket.send(data, flags) -> Integer
 */
static void
c_ssl_socket_send(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (argc != 2) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments");
    return;
  }

  /* Get SSL socket pointer from wrapper */
  ssl_socket_wrapper_t *wrapper = (ssl_socket_wrapper_t *)v[0].instance->data;
  if (!wrapper->ptr) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "SSL socket is not initialized");
    return;
  }

  /* Check argument types */
  mrbc_value data = GET_ARG(1);
  if (data.tt != MRBC_TT_STRING) {
    mrbc_raise(vm, MRBC_CLASS(TypeError), "data must be a String");
    return;
  }
  mrbc_value flags = GET_ARG(2);
  if (flags.tt != MRBC_TT_INTEGER) {
    mrbc_raise(vm, MRBC_CLASS(TypeError), "flags must be an Integer");
    return;
  }

  /* Send data */
  ssize_t sent = SSLSocket_send(vm, wrapper->ptr, (const void *)data.string->data, data.string->size);
  if (sent < 0) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "SSL send failed");
    return;
  }

  SET_INT_RETURN(sent);
}

/*
 * ssl_socket.readpartial(maxlen) -> String
 * Raises EOFError on EOF.
 */
static void
c_ssl_socket_readpartial(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (argc != 1) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments");
    return;
  }

  ssl_socket_wrapper_t *wrapper = (ssl_socket_wrapper_t *)v[0].instance->data;
  if (!wrapper->ptr) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "SSL socket is not initialized");
    return;
  }

  mrbc_value maxlen_arg = GET_ARG(1);
  if (maxlen_arg.tt != MRBC_TT_INTEGER) {
    mrbc_raise(vm, MRBC_CLASS(TypeError), "maxlen must be an Integer");
    return;
  }
  int maxlen = (int)maxlen_arg.i;
  if (maxlen <= 0) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "maxlen must be positive");
    return;
  }

  char stack_buf[PICORB_SOCKET_STACK_BUF_SIZE];
  char *buffer = (maxlen < PICORB_SOCKET_STACK_BUF_SIZE)
    ? stack_buf
    : (char *)picorb_alloc(vm, maxlen);
  if (!buffer) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "failed to allocate buffer");
    return;
  }

  ssize_t received = SSLSocket_recv(vm, wrapper->ptr, buffer, maxlen, false);

  if (received == 0) {
    if (buffer != stack_buf) picorb_free(vm, buffer);
    mrbc_raise(vm, mrbc_get_class_by_name("EOFError"), "end of file reached");
    return;
  }

  if (received == PICORB_RECV_TIMEOUT) {
    if (buffer != stack_buf) picorb_free(vm, buffer);
    mrbc_raise(vm, mrbc_get_class_by_name("SocketError"), "read timeout");
    return;
  }

  if (received < 0) {
    if (buffer != stack_buf) picorb_free(vm, buffer);
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "SSL read failed");
    return;
  }

  mrbc_value ret = mrbc_string_new(vm, buffer, received);
  if (buffer != stack_buf) picorb_free(vm, buffer);
  SET_RETURN(ret);
}

/*
 * ssl_socket.read_nonblock(maxlen) -> String or nil
 */
static void
c_ssl_socket_read_nonblock(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (argc != 1) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments");
    return;
  }

  ssl_socket_wrapper_t *wrapper = (ssl_socket_wrapper_t *)v[0].instance->data;
  if (!wrapper->ptr) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "SSL socket is not initialized");
    return;
  }

  mrbc_value maxlen_arg = GET_ARG(1);
  if (maxlen_arg.tt != MRBC_TT_INTEGER) {
    mrbc_raise(vm, MRBC_CLASS(TypeError), "maxlen must be an Integer");
    return;
  }

  int maxlen = (int)maxlen_arg.i;
  if (maxlen <= 0) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "maxlen must be positive");
    return;
  }

  char stack_buf[PICORB_SOCKET_STACK_BUF_SIZE];
  char *buffer = (maxlen < PICORB_SOCKET_STACK_BUF_SIZE)
    ? stack_buf
    : (char *)picorb_alloc(vm, maxlen);
  if (!buffer) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "failed to allocate buffer");
    return;
  }

  ssize_t received = SSLSocket_recv(vm, wrapper->ptr, buffer, maxlen, true);

  if (received == PICORB_RECV_WOULD_BLOCK) {
    if (buffer != stack_buf) picorb_free(vm, buffer);
    SET_NIL_RETURN();
    return;
  }

  if (received == 0) {
    if (buffer != stack_buf) picorb_free(vm, buffer);
    mrbc_raise(vm, mrbc_get_class_by_name("EOFError"), "end of file reached");
    return;
  }

  if (received < 0) {
    if (buffer != stack_buf) picorb_free(vm, buffer);
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "SSL read failed");
    return;
  }

  mrbc_value ret = mrbc_string_new(vm, buffer, received);
  if (buffer != stack_buf) picorb_free(vm, buffer);
  SET_RETURN(ret);
}

/*
 * ssl_socket.close -> nil
 */
static void
c_ssl_socket_close(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (argc != 0) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments");
    return;
  }

  /* Get SSL socket pointer from wrapper */
  ssl_socket_wrapper_t *wrapper = (ssl_socket_wrapper_t *)v[0].instance->data;
  if (!wrapper->ptr) {
    /* Already closed or not initialized */
    SET_NIL_RETURN();
    return;
  }

  /* Close SSL socket (also frees the SSL socket structure) */
  SSLSocket_close(vm, wrapper->ptr);

  /* Clear the pointer */
  wrapper->ptr = NULL;

  SET_NIL_RETURN();
}

/*
 * ssl_socket.closed? -> true or false
 */
static void
c_ssl_socket_closed_q(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (argc != 0) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments");
    return;
  }

  /* Get SSL socket pointer from wrapper */
  ssl_socket_wrapper_t *wrapper = (ssl_socket_wrapper_t *)v[0].instance->data;
  if (!wrapper->ptr) {
    SET_TRUE_RETURN();
    return;
  }

  /* Check if SSL socket is closed */
  bool is_closed = SSLSocket_closed(vm, wrapper->ptr);
  if (is_closed) {
    SET_TRUE_RETURN();
  } else {
    SET_FALSE_RETURN();
  }
}

/*
 * ssl_socket.remote_host -> String
 */
static void
c_ssl_socket_remote_host(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (argc != 0) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments");
    return;
  }

  /* Get SSL socket pointer from wrapper */
  ssl_socket_wrapper_t *wrapper = (ssl_socket_wrapper_t *)v[0].instance->data;
  if (!wrapper->ptr) {
    SET_NIL_RETURN();
    return;
  }

  /* Get remote host */
  const char *host = SSLSocket_remote_host(vm, wrapper->ptr);
  if (!host || host[0] == '\0') {
    SET_NIL_RETURN();
    return;
  }

  SET_RETURN(mrbc_string_new_cstr(vm, host));
}

/*
 * ssl_socket.remote_port -> Integer
 */
static void
c_ssl_socket_remote_port(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (argc != 0) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments");
    return;
  }

  /* Get SSL socket pointer from wrapper */
  ssl_socket_wrapper_t *wrapper = (ssl_socket_wrapper_t *)v[0].instance->data;
  if (!wrapper->ptr) {
    SET_NIL_RETURN();
    return;
  }

  /* Get remote port */
  int port = SSLSocket_remote_port(vm, wrapper->ptr);
  if (port < 0) {
    SET_NIL_RETURN();
    return;
  }

  SET_INT_RETURN(port);
}

/*
 * ssl_socket.ready? -> true or false
 */
static void
c_ssl_socket_ready_q(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (argc != 0) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments");
    return;
  }

  /* Get SSL socket pointer from wrapper */
  ssl_socket_wrapper_t *wrapper = (ssl_socket_wrapper_t *)v[0].instance->data;
  if (!wrapper->ptr) {
    SET_FALSE_RETURN();
    return;
  }

  /* Check if data is ready to read */
  bool is_ready = SSLSocket_ready(vm, wrapper->ptr);
  if (is_ready) {
    SET_TRUE_RETURN();
  } else {
    SET_FALSE_RETURN();
  }
}

/*
 * SSLContext.new() -> SSLContext
 */
static void
c_ssl_context_new(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (argc != 0) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments");
    return;
  }

  /* Create instance with wrapper structure */
  mrbc_value instance = mrbc_instance_new(vm, v->cls, sizeof(ssl_context_wrapper_t));
  ssl_context_wrapper_t *wrapper = (ssl_context_wrapper_t *)instance.instance->data;
  wrapper->vm = vm;

  /* Create SSL context and store pointer */
  wrapper->ptr = SSLContext_create(vm);
  if (!wrapper->ptr) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "failed to create SSL context");
    return;
  }

  SET_RETURN(instance);
}

/*
 * ssl_context.ca_file = path -> String
 */
static void
c_ssl_context_set_ca_file(mrbc_vm *vm, mrbc_value *v, int argc)
{
#ifdef PICORB_PLATFORM_POSIX
  if (argc != 1) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments");
    return;
  }

  /* Get SSL context pointer from wrapper */
  ssl_context_wrapper_t *wrapper = (ssl_context_wrapper_t *)v[0].instance->data;
  if (!wrapper->ptr) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "SSL context is not initialized");
    return;
  }

  /* Get ca_file argument */
  mrbc_value ca_file_arg = GET_ARG(1);
  if (ca_file_arg.tt != MRBC_TT_STRING) {
    mrbc_raise(vm, MRBC_CLASS(TypeError), "ca_file must be a String");
    return;
  }

  const char *ca_file = (const char *)ca_file_arg.string->data;

  /* Set CA file */
  if (!SSLContext_set_ca_file(vm, wrapper->ptr, ca_file)) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "failed to set CA file");
    return;
  }

  mrbc_incref(&ca_file_arg);
  SET_RETURN(ca_file_arg);
#else
  (void)argc;
  (void)v;
  mrbc_raise(vm, MRBC_CLASS(NotImplementedError), "ca_file= is not supported on this platform. Use set_ca instead");
#endif
}

/* ssl_context.set_ca(addr, size) */
static void
c_ssl_context_set_ca(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (argc != 2) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments");
    return;
  }

  /* Get SSL context pointer from wrapper */
  ssl_context_wrapper_t *wrapper = (ssl_context_wrapper_t *)v[0].instance->data;
  if (!wrapper->ptr) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "SSL context is not initialized");
    return;
  }

  /* Get addr and size arguments */
  mrbc_value addr_arg = GET_ARG(1);
  mrbc_value size_arg = GET_ARG(2);

  if (addr_arg.tt != MRBC_TT_INTEGER) {
    mrbc_raise(vm, MRBC_CLASS(TypeError), "addr must be an Integer");
    return;
  }
  if (size_arg.tt != MRBC_TT_INTEGER) {
    mrbc_raise(vm, MRBC_CLASS(TypeError), "size must be an Integer");
    return;
  }

  const void *addr = (const void *)(intptr_t)addr_arg.i;
  size_t size = (size_t)size_arg.i;

  /* Set CA certificate */
  if (!SSLContext_set_ca(vm, wrapper->ptr, addr, size)) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "failed to set CA certificate");
    return;
  }

  SET_NIL_RETURN();
}

/*
 * ssl_context.cert_file = path -> String
 */
static void
c_ssl_context_set_cert_file(mrbc_vm *vm, mrbc_value *v, int argc)
{
#ifdef PICORB_PLATFORM_POSIX
  if (argc != 1) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments");
    return;
  }

  /* Get SSL context pointer from wrapper */
  ssl_context_wrapper_t *wrapper = (ssl_context_wrapper_t *)v[0].instance->data;
  if (!wrapper->ptr) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "SSL context is not initialized");
    return;
  }

  /* Get cert_file argument */
  mrbc_value cert_file_arg = GET_ARG(1);
  if (cert_file_arg.tt != MRBC_TT_STRING) {
    mrbc_raise(vm, MRBC_CLASS(TypeError), "cert_file must be a String");
    return;
  }

  const char *cert_file = (const char *)cert_file_arg.string->data;

  /* Set certificate file */
  if (!SSLContext_set_cert_file(vm, wrapper->ptr, cert_file)) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "failed to set certificate file");
    return;
  }

  mrbc_incref(&cert_file_arg);
  SET_RETURN(cert_file_arg);
#else
  (void)argc;
  (void)v;
  mrbc_raise(vm, MRBC_CLASS(NotImplementedError), "cert_file= is not supported on this platform. Use set_cert instead");
#endif
}

/* ssl_context.set_cert(addr, size) */
static void
c_ssl_context_set_cert(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (argc != 2) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments");
    return;
  }

  /* Get SSL context pointer from wrapper */
  ssl_context_wrapper_t *wrapper = (ssl_context_wrapper_t *)v[0].instance->data;
  if (!wrapper->ptr) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "SSL context is not initialized");
    return;
  }

  /* Get addr and size arguments */
  mrbc_value addr_arg = GET_ARG(1);
  mrbc_value size_arg = GET_ARG(2);

  if (addr_arg.tt != MRBC_TT_INTEGER) {
    mrbc_raise(vm, MRBC_CLASS(TypeError), "addr must be an Integer");
    return;
  }
  if (size_arg.tt != MRBC_TT_INTEGER) {
    mrbc_raise(vm, MRBC_CLASS(TypeError), "size must be an Integer");
    return;
  }

  const void *addr = (const void *)(intptr_t)addr_arg.i;
  size_t size = (size_t)size_arg.i;

  /* Set certificate */
  if (!SSLContext_set_cert(vm, wrapper->ptr, addr, size)) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "failed to set certificate");
    return;
  }

  SET_NIL_RETURN();
}

/*
 * ssl_context.key_file = path -> String
 */
static void
c_ssl_context_set_key_file(mrbc_vm *vm, mrbc_value *v, int argc)
{
#ifdef PICORB_PLATFORM_POSIX
  if (argc != 1) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments");
    return;
  }

  /* Get SSL context pointer from wrapper */
  ssl_context_wrapper_t *wrapper = (ssl_context_wrapper_t *)v[0].instance->data;
  if (!wrapper->ptr) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "SSL context is not initialized");
    return;
  }

  /* Get key_file argument */
  mrbc_value key_file_arg = GET_ARG(1);
  if (key_file_arg.tt != MRBC_TT_STRING) {
    mrbc_raise(vm, MRBC_CLASS(TypeError), "key_file must be a String");
    return;
  }

  const char *key_file = (const char *)key_file_arg.string->data;
  /* Set key file */
  if (!SSLContext_set_key_file(vm, wrapper->ptr, key_file)) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "failed to set key file");
    return;
  }

  mrbc_incref(&key_file_arg);
  SET_RETURN(key_file_arg);
#else
  (void)argc;
  (void)v;
  mrbc_raise(vm, MRBC_CLASS(NotImplementedError), "key_file= is not supported on this platform. Use set_key instead");
#endif
}

/* ssl_context.set_ca_pem(str) -- store pointer to String buffer; keeps String alive via ivar */
static void
c_ssl_context_set_ca_pem(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (argc != 1) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments");
    return;
  }

  ssl_context_wrapper_t *wrapper = (ssl_context_wrapper_t *)v[0].instance->data;
  if (!wrapper->ptr) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "SSL context is not initialized");
    return;
  }

  mrbc_value str = GET_ARG(1);
  if (str.tt != MRBC_TT_STRING) {
    mrbc_raise(vm, MRBC_CLASS(TypeError), "argument must be a String");
    return;
  }

  mrbc_instance_setiv(&v[0], mrbc_str_to_symid("@ca_pem"), &str);

  if (!SSLContext_set_ca(vm, wrapper->ptr, (const void *)str.string->data, (size_t)str.string->size)) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "failed to set CA certificate");
    return;
  }

  SET_NIL_RETURN();
}

/* ssl_context.set_cert_pem(str) */
static void
c_ssl_context_set_cert_pem(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (argc != 1) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments");
    return;
  }

  ssl_context_wrapper_t *wrapper = (ssl_context_wrapper_t *)v[0].instance->data;
  if (!wrapper->ptr) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "SSL context is not initialized");
    return;
  }

  mrbc_value str = GET_ARG(1);
  if (str.tt != MRBC_TT_STRING) {
    mrbc_raise(vm, MRBC_CLASS(TypeError), "argument must be a String");
    return;
  }

  mrbc_instance_setiv(&v[0], mrbc_str_to_symid("@cert_pem"), &str);

  if (!SSLContext_set_cert(vm, wrapper->ptr, (const void *)str.string->data, (size_t)str.string->size)) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "failed to set certificate");
    return;
  }

  SET_NIL_RETURN();
}

/* ssl_context.set_key_pem(str) */
static void
c_ssl_context_set_key_pem(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (argc != 1) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments");
    return;
  }

  ssl_context_wrapper_t *wrapper = (ssl_context_wrapper_t *)v[0].instance->data;
  if (!wrapper->ptr) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "SSL context is not initialized");
    return;
  }

  mrbc_value str = GET_ARG(1);
  if (str.tt != MRBC_TT_STRING) {
    mrbc_raise(vm, MRBC_CLASS(TypeError), "argument must be a String");
    return;
  }

  mrbc_instance_setiv(&v[0], mrbc_str_to_symid("@key_pem"), &str);

  if (!SSLContext_set_key(vm, wrapper->ptr, (const void *)str.string->data, (size_t)str.string->size)) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "failed to set key");
    return;
  }

  SET_NIL_RETURN();
}

/* ssl_context.set_key(addr, size) */
static void
c_ssl_context_set_key(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (argc != 2) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments");
    return;
  }

  /* Get SSL context pointer from wrapper */
  ssl_context_wrapper_t *wrapper = (ssl_context_wrapper_t *)v[0].instance->data;
  if (!wrapper->ptr) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "SSL context is not initialized");
    return;
  }

  /* Get addr and size arguments */
  mrbc_value addr_arg = GET_ARG(1);
  mrbc_value size_arg = GET_ARG(2);

  if (addr_arg.tt != MRBC_TT_INTEGER) {
    mrbc_raise(vm, MRBC_CLASS(TypeError), "addr must be an Integer");
    return;
  }
  if (size_arg.tt != MRBC_TT_INTEGER) {
    mrbc_raise(vm, MRBC_CLASS(TypeError), "size must be an Integer");
    return;
  }

  const void *addr = (const void *)(intptr_t)addr_arg.i;
  size_t size = (size_t)size_arg.i;

  /* Set key */
  if (!SSLContext_set_key(vm, wrapper->ptr, addr, size)) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "failed to set key");
    return;
  }

  SET_NIL_RETURN();
}

/*
 * ssl_context.verify_mode = mode -> Integer
 */
static void
c_ssl_context_set_verify_mode(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (argc != 1) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments");
    return;
  }

  /* Get SSL context pointer from wrapper */
  ssl_context_wrapper_t *wrapper = (ssl_context_wrapper_t *)v[0].instance->data;
  if (!wrapper->ptr) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "SSL context is not initialized");
    return;
  }

  /* Get verify_mode argument */
  mrbc_value mode_arg = GET_ARG(1);
  if (mode_arg.tt != MRBC_TT_INTEGER) {
    mrbc_raise(vm, MRBC_CLASS(TypeError), "verify_mode must be an Integer");
    return;
  }

  int mode = mode_arg.i;

  /* Set verify mode */
  if (!SSLContext_set_verify_mode(vm, wrapper->ptr, mode)) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "failed to set verify mode");
    return;
  }

  mrbc_incref(&mode_arg);
  SET_RETURN(mode_arg);
}

/*
 * ssl_context.verify_mode -> Integer
 */
static void
c_ssl_context_get_verify_mode(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (argc != 0) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments");
    return;
  }

  /* Get SSL context pointer from wrapper */
  ssl_context_wrapper_t *wrapper = (ssl_context_wrapper_t *)v[0].instance->data;
  if (!wrapper->ptr) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "SSL context is not initialized");
    return;
  }

  /* Get verify mode */
  int mode = SSLContext_get_verify_mode(vm, wrapper->ptr);
  if (mode < 0) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "failed to get verify mode");
    return;
  }

  SET_INT_RETURN(mode);
}


void
ssl_socket_init(mrbc_vm *vm, mrbc_class *class_BasicSocket)
{
  /* SSLSocket */
  mrbc_class *class_SSLSocket = mrbc_define_class(vm, "SSLSocket", class_BasicSocket);
  mrbc_define_destructor(class_SSLSocket, mrbc_ssl_socket_free);

  mrbc_define_method(vm, class_SSLSocket, "new", c_ssl_socket_new);
  mrbc_define_method(vm, class_SSLSocket, "open", c_ssl_socket_open);
  mrbc_define_method(vm, class_SSLSocket, "connect", c_ssl_socket_connect);
  mrbc_define_method(vm, class_SSLSocket, "send", c_ssl_socket_send);
  mrbc_define_method(vm, class_SSLSocket, "readpartial", c_ssl_socket_readpartial);
  mrbc_define_method(vm, class_SSLSocket, "read_nonblock", c_ssl_socket_read_nonblock);
  mrbc_define_method(vm, class_SSLSocket, "close", c_ssl_socket_close);
  mrbc_define_method(vm, class_SSLSocket, "closed?", c_ssl_socket_closed_q);
  mrbc_define_method(vm, class_SSLSocket, "ready?", c_ssl_socket_ready_q);
  mrbc_define_method(vm, class_SSLSocket, "remote_host", c_ssl_socket_remote_host);
  mrbc_define_method(vm, class_SSLSocket, "remote_port", c_ssl_socket_remote_port);


  /* SSLContext */
  mrbc_class *class_SSLContext = mrbc_define_class(vm, "SSLContext", mrbc_class_object);
  mrbc_define_destructor(class_SSLContext, mrbc_ssl_context_free);

  mrbc_define_method(vm, class_SSLContext, "new", c_ssl_context_new);
  mrbc_define_method(vm, class_SSLContext, "ca_file=", c_ssl_context_set_ca_file);
  mrbc_define_method(vm, class_SSLContext, "set_ca", c_ssl_context_set_ca);
  mrbc_define_method(vm, class_SSLContext, "set_ca_pem", c_ssl_context_set_ca_pem);
  mrbc_define_method(vm, class_SSLContext, "cert_file=", c_ssl_context_set_cert_file);
  mrbc_define_method(vm, class_SSLContext, "set_cert", c_ssl_context_set_cert);
  mrbc_define_method(vm, class_SSLContext, "set_cert_pem", c_ssl_context_set_cert_pem);
  mrbc_define_method(vm, class_SSLContext, "key_file=", c_ssl_context_set_key_file);
  mrbc_define_method(vm, class_SSLContext, "set_key", c_ssl_context_set_key);
  mrbc_define_method(vm, class_SSLContext, "set_key_pem", c_ssl_context_set_key_pem);
  mrbc_define_method(vm, class_SSLContext, "verify_mode=", c_ssl_context_set_verify_mode);
  mrbc_define_method(vm, class_SSLContext, "verify_mode", c_ssl_context_get_verify_mode);

  mrbc_value verify_none = mrbc_integer_value(SSL_VERIFY_NONE);
  mrbc_set_class_const(class_SSLContext, mrbc_str_to_symid("VERIFY_NONE"), &verify_none);
  mrbc_value verify_peer = mrbc_integer_value(SSL_VERIFY_PEER);
  mrbc_set_class_const(class_SSLContext, mrbc_str_to_symid("VERIFY_PEER"), &verify_peer);
}
