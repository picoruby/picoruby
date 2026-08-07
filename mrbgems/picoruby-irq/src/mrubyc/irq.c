#include <mrubyc.h>
#include "../../include/irq.h"

#if defined(PICORB_IRQ_EVENT_BRIDGE)
#include "c_task_queue.h"
#include "../../../picoruby-machine/include/hal.h"
#endif

/*
 * IRQ.register_gpio(pin, event_type, opts)
 */
static void
c_register_gpio(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (argc != 3) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments");
    return;
  }

  if (v[1].tt != MRBC_TT_INTEGER || v[2].tt != MRBC_TT_INTEGER) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "pin and event_type must be integers");
    return;
  }

  int pin = v[1].i;
  int event_type = v[2].i;

  /* Extract debounce from opts hash */
  uint32_t debounce_ms = 0;
  if (argc >= 3 && v[3].tt == MRBC_TT_HASH) {
    mrbc_value debounce_key = mrbc_symbol_value(mrbc_str_to_symid("debounce"));
    mrbc_value debounce_val = mrbc_hash_get(&v[3], &debounce_key);
    if (debounce_val.tt == MRBC_TT_INTEGER) {
      debounce_ms = (uint32_t)debounce_val.i;
    }
  }

  int irq_id = IRQ_register_gpio(pin, event_type, debounce_ms);

  if (irq_id < 0) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "Failed to register GPIO IRQ");
    return;
  }

  SET_INT_RETURN(irq_id);
}

/*
 * IRQ.unregister_gpio(id)
 */
static void
c_unregister_gpio(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (argc != 1) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments");
    return;
  }

  if (v[1].tt != MRBC_TT_INTEGER) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "irq_id must be integer");
    return;
  }

  int irq_id = v[1].i;
  bool prev_state = IRQ_unregister_gpio(irq_id);

  if (prev_state) {
    SET_TRUE_RETURN();
  } else {
    SET_FALSE_RETURN();
  }
}

/*
 * IRQ.peek_event()
 */
static void
c_peek_event(mrbc_vm *vm, mrbc_value *v, int argc)
{
  int irq_id, event_type;
  bool has_event = IRQ_peek_event(&irq_id, &event_type);

  mrbc_value result = mrbc_array_new(vm, 2);

  if (has_event) {
    mrbc_array_set(&result, 0, &mrbc_integer_value(irq_id));
  } else {
    mrbc_array_set(&result, 0, &mrbc_nil_value());
  }
  mrbc_array_set(&result, 1, &mrbc_integer_value(event_type));

  SET_RETURN(result);
}

#if defined(PICORB_IRQ_EVENT_BRIDGE)

/*
 * ISR-to-task event bridge (VM side)
 *
 * Mirror of src/mruby/irq.c; see that file and include/irq.h for the
 * design. The differences are mruby/c's: the bindings are held as
 * reference-counted values rather than rooted in a GC-visible array,
 * and the scheduler hook is process-global, so there is no owner VM to
 * track.
 */

static mrbc_value irq_bindings_[IRQ_MAX_SOURCES];

static int
irq_check_id(mrbc_vm *vm, mrbc_value *v, mrbc_int_t id)
{
  if (id < 0 || IRQ_MAX_SOURCES <= id) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "IRQ source id out of range");
    return 0;
  }
  return 1;
}

static int
irq_lowest_bit(uint32_t v)
{
#if defined(__GNUC__) || defined(__clang__)
  return __builtin_ctz(v);
#else
  int n = 0;
  while ((v & UINT32_C(1)) == 0) {
    v >>= 1;
    n++;
  }
  return n;
#endif
}

static void
irq_unbind_source(int id)
{
  mrbc_decref(&irq_bindings_[id]);
  irq_bindings_[id] = mrbc_nil_value();
}

/*
 * Scheduler hook: turn every ready source into at most one token.
 *
 * A source leaves the ready set only after it has been dealt with, and
 * the token is recorded only once the push happened, so an allocation
 * failure inside the push cannot leave a source claimed forever.
 */
static void
irq_drain(void *ud)
{
  uint32_t ready;

  (void)ud;
  ready = IRQ_peek_ready();
  while (ready) {
    int id = irq_lowest_bit(ready);
    ready &= ready - 1;

    if (mrbc_type(irq_bindings_[id]) == MRBC_TT_NIL) {
      /* Nobody is listening yet. The bits stay latched; IRQ.bind
         re-asserts the ready bit, so a late consumer still sees them. */
      IRQ_clear_ready(id);
      continue;
    }
    if (IRQ_is_enqueued(id)) {
      /* A token is already outstanding; it covers these bits. */
      IRQ_clear_ready(id);
      continue;
    }
    mrbc_value token = mrbc_integer_value(id);
    switch (mrbc_task_queue_push(&irq_bindings_[id], &token)) {
      case MRBC_TASK_QUEUE_PUSH_OK:
      case MRBC_TASK_QUEUE_PUSH_OK_WOKE:
        IRQ_mark_enqueued(id);
        break;
      case MRBC_TASK_QUEUE_PUSH_CLOSED:
      case MRBC_TASK_QUEUE_PUSH_INVALID:
        /* The consumer is gone. Drop the binding rather than retry at
           every scheduler entry for the rest of the process. The bits
           stay latched for whoever binds next. */
        irq_unbind_source(id);
        break;
    }
    IRQ_clear_ready(id);
  }
}

/*
 * IRQ.bind(id, queue) -> queue
 */
static void
c_bind(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (argc != 2 || v[1].tt != MRBC_TT_INTEGER) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "IRQ.bind(source, queue)");
    return;
  }
  mrbc_int_t id = v[1].i;
  if (!irq_check_id(vm, v, id)) return;

  if (!mrbc_obj_is_kind_of(&v[2], MRBC_CLASS(Task_Queue))) {
    mrbc_raise(vm, MRBC_CLASS(TypeError), "IRQ.bind expects a Task::Queue");
    return;
  }
  if (mrbc_type(irq_bindings_[id]) == MRBC_TT_OBJECT &&
      irq_bindings_[id].instance == v[2].instance) {
    /* rebinding the same queue changes nothing */
    mrbc_incref(&v[2]);   /* the reference handed back to the caller */
    SET_RETURN(v[2]);
    return;
  }
  if (IRQ_is_enqueued(id)) {
    /* The outstanding token names this source, and the new consumer has
       no way to know the old queue still holds it. */
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "IRQ source has an undelivered token");
    return;
  }
  mrbc_decref(&irq_bindings_[id]);
  mrbc_incref(&v[2]);   /* the reference the table keeps */
  irq_bindings_[id] = v[2];
  if (IRQ_peek_pending((int)id) != 0) {
    IRQ_mark_ready((int)id);
  }
  mrbc_incref(&v[2]);   /* the reference handed back to the caller */
  SET_RETURN(v[2]);
}

/*
 * IRQ.unbind(id) -> true if a binding was removed
 */
static void
c_unbind(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (argc != 1 || v[1].tt != MRBC_TT_INTEGER) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "IRQ.unbind(source)");
    return;
  }
  mrbc_int_t id = v[1].i;
  if (!irq_check_id(vm, v, id)) return;

  if (mrbc_type(irq_bindings_[id]) == MRBC_TT_NIL) {
    SET_FALSE_RETURN();
    return;
  }
  if (IRQ_is_enqueued(id)) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "IRQ source has an undelivered token");
    return;
  }
  irq_unbind_source((int)id);
  SET_TRUE_RETURN();
}

/*
 * IRQ.take(id) -> event bits (0 is possible: tokens may be spurious)
 */
static void
c_take(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (argc != 1 || v[1].tt != MRBC_TT_INTEGER) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "IRQ.take(source)");
    return;
  }
  mrbc_int_t id = v[1].i;
  if (!irq_check_id(vm, v, id)) return;

  SET_INT_RETURN((mrbc_int_t)IRQ_take_bits((int)id));
}

/*
 * IRQ.simulate(id, bits) -> nil
 */
static void
c_simulate(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (argc != 2 || v[1].tt != MRBC_TT_INTEGER || v[2].tt != MRBC_TT_INTEGER) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "IRQ.simulate(source, bits)");
    return;
  }
  mrbc_int_t id = v[1].i;
  if (!irq_check_id(vm, v, id)) return;

  IRQ_signal_from_isr((int)id, (uint32_t)v[2].i);
  SET_NIL_RETURN();
}

/*
 * IRQ.gpio_source -> the source id shared by every GPIO interrupt
 */
static void
c_gpio_source(mrbc_vm *vm, mrbc_value *v, int argc)
{
  SET_INT_RETURN(IRQ_SRC_GPIO);
}

static void
irq_bridge_init(mrbc_vm *vm, mrbc_class *module_IRQ)
{
  int i = 0;

  while (i < IRQ_MAX_SOURCES) {
    irq_bindings_[i] = mrbc_nil_value();
    i++;
  }
  IRQ_reset();

  mrbc_define_method(vm, module_IRQ, "bind", c_bind);
  mrbc_define_method(vm, module_IRQ, "unbind", c_unbind);
  mrbc_define_method(vm, module_IRQ, "take", c_take);
  mrbc_define_method(vm, module_IRQ, "simulate", c_simulate);
  mrbc_define_method(vm, module_IRQ, "gpio_source", c_gpio_source);
  mrbc_set_class_const(module_IRQ, mrbc_str_to_symid("MAX_SOURCES"),
                       &mrbc_integer_value(IRQ_MAX_SOURCES));

  /* Not mrbc_task_set_scheduler_hook: the hook slot is shared with the
     platform's own servicing. See include/hal.h in picoruby-machine. */
  picorb_scheduler_service_add(irq_drain, NULL);
}

#endif /* PICORB_IRQ_EVENT_BRIDGE */

#define SET_MODULE_CONST(mod, cst) \
  mrbc_set_const(mod, mrbc_str_to_symid(#cst), &mrbc_integer_value(cst))

void
mrbc_irq_init(mrbc_vm *vm)
{
  mrbc_class *module_IRQ = mrbc_define_module(vm, "IRQ");

  /* Define module methods */
  mrbc_define_method(vm, module_IRQ, "register_gpio", c_register_gpio);
  mrbc_define_method(vm, module_IRQ, "unregister_gpio", c_unregister_gpio);
  mrbc_define_method(vm, module_IRQ, "peek_event", c_peek_event);

#if defined(PICORB_IRQ_EVENT_BRIDGE)
  irq_bridge_init(vm, module_IRQ);
#endif

  /* Initialize the IRQ system */
  IRQ_init();
}
