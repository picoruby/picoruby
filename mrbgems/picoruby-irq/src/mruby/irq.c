#include <mruby.h>
#include <mruby/presym.h>
#include <mruby/variable.h>
#include <mruby/string.h>
#include <mruby/hash.h>
#include <mruby/array.h>
#include <mruby/class.h>

#include "../../include/irq.h"

#if defined(PICORB_IRQ_EVENT_BRIDGE)
#include "task.h"
/* Spelled out: picoruby-mruby/include/hal.h shadows this one on the
   include path. */
#include "../../../picoruby-machine/include/hal.h"
#endif

/*
 * IRQ.register_gpio(pin, event_type, opts)
 */
static mrb_value
mrb_s_register_gpio(mrb_state *mrb, mrb_value self)
{
  mrb_int pin;
  mrb_int event_type;
  mrb_value opts;
  mrb_get_args(mrb, "iiH", &pin, &event_type, &opts);

  /* Extract debounce from opts hash */
  uint32_t debounce_ms = 0;
  if (!mrb_nil_p(opts)) {
    mrb_value debounce_val = mrb_hash_get(mrb, opts, mrb_symbol_value(MRB_SYM(debounce)));
    if (!mrb_nil_p(debounce_val)) {
      debounce_ms = (uint32_t)mrb_integer(debounce_val);
    }
  }

  int irq_id = IRQ_register_gpio(pin, event_type, debounce_ms);

  if (irq_id < 0) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "Failed to register GPIO IRQ");
  }

  return mrb_fixnum_value(irq_id);
}

/*
 * IRQ.unregister_gpio(id)
 */
static mrb_value
mrb_s_unregister_gpio(mrb_state *mrb, mrb_value self)
{
  mrb_int irq_id;
  mrb_get_args(mrb, "i", &irq_id);

  bool prev_state = IRQ_unregister_gpio(irq_id);
  return mrb_bool_value(prev_state);
}

/*
 * IRQ.peek_event()
 */
static mrb_value
mrb_s_peek_event(mrb_state *mrb, mrb_value self)
{
  int irq_id, event_type;
  bool has_event = IRQ_peek_event(&irq_id, &event_type);

  mrb_value result = mrb_ary_new_capa(mrb, 2);
  if (has_event) {
    mrb_ary_push(mrb, result, mrb_fixnum_value(irq_id));
  } else {
    mrb_ary_push(mrb, result, mrb_nil_value());
  }
  mrb_ary_push(mrb, result, mrb_fixnum_value(event_type));

  return result;
}

#if defined(PICORB_IRQ_EVENT_BRIDGE)

/*
 * ISR-to-task event bridge (VM side)
 *
 * The C core (src/irq_source.c) only latches bits. This is where a
 * ready source becomes a token in a Task::Queue, which is why it runs
 * in VM context: pushing allocates.
 *
 * Exactly one mrb_state owns the bridge, because the source table is a
 * process-global that ISRs write to -- a second owner could not tell
 * whose queue an event belongs to. The owner is claimed at gem init and
 * released at gem final; a second VM attempting to initialize the bridge
 * raises.
 */

static mrb_state *irq_owner_;
static mrb_value irq_bindings_;   /* Array(IRQ_MAX_SOURCES) of queue-or-nil */

static void
irq_check_id(mrb_state *mrb, mrb_int id)
{
  if (id < 0 || IRQ_MAX_SOURCES <= id) {
    mrb_raisef(mrb, E_ARGUMENT_ERROR, "IRQ source id out of range: %i", id);
  }
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

/*
 * Scheduler hook: turn every ready source into at most one token.
 *
 * Runs in thread context at every scheduler entry, right before the
 * ready-queue read, so a task woken here runs in the same iteration.
 * Kept cheap: with no pending event this is one atomic load.
 *
 * A push can allocate, so it can raise, and the exception escapes into
 * the scheduler. That is deliberate -- a silently dropped interrupt is
 * worse than a visible failure -- but it must not cost the event. Hence
 * a source is cleared from the ready set only after it has been dealt
 * with, and the token is recorded only after the push actually
 * happened: an interrupted drain retries that source at the next
 * scheduler entry instead of leaving it claimed forever.
 */
static void
irq_drain(mrb_state *mrb, void *ud)
{
  uint32_t ready;

  (void)ud;
  ready = IRQ_peek_ready();
  while (ready) {
    int id = irq_lowest_bit(ready);
    ready &= ready - 1;

    mrb_value queue = mrb_ary_entry(irq_bindings_, id);
    if (mrb_nil_p(queue)) {
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
    switch (mrb_task_queue_push(mrb, queue, mrb_fixnum_value(id))) {
      case MRB_TASK_QUEUE_PUSH_OK:
        IRQ_mark_enqueued(id);
        break;
      case MRB_TASK_QUEUE_PUSH_CLOSED:
      case MRB_TASK_QUEUE_PUSH_INVALID:
        /* The consumer is gone. Drop the binding rather than retry at
           every scheduler entry for the rest of the process. The bits
           stay latched for whoever binds next. */
        mrb_ary_set(mrb, irq_bindings_, id, mrb_nil_value());
        break;
    }
    IRQ_clear_ready(id);
  }
}

/*
 * IRQ.bind(id, queue) -> queue
 *
 * Tokens are edge-latched and coalesced: the queue receives the source
 * id once per "something happened since you last took the bits", not
 * once per interrupt. Binding a source that already has pending bits
 * delivers them on the next scheduler entry.
 */
static mrb_value
mrb_s_bind(mrb_state *mrb, mrb_value self)
{
  mrb_int id;
  mrb_value queue, current;
  struct RClass *queue_class;

  mrb_get_args(mrb, "io", &id, &queue);
  irq_check_id(mrb, id);

  queue_class = mrb_class_get_under_id(mrb, mrb_class_get_id(mrb, MRB_SYM(Task)), MRB_SYM(Queue));
  if (!mrb_obj_is_kind_of(mrb, queue, queue_class)) {
    mrb_raise(mrb, E_TYPE_ERROR, "IRQ.bind expects a Task::Queue");
  }

  current = mrb_ary_entry(irq_bindings_, id);
  if (mrb_obj_eq(mrb, current, queue)) {
    return queue;  /* rebinding the same queue changes nothing */
  }
  if (IRQ_is_enqueued(id)) {
    /* The outstanding token names this source, and the new consumer has
       no way to know the old queue still holds it. Make the caller take
       the bits first. */
    mrb_raise(mrb, E_RUNTIME_ERROR, "IRQ source has an undelivered token");
  }
  mrb_ary_set(mrb, irq_bindings_, id, queue);
  if (IRQ_peek_pending((int)id) != 0) {
    IRQ_mark_ready((int)id);
  }
  return queue;
}

/*
 * IRQ.unbind(id) -> true if a binding was removed
 */
static mrb_value
mrb_s_unbind(mrb_state *mrb, mrb_value self)
{
  mrb_int id;

  mrb_get_args(mrb, "i", &id);
  irq_check_id(mrb, id);

  if (mrb_nil_p(mrb_ary_entry(irq_bindings_, id))) {
    return mrb_false_value();
  }
  if (IRQ_is_enqueued(id)) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "IRQ source has an undelivered token");
  }
  mrb_ary_set(mrb, irq_bindings_, id, mrb_nil_value());
  return mrb_true_value();
}

/*
 * IRQ.take(id) -> event bits (0 is possible: tokens may be spurious)
 */
static mrb_value
mrb_s_take(mrb_state *mrb, mrb_value self)
{
  mrb_int id;

  mrb_get_args(mrb, "i", &id);
  irq_check_id(mrb, id);
  return mrb_fixnum_value((mrb_int)IRQ_take_bits((int)id));
}

/*
 * IRQ.simulate(id, bits) -> nil
 *
 * Drives the bridge from VM context exactly as an interrupt handler
 * would. For tests and for bringing up a consumer on a host build,
 * where there is no interrupt to fire.
 */
static mrb_value
mrb_s_simulate(mrb_state *mrb, mrb_value self)
{
  mrb_int id, bits;

  mrb_get_args(mrb, "ii", &id, &bits);
  irq_check_id(mrb, id);
  IRQ_signal_from_isr((int)id, (uint32_t)bits);
  return mrb_nil_value();
}

/*
 * IRQ._bind_if_unbound(id, queue) -> whether the binding was made
 *
 * IRQ.on's binding step. Tasks switch only at instruction boundaries,
 * so one C call is indivisible; a check-then-bind written in Ruby
 * leaves a window in which another task's manual IRQ.bind lands
 * between the two and is then silently stolen. Refuses any existing
 * binding instead of replacing it; otherwise the semantics are
 * bind's, including re-asserting bits that arrived while nothing was
 * bound.
 */
static mrb_value
mrb_s_bind_if_unbound(mrb_state *mrb, mrb_value self)
{
  mrb_int id;
  mrb_value queue, current;
  struct RClass *queue_class;

  mrb_get_args(mrb, "io", &id, &queue);
  irq_check_id(mrb, id);

  queue_class = mrb_class_get_under_id(mrb, mrb_class_get_id(mrb, MRB_SYM(Task)), MRB_SYM(Queue));
  if (!mrb_obj_is_kind_of(mrb, queue, queue_class)) {
    mrb_raise(mrb, E_TYPE_ERROR, "IRQ.bind expects a Task::Queue");
  }
  current = mrb_ary_entry(irq_bindings_, id);
  if (mrb_obj_eq(mrb, current, queue)) {
    return mrb_true_value();   /* already this queue; nothing to do */
  }
  if (!mrb_nil_p(current)) {
    return mrb_false_value();  /* someone else's binding; never steal it */
  }
  mrb_ary_set(mrb, irq_bindings_, id, queue);
  if (IRQ_peek_pending((int)id) != 0) {
    IRQ_mark_ready((int)id);
  }
  return mrb_true_value();
}

/*
 * IRQ._unbind_if_bound(id, queue) -> whether the binding was removed
 *
 * IRQ.off's release step, the mirror of _bind_if_unbound: it touches
 * the source only while the binding still belongs to `queue`. Written
 * in Ruby as take-then-unbind it has two windows -- a scheduler entry
 * between the two can push a fresh token, and another task's public
 * IRQ.bind can replace the binding, which the unbind would then tear
 * down. One C call has neither: tasks switch only at instruction
 * boundaries, and the drain that creates tokens only runs at scheduler
 * entries. An interrupt landing mid-call merely latches bits, which
 * stay latched for whoever binds the source next.
 */
static mrb_value
mrb_s_unbind_if_bound(mrb_state *mrb, mrb_value self)
{
  mrb_int id;
  mrb_value queue, current;

  mrb_get_args(mrb, "io", &id, &queue);
  irq_check_id(mrb, id);

  current = mrb_ary_entry(irq_bindings_, id);
  if (!mrb_obj_eq(mrb, current, queue)) {
    return mrb_false_value();  /* not ours (anymore); leave it alone */
  }
  /* Take discards the undelivered bits -- that is what off means --
     and releases the outstanding-token state, which is what makes
     removing the binding legal. A token already pushed stays in the
     queue; the dispatcher resolves it as stale. */
  IRQ_take_bits((int)id);
  mrb_ary_set(mrb, irq_bindings_, id, mrb_nil_value());
  return mrb_true_value();
}

static void
irq_bridge_init(mrb_state *mrb, struct RClass *module_IRQ)
{
  mrb_value bindings;
  struct RClass *irq_singleton;
  int i;

  if (irq_owner_ != NULL) {
    /* Nothing has been touched yet, so the first owner keeps working. */
    mrb_raise(mrb, E_RUNTIME_ERROR, "IRQ event bridge is owned by another VM");
  }

  bindings = mrb_ary_new_capa(mrb, IRQ_MAX_SOURCES);
  i = 0;
  while (i < IRQ_MAX_SOURCES) {
    mrb_ary_push(mrb, bindings, mrb_nil_value());
    i++;
  }
  /* Rooted once for the lifetime of the gem. Registering per binding
     would be wrong: mrb_gc_unregister removes ALL occurrences of an
     object, so two bindings to one queue would unroot it too early. */
  mrb_gc_register(mrb, bindings);

  mrb_define_module_function_id(mrb, module_IRQ, MRB_SYM(bind), mrb_s_bind, MRB_ARGS_REQ(2));
  mrb_define_module_function_id(mrb, module_IRQ, MRB_SYM(unbind), mrb_s_unbind, MRB_ARGS_REQ(1));
  mrb_define_module_function_id(mrb, module_IRQ, MRB_SYM(take), mrb_s_take, MRB_ARGS_REQ(1));
  mrb_define_module_function_id(mrb, module_IRQ, MRB_SYM(simulate), mrb_s_simulate, MRB_ARGS_REQ(2));
  /* IRQ::SOURCE::GPIO -- the source shared by every GPIO interrupt.
     One source for the whole subsystem, not one per pin: the token
     only says "the GPIO event queue is worth looking at", and
     IRQ.process is what sorts events out to their handlers. A
     constant, not a method: source ids are compile-time constants and
     should read as such. Platform sources (UART units and friends)
     are deliberately NOT exported -- they are port-specific, and
     peripheral objects hand out their own via #event_source_id. */
  {
    struct RClass *source_mod = mrb_define_module_under_id(mrb, module_IRQ, MRB_SYM(SOURCE));
    mrb_define_const_id(mrb, source_mod, MRB_SYM(GPIO), mrb_fixnum_value(IRQ_SRC_GPIO));
  }
  /* Private singleton methods, not module functions: module_function
     would also mix them into every includer as private instance
     methods (UART gains #irq via include IRQ). Defined private from
     the start; class << self code still reaches them through the
     implicit receiver. */
  irq_singleton = mrb_singleton_class_ptr(mrb, mrb_obj_value(module_IRQ));
  mrb_define_private_method_id(mrb, irq_singleton, MRB_SYM(_bind_if_unbound), mrb_s_bind_if_unbound, MRB_ARGS_REQ(2));
  mrb_define_private_method_id(mrb, irq_singleton, MRB_SYM(_unbind_if_bound), mrb_s_unbind_if_bound, MRB_ARGS_REQ(2));
  mrb_define_const(mrb, module_IRQ, "MAX_SOURCES", mrb_fixnum_value(IRQ_MAX_SOURCES));

  irq_bindings_ = bindings;
  IRQ_reset();
  /* Not mrb_task_set_scheduler_hook: the hook slot is shared with the
     platform's own servicing (cyw43_arch poll on pico_w/pico2_w), and
     setting it directly would silently disable whichever registered
     first. See include/hal.h in picoruby-machine. */
  picorb_scheduler_service_add(mrb, irq_drain, NULL);

  /* Claim ownership only once nothing above can still raise. Otherwise
     a failed init would leave the bridge owned by a VM that does not
     exist, and every later mrb_open would be refused. */
  irq_owner_ = mrb;
}

static void
irq_bridge_final(mrb_state *mrb)
{
  if (irq_owner_ != mrb) return;

  picorb_scheduler_service_remove(mrb, irq_drain, NULL);
  IRQ_reset();
  mrb_gc_unregister(mrb, irq_bindings_);
  irq_bindings_ = mrb_nil_value();
  irq_owner_ = NULL;
}

#endif /* PICORB_IRQ_EVENT_BRIDGE */

void
mrb_picoruby_irq_gem_init(mrb_state *mrb)
{
  struct RClass *module_IRQ = mrb_define_module(mrb, "IRQ");

  /* Define singleton methods */
  mrb_define_module_function_id(mrb, module_IRQ, MRB_SYM(register_gpio), mrb_s_register_gpio, MRB_ARGS_REQ(3));
  mrb_define_module_function_id(mrb, module_IRQ, MRB_SYM(unregister_gpio), mrb_s_unregister_gpio, MRB_ARGS_REQ(1));
  mrb_define_class_method_id(mrb, module_IRQ, MRB_SYM(peek_event), mrb_s_peek_event, MRB_ARGS_NONE());

#if defined(PICORB_IRQ_EVENT_BRIDGE)
  irq_bridge_init(mrb, module_IRQ);
#endif

  IRQ_init(); // Initialize the IRQ system
}

void
mrb_picoruby_irq_gem_final(mrb_state *mrb)
{
#if defined(PICORB_IRQ_EVENT_BRIDGE)
  irq_bridge_final(mrb);
#endif
}
