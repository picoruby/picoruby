# picoruby-irq

Interrupt Request (IRQ) handling for PicoRuby, providing asynchronous event processing for GPIO pins and other peripherals.

## Features

- **GPIO Interrupt Support**: Handle level and edge-triggered interrupts on GPIO pins
- **Event Queue**: Non-blocking event queue with configurable size
- **Debounce Support**: Built-in debouncing to filter out rapid, repeated events
- **Multiple Event Types**: Support for `LEVEL_LOW`, `LEVEL_HIGH`, `EDGE_FALL`, and `EDGE_RISE`
- **Resource Management**: Automatic registration and cleanup of interrupt handlers

## Usage

### Basic GPIO IRQ

```ruby
require 'irq'

gpio = GPIO.new(17, GPIO::IN|GPIO::PULL_UP)  # GPIO pin

# Register IRQ handler for falling edge
irq_instance = gpio.irq(GPIO::EDGE_FALL, capture: "My IRQ") do |peripheral, event_type, capture|
  puts "#{capture} -- Button pressed! Event: #{event_type}"
end
```

On PicoRuby (the mruby runtime), one call then delivers events to
every registered handler, with no polling. Delivery through
`IRQ.start` runs in a hidden dispatcher task; do not also poll with
`IRQ.process`, which runs handlers on whichever task calls it:

```ruby
IRQ.start
```

On FemtoRuby (mruby/c), `IRQ.start` is not available yet; poll
instead:

```ruby
loop do
  count = IRQ.process  # Process up to 5 events
  sleep_ms(10)
end
```

For `capture`, see [example/irq_gpio_femtoruby.rb](example/irq_gpio_femtoruby.rb)

### IRQ with Debouncing

```ruby
# Register IRQ with 50ms debounce to filter out button bounce
irq_instance = gpio.irq(GPIO::EDGE_FALL, debounce: 50) do |peripheral, event_type|
  puts "Debounced button press detected"
end
```

### Multiple Event Types

```ruby
# Handle both rising and falling edges
irq_instance = gpio.irq(GPIO::EDGE_FALL | GPIO::EDGE_RISE) do |peripheral, event_type|
  case event_type
  when GPIO::EDGE_FALL
    puts "Button pressed"
  when GPIO::EDGE_RISE  
    puts "Button released"
  end
end
```

### Level-Triggered IRQs

```ruby
# Handle low level (useful for active-low sensors)
irq_instance = gpio.irq(GPIO::LEVEL_LOW) do |peripheral, event_type|
  puts "Sensor active"
end
```

### IRQ Management

```ruby
# Check if IRQ is enabled
puts irq_instance.enabled?  # => true

# Temporarily disable IRQ
previous_state = irq_instance.disable
puts irq_instance.enabled?  # => false

# Re-enable IRQ
irq_instance.enable
puts irq_instance.enabled?  # => true

# Unregister IRQ completely
irq_instance.unregister
```

### Manual Event Processing

```ruby
# Process specific number of events
processed_count = IRQ.process(10)  # Process up to 10 events
puts "Processed #{processed_count} events"
```

## Event delivery: IRQ.start / IRQ.stop

`IRQ.process` above is polling: something has to keep asking. On
PicoRuby (the mruby runtime), `IRQ.start` inverts that -- register
handlers on the peripherals you care about, flip the one switch, and
they are called when their events arrive:

```ruby
gpio.irq(GPIO::EDGE_FALL) { |peri, ev| puts "pressed" }

uart.irq(UART::RX_RECEIVE) do |u, ev|
  while line = u.gets
    handle(line)
  end
end

IRQ.start   # from here on, both handlers fire without any loop
```

`IRQ.stop` releases everything and lets a finished program exit: the
scheduler runs until no task is left waiting, so while delivery is
started the VM stays alive to serve interrupts, and after `IRQ.stop`
nothing lingers to hold the program open. Registrations survive a
stop; the next `IRQ.start` picks them up again, and events that
arrived in between are delivered then, coalesced. Both calls are
idempotent.

Three rules:

1. **Handlers run in a hidden dispatcher task**, not in the task that
   registered them. Anything they touch is shared state.
2. **A raising handler does not stop delivery.** The exception is
   reported and delivery continues, for every peripheral.
3. **One call may stand for a burst.** Events are coalesced at the
   interrupt level, so a handler drains its peripheral -- reads until
   empty -- rather than assuming one call means one event.

`IRQ.start` refuses (with `ArgumentError`) a source that is already
bound by hand through the queue-level API below -- it will not steal
it. FemtoRuby (mruby/c) raises `NotImplementedError` for now: it
cannot create the dispatcher task from a block. There, poll with
`IRQ.process`, or use the queue-level API.

### For driver authors

Any peripheral gem can plug into this without touching IRQ internals.
The protocol is one method and one include:

```ruby
class MySensor
  include IRQ                 # provides #irq registration

  def event_source_id         # the bridge source this device signals
    IRQ_SRC_MYSENSOR
  end
end
```

The gem's interrupt handler signals event bits through
`IRQ_signal_from_isr(source, bits)`; the bits ARE the event-type mask
that `MySensor#irq(mask) { |peri, event_type, capture| }` filters on,
so new event kinds are new bits, with no API change. See picoruby-uart
(`UART::RX_RECEIVE`) for a worked example.

### The queue-level API

An interrupt handler cannot enter the VM, so it only stages work: it ORs
event bits into a *source* and marks that source ready. At every
scheduler entry, before the ready queue is read, PicoRuby turns each
ready source into one token in the queue you bound to it. A task blocked
on that queue becomes runnable in the same scheduler iteration.

```ruby
q = Task::Queue.new
IRQ.bind(IRQ::SOURCE::GPIO, q)

Task.new do
  while source = q.pop
    IRQ.take(source)   # required: this is what allows the next token
    # One token can stand for any number of interrupts, so drain rather
    # than assume it means exactly one event.
    while 0 < IRQ.process
    end
  end
end
```

Source ids are compile-time constants, never allocated and never reused.
GPIO uses one source for the whole subsystem: the token only says "the
GPIO event queue is worth looking at", and `IRQ.process` is still what
dispatches events to their handlers.

### Contract

1. **Tokens are edge-latched, coalesced notifications** -- one token per
   "something happened since you last took the bits", not one per
   interrupt. A burst of interrupts produces a single token whose bits
   are ORed together, so the queue can never grow without bound.
2. **Consumers must tolerate spurious tokens.** `IRQ.take` may return 0;
   treat that as "nothing to do" rather than an error, and drain the
   underlying peripheral until it reports empty rather than assuming one
   token means exactly one event.
3. **Late binding works, but coalesced.** Bits signalled while nothing
   was bound are not lost: `IRQ.bind` re-asserts the source, and the
   accumulated bits arrive on the next scheduler entry as one token.
4. **Rebinding or unbinding a source with an undelivered token raises.**
   Take the bits first. Rebinding the same queue is always a no-op.

`IRQ.simulate(source, bits)` drives the bridge exactly as an interrupt
handler would, for tests and for bringing a consumer up on a host build.
Note that on a host build there are no GPIO interrupts to bind to.

The bridge is available on both runtimes. A build configured without a
task scheduler does not define these methods, and pays nothing for
them.

## Constants

### GPIO Event Types

- `GPIO::LEVEL_LOW` (1): Trigger when pin is at low level
- `GPIO::LEVEL_HIGH` (2): Trigger when pin is at high level
- `GPIO::EDGE_FALL` (4): Trigger on falling edge (high → low)
- `GPIO::EDGE_RISE` (8): Trigger on rising edge (low → high)

Event types can be combined using bitwise OR:
```ruby
gpio.irq(GPIO::EDGE_FALL | GPIO::EDGE_RISE) { |gpio, event| ... }
```

## Example: Button with LED

- PicoRuby (IRQ.start): [example/irq_gpio_picoruby.rb](example/irq_gpio_picoruby.rb)
- FemtoRuby (IRQ.process polling): [example/irq_gpio_femtoruby.rb](example/irq_gpio_femtoruby.rb)

## License

MIT License
