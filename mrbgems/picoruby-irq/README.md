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
irq_instance = gpio.irq(GPIO::EDGE_FALL) do |peripheral, event_type|
  puts "Button pressed! Event: #{event_type}"
end

# Process IRQ events in main loop
loop do
  count = IRQ.process  # Process up to 5 events
  sleep_ms(10)
end
```

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

See [example/irq_gpio.rb](example/irq_gpio.rb)

## License

MIT License
