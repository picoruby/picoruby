/*
 * MQTT VM switching layer
 */

#if defined(PICORB_VM_MRUBY)
  #include "mruby/mqtt.c"
#elif defined(PICORB_VM_MRUBYC)
  #include "mrubyc/mqtt.c"
#endif