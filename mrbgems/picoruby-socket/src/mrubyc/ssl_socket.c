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

  /* Get TCP socket and SSL context arguments */
  mrbc_value tcp_socket_obj = GET_ARG(1);
  mrbc_value ssl_context_obj = GET_ARG(2);
  ssl_context_wrapper_t *ctx_wrapper = (ssl_context_wrapper_t *)ssl_context_obj.instance->data;

  if (!ctx_wrapper->ptr) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "second argument must be an SSLContext");
    return;
  }

  /* Create instance with wrapper structure */
  mrbc_value instance = mrbc_instance_new(vm, v->cls, sizeof(ssl_socket_wrapper_t));
  ssl_socket_wrapper_t *wrapper = (ssl_socket_wrapper_t *)instance.instance->data;

  /* Create SSL socket */
  wrapper->ptr = SSLSocket_create(ctx_wrapper->ptr);
  if (!wrapper->ptr) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "failed to create SSL socket");
    return;
  }

  /* Get socket pointer from TCPSocket instance */
  if (tcp_socket_obj.tt != MRBC_TT_OBJECT) {
    SSLSocket_close(wrapper->ptr);
    wrapper->ptr = NULL;
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "first argument must be a TCPSocket");
    return;
  }

  void *data = tcp_socket_obj.instance->data;
  picorb_socket_t **sock_ptr = (picorb_socket_t **)data;
  picorb_socket_t *tcp_sock = *sock_ptr;

  if (!tcp_sock) {
    SSLSocket_close(wrapper->ptr);
    wrapper->ptr = NULL;
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "TCPSocket is not initialized");
    return;
  }

  /* Get remote_host and remote_port from TCPSocket */
  const char *host = TCPSocket_remote_host(tcp_sock);
  int port = TCPSocket_remote_port(tcp_sock);

  if (!host) {
    SSLSocket_close(wrapper->ptr);
    wrapper->ptr = NULL;
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "TCPSocket must have remote_host");
    return;
  }

  if (port < 0) {
    SSLSocket_close(wrapper->ptr);
    wrapper->ptr = NULL;
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "TCPSocket must have remote_port");
    return;
  }

  /* Set hostname and port from TCPSocket */
  if (!SSLSocket_set_hostname(wrapper->ptr, host)) {
    SSLSocket_close(wrapper->ptr);
    wrapper->ptr = NULL;
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "failed to set hostname");
    return;
  }

  if (!SSLSocket_set_port(wrapper->ptr, port)) {
    SSLSocket_close(wrapper->ptr);
    wrapper->ptr = NULL;
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "failed to set port");
    return;
  }

  /* Close the original TCP socket to avoid resource leak */
  /* The SSL connection will create its own TCP socket internally */
  if (tcp_sock && !TCPSocket_closed(tcp_sock)) {
    TCPSocket_close(tcp_sock);
  }

  /* Store references for GC protection */
  mrbc_instance_setiv(&instance, mrbc_str_to_symid("@tcp_socket"), &tcp_socket_obj);
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
  if (!SSLContext_set_cert_file(wrapper->ptr, cert_file)) {
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
  if (!SSLContext_set_cert(wrapper->ptr, addr, size)) {
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
  if (!SSLContext_set_key_file(wrapper->ptr, key_file)) {
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

  /* Set certificate */
  if (!SSLContext_set_key(wrapper->ptr, addr, size)) {
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
  mrbc_define_method(vm, class_SSLContext, "cert_file=", c_ssl_context_set_cert_file);
  mrbc_define_method(vm, class_SSLContext, "set_cert", c_ssl_context_set_cert);
  mrbc_define_method(vm, class_SSLContext, "key_file=", c_ssl_context_set_key_file);
  mrbc_define_method(vm, class_SSLContext, "set_key", c_ssl_context_set_key);
  mrbc_define_method(vm, class_SSLContext, "verify_mode=", c_ssl_context_set_verify_mode);
  mrbc_define_method(vm, class_SSLContext, "verify_mode", c_ssl_context_get_verify_mode);

  mrbc_value verify_none = mrbc_integer_value(SSL_VERIFY_NONE);
  mrbc_set_class_const(class_SSLContext, mrbc_str_to_symid("VERIFY_NONE"), &verify_none);
  mrbc_value verify_peer = mrbc_integer_value(SSL_VERIFY_PEER);
  mrbc_set_class_const(class_SSLContext, mrbc_str_to_symid("VERIFY_PEER"), &verify_peer);
}
