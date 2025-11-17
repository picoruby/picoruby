#include <stdlib.h>
#include <string.h>
#include <stdint.h>

typedef struct {
  picorb_ssl_socket_t *ptr;
} ssl_socket_wrapper_t;

static void
mrbc_ssl_socket_free(mrbc_value *self)
{
  ssl_socket_wrapper_t *wrapper = (ssl_socket_wrapper_t *)self->instance->data;
  if (wrapper->ptr && !SSLSocket_closed(wrapper->ptr)) {
    SSLSocket_close(wrapper->ptr);
    wrapper->ptr = NULL;
  }
}

typedef struct {
  picorb_ssl_context_t *ptr;
} ssl_context_wrapper_t;

static void
mrbc_ssl_context_free(mrbc_value *self)
{
  ssl_context_wrapper_t *wrapper = (ssl_context_wrapper_t *)self->instance->data;
  if (wrapper->ptr) {
    SSLContext_free(wrapper->ptr);
    wrapper->ptr = NULL;
  }
}


/*
 * SSLSocket.new(tcp_socket, ssl_context) -> SSLSocket
 */
static void
c_ssl_socket_new(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (argc != 2) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments");
    return;
  }

  /* Get TCP socket argument */
  mrbc_value tcp_socket_obj = GET_ARG(1);
  picorb_socket_t *tcp_socket = (picorb_socket_t *)tcp_socket_obj.instance->data;

  if (!tcp_socket) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "first argument must be a TCPSocket");
    return;
  }

  if (!tcp_socket->connected) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "TCP socket is not connected");
    return;
  }

  /* Get SSL context argument */
  mrbc_value ssl_context_obj = GET_ARG(2);
  ssl_context_wrapper_t *ctx_wrapper = (ssl_context_wrapper_t *)ssl_context_obj.instance->data;

  if (!ctx_wrapper->ptr) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "second argument must be an SSLContext");
    return;
  }

  /* Create instance with wrapper structure */
  mrbc_value instance = mrbc_instance_new(vm, v->cls, sizeof(ssl_socket_wrapper_t));
  ssl_socket_wrapper_t *wrapper = (ssl_socket_wrapper_t *)instance.instance->data;

  /* Create SSL socket wrapping TCP socket and store pointer */
  wrapper->ptr = SSLSocket_create(tcp_socket, ctx_wrapper->ptr);
  if (!wrapper->ptr) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "failed to create SSL socket");
    return;
  }

  mrbc_incref(&tcp_socket_obj);
  //mrbc_incref(&ssl_context_obj);

  SET_RETURN(instance);
}

/*
 * ssl_socket.hostname = hostname -> String
 */
static void
c_ssl_socket_set_hostname(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (argc != 1) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments");
    return;
  }

  /* Get SSL socket pointer from wrapper */
  ssl_socket_wrapper_t *wrapper = (ssl_socket_wrapper_t *)v[0].instance->data;
  if (!wrapper->ptr) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "SSL socket is not initialized");
    return;
  }

  /* Get hostname argument */
  mrbc_value hostname_arg = GET_ARG(1);
  if (hostname_arg.tt != MRBC_TT_STRING) {
    mrbc_raise(vm, MRBC_CLASS(TypeError), "hostname must be a String");
    return;
  }

  const char *hostname = (const char *)hostname_arg.string->data;

  /* Set hostname */
  if (!SSLSocket_set_hostname(wrapper->ptr, hostname)) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "failed to set hostname");
    return;
  }

  mrbc_incref(&hostname_arg);
  SET_RETURN(hostname_arg);
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
  if (!SSLSocket_connect(wrapper->ptr)) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "SSL handshake failed");
    return;
  }
}

/*
 * ssl_socket.write(data) -> Integer
 */
static void
c_ssl_socket_write(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (argc != 1) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments");
    return;
  }

  /* Get SSL socket pointer from wrapper */
  ssl_socket_wrapper_t *wrapper = (ssl_socket_wrapper_t *)v[0].instance->data;
  if (!wrapper->ptr) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "SSL socket is not initialized");
    return;
  }

  /* Check argument type */
  mrbc_value data = GET_ARG(1);
  if (data.tt != MRBC_TT_STRING) {
    mrbc_raise(vm, MRBC_CLASS(TypeError), "data must be a String");
    return;
  }

  /* Send data */
  ssize_t sent = SSLSocket_send(wrapper->ptr, (const void *)data.string->data, data.string->size);
  if (sent < 0) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "SSL send failed");
    return;
  }

  SET_INT_RETURN(sent);
}

/*
 * ssl_socket.read(maxlen = 4096) -> String or nil
 */
static void
c_ssl_socket_read(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (argc > 1) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments");
    return;
  }

  /* Get SSL socket pointer from wrapper */
  ssl_socket_wrapper_t *wrapper = (ssl_socket_wrapper_t *)v[0].instance->data;
  if (!wrapper->ptr) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "SSL socket is not initialized");
    return;
  }

  /* Get maxlen parameter (default: 4096) */
  int maxlen = 4096;
  if (argc == 1) {
    mrbc_value maxlen_arg = GET_ARG(1);
    if (maxlen_arg.tt != MRBC_TT_INTEGER) {
      mrbc_raise(vm, MRBC_CLASS(TypeError), "maxlen must be an Integer");
      return;
    }
    maxlen = (int)maxlen_arg.i;
    if (maxlen <= 0) {
      mrbc_raise(vm, MRBC_CLASS(ArgumentError), "maxlen must be positive");
      return;
    }
  }

  /* Allocate buffer */
  char *buffer = (char *)mrbc_raw_alloc(maxlen);
  if (!buffer) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "failed to allocate buffer");
    return;
  }

  /* Receive data */
  ssize_t received = SSLSocket_recv(wrapper->ptr, buffer, maxlen);

  if (received < 0) {
    mrbc_raw_free(buffer);
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "SSL recv failed");
    return;
  }

  if (received == 0) {
    /* EOF */
    mrbc_raw_free(buffer);
    SET_NIL_RETURN();
    return;
  }

  /* Create string and return */
  mrbc_value ret = mrbc_string_new(vm, buffer, received);
  mrbc_raw_free(buffer);
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
  SSLSocket_close(wrapper->ptr);

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
  bool is_closed = SSLSocket_closed(wrapper->ptr);
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
  const char *host = SSLSocket_remote_host(wrapper->ptr);
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
  int port = SSLSocket_remote_port(wrapper->ptr);
  if (port < 0) {
    SET_NIL_RETURN();
    return;
  }

  SET_INT_RETURN(port);
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

  /* Create SSL context and store pointer */
  wrapper->ptr = SSLContext_create();
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
  if (!SSLContext_set_ca_file(wrapper->ptr, ca_file)) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "failed to set CA file");
    return;
  }

  mrbc_incref(&ca_file_arg);
  SET_RETURN(ca_file_arg);
#else
  (void)argc;
  (void)v;
  mrbc_raise(vm, MRBC_CLASS(NotImplementedError), "ca_file= is not supported on this platform. Use set_ca_cert instead");
#endif
}

/* ssl_context.set_ca_cert(addr, size) */
static void
c_ssl_context_set_ca_cert(mrbc_vm *vm, mrbc_value *v, int argc)
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
  if (!SSLContext_set_ca_cert(wrapper->ptr, addr, size)) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "failed to set CA certificate");
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
  if (!SSLContext_set_verify_mode(wrapper->ptr, mode)) {
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
  int mode = SSLContext_get_verify_mode(wrapper->ptr);
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
  mrbc_define_method(vm, class_SSLSocket, "hostname=", c_ssl_socket_set_hostname);
  mrbc_define_method(vm, class_SSLSocket, "connect", c_ssl_socket_connect);
  mrbc_define_method(vm, class_SSLSocket, "write", c_ssl_socket_write);
  mrbc_define_method(vm, class_SSLSocket, "read", c_ssl_socket_read);
  mrbc_define_method(vm, class_SSLSocket, "close", c_ssl_socket_close);
  mrbc_define_method(vm, class_SSLSocket, "closed?", c_ssl_socket_closed_q);
  mrbc_define_method(vm, class_SSLSocket, "remote_host", c_ssl_socket_remote_host);
  mrbc_define_method(vm, class_SSLSocket, "remote_port", c_ssl_socket_remote_port);


  /* SSLContext */
  mrbc_class *class_SSLContext = mrbc_define_class(vm, "SSLContext", mrbc_class_object);
  mrbc_define_destructor(class_SSLContext, mrbc_ssl_context_free);

  mrbc_define_method(vm, class_SSLContext, "new", c_ssl_context_new);
  mrbc_define_method(vm, class_SSLContext, "ca_file=", c_ssl_context_set_ca_file);
  mrbc_define_method(vm, class_SSLContext, "set_ca_cert", c_ssl_context_set_ca_cert);
  mrbc_define_method(vm, class_SSLContext, "verify_mode=", c_ssl_context_set_verify_mode);
  mrbc_define_method(vm, class_SSLContext, "verify_mode", c_ssl_context_get_verify_mode);

  mrbc_value verify_none = mrbc_integer_value(SSL_VERIFY_NONE);
  mrbc_set_class_const(class_SSLContext, mrbc_str_to_symid("VERIFY_NONE"), &verify_none);
  mrbc_value verify_peer = mrbc_integer_value(SSL_VERIFY_PEER);
  mrbc_set_class_const(class_SSLContext, mrbc_str_to_symid("VERIFY_PEER"), &verify_peer);
}
