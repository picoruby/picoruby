require 'gpio'

module IRQ
  MAX_PROCESS_COUNT = 5
  HANDLER = {}

  class << self
    def unregister(id)
      unless irq = HANDLER.delete(id)
        raise "IRQ not registered: #{id}"
      end
      prev_registered_p = case irq.peripheral
      when GPIO
        unregister_gpio(id)
      else
        raise "Fatal: This should not happen"
      end
      return prev_registered_p
    end

    def register(irq, opts)
      id = case irq.peripheral
      when GPIO
        register_gpio(irq.peripheral.pin, irq.event_type, opts)
      else
        raise "Unsupported peripheral for IRQ: #{irq.peripheral.class}"
      end
      HANDLER[id] = irq
      return id
    end

    # Returns the number of events dispatched. The limit is checked
    # before peek_event, not after: peek_event removes the event from
    # the queue, so testing the limit afterwards threw one event away
    # on every call that hit the limit.
    def process(max_count = MAX_PROCESS_COUNT)
      count = 0
      while count < max_count
        id, event_type = peek_event
        break if id.nil?
        if irq = HANDLER[id]
          irq.call(event_type)
          count += 1
        end
      end
      return count
    end
  end

  def irq(event_type, **opts, &callback)
    # @type var irq_peri: IRQ::irq_peri_t
    irq_peri = self
    instance = IRQInstance.new(irq_peri, event_type, opts, callback)
    return instance
  end

  class IRQInstance
    def initialize(peripheral, event_type, opts, callback)
      @peripheral = peripheral
      @callback = callback
      @event_type = event_type
      @enabled = true
      @capture = opts.delete(:capture)
      @id = IRQ.register(self, opts)
    end

    attr_accessor :capture
    attr_reader :peripheral, :event_type

    def call(event_type)
      return unless @enabled
      @callback&.call(@peripheral, event_type, @capture)
    end

    def enabled?
      @enabled
    end

    def enable
      previous = @enabled
      @enabled = true
      return previous
    end

    def disable
      previous = @enabled
      @enabled = false
      return previous
    end

    def unregister
      IRQ.unregister(@id)
    end
  end
end

class GPIO
  include IRQ
  # Corresponding to ENUM in picoruby-irq/include/irq.h
  LEVEL_LOW = 1
  LEVEL_HIGH = 2
  EDGE_FALL = 4
  EDGE_RISE = 8
end
