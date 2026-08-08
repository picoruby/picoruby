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
        # register vetted the protocol, so this can only be a bridge
        # registration.
        _unregister_bridge(id, irq)
      end
      return prev_registered_p
    end

    def register(irq, opts)
      peri = irq.peripheral
      id = case peri
      when GPIO
        register_gpio(peri.pin, irq.event_type, opts)
      else
        # The extension protocol: any peripheral that exposes its
        # bridge source id gets callback delivery, one IRQInstance per
        # registration, filtered by the event mask. UART is the first
        # taker; a build whose glue defines no source (no bridge) fails
        # here, visibly.
        unless peri.respond_to?(:event_source_id)
          # NotImplementedError, matching every other no-bridge path:
          # the usual reason to land here is a build whose glue defines
          # no source (ESP32), not a wrong argument.
          raise NotImplementedError, "IRQ: #{peri.class} has no event source in this build"
        end
        _register_bridge(irq)
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

    # -- Event delivery ----------------------------------------------
    #
    # IRQ.start is the one switch a user program flips: after it,
    # every handler registered on a peripheral -- gpio.irq, uart.irq
    # -- is called when its event arrives, with no polling loop.
    # IRQ.stop releases everything and retires the hidden dispatcher
    # task, which is what lets a finished program exit: the scheduler
    # only ends when no task is left waiting. Registrations survive a
    # stop; start picks them up again.
    #
    # Peripherals plug in through one protocol: an object that
    # responds to #event_source_id has its events delivered through
    # the ISR-to-task bridge, one IRQInstance per registration,
    # filtered by the event mask it registered with -- the bridge bits
    # ARE the event-type mask, which is what leaves room for more
    # conditions per peripheral later. GPIO predates the bridge and
    # keeps its own C event queue; its delivery is a pump that runs
    # IRQ.process, subscribed as one more internal handler.
    #
    # PicoRuby (mruby) only: FemtoRuby cannot create the dispatcher
    # task from a block. There, IRQ.process polling keeps working, as
    # does the queue-level API below.

    def start
      unless RUBY_ENGINE == "mruby"
        raise NotImplementedError, "IRQ.start needs PicoRuby: FemtoRuby cannot spawn the dispatcher task"
      end
      unless respond_to?(:bind)
        raise NotImplementedError, "this build has no event bridge"
      end
      # @type var already: bool
      already = false
      _with_api_lock do
        if @started
          already = true
        else
          @started = true
          begin
            # The GPIO pump: one subscription drains the legacy event
            # queue into the per-pin handlers, however many there are.
            # Unconditional, so "started" has one meaning: the VM
            # serves interrupts until IRQ.stop.
            on(SOURCE::GPIO) do |bits|
              # A raising handler aborts only its own IRQ.process
              # batch -- its event was already consumed -- so resume
              # the drain until the queue reports empty; isolation is
              # per handler, and IRQ.process itself keeps its polling
              # contract of letting exceptions reach the caller.
              done = false
              while !done
                begin
                  done = IRQ.process == 0
                rescue => e
                  puts "IRQ: GPIO handler raised #{e.class}: #{e.message}"
                end
              end
            end
            insts = @bridge_instances
            if insts
              keys = insts.keys
              keys_size = keys.size
              i = 0
              while i < keys_size
                list = insts[keys[i]]
                _bridge_activate(keys[i]) if list && 0 < list.size
                i += 1
              end
            end
          rescue => e
            # A source someone bound by hand raises out of on; undo
            # the half-made start so a retry is possible.
            _stop_locked
            raise e
          end
        end
      end
      !already
    end

    def stop
      # Guards mirror off: stop must be callable from ensure on any
      # engine and any build, and mean "nothing to stop" there.
      return false unless RUBY_ENGINE == "mruby"
      return false unless respond_to?(:bind)
      # @type var stopped: bool
      stopped = false
      _with_api_lock do
        stopped = _stop_locked
      end
      stopped
    end

    def _stop_locked
      return false unless @started
      @started = false
      off(SOURCE::GPIO)
      insts = @bridge_instances
      if insts
        keys = insts.keys
        keys_size = keys.size
        i = 0
        while i < keys_size
          off(keys[i])
          i += 1
        end
      end
      true
    end

    def _register_bridge(irq)
      unless RUBY_ENGINE == "mruby"
        raise NotImplementedError, "IRQ delivery needs PicoRuby: FemtoRuby cannot spawn the dispatcher task"
      end
      unless respond_to?(:bind)
        raise NotImplementedError, "this build has no event bridge"
      end
      # @type var peri: untyped
      peri = irq.peripheral
      # @type var id: Integer
      id = 0
      _with_api_lock do
        source = peri.event_source_id
        insts = (@bridge_instances ||= {}) # steep:ignore UnannotatedEmptyCollection
        list = (insts[source] ||= [])
        ids = (@bridge_ids ||= {}) # steep:ignore UnannotatedEmptyCollection
        # Negative, so they can share HANDLER with the C-side GPIO ids
        # (0, 1, ...) and never collide.
        id = (@bridge_next_id ||= 0) - 1
        @bridge_next_id = id
        list << irq
        ids[id] = source
        if @started && list.size == 1
          begin
            _bridge_activate(source)
          rescue => e
            # The source is bound by hand; take the registration back
            # out so a failed irq() leaves no trace.
            list.delete(irq)
            insts.delete(source) if list.empty?
            ids.delete(id)
            raise e
          end
        end
      end
      id
    end

    def _unregister_bridge(id, irq)
      # @type var removed: bool
      removed = false
      _with_api_lock do
        ids = @bridge_ids
        source = ids ? ids.delete(id) : nil
        unless source.nil?
          removed = true
          insts = @bridge_instances
          list = insts ? insts[source] : nil
          if list
            list.delete(irq)
            if list.empty?
              insts.delete(source) if insts
              # The last instance releases the source completely, so
              # the queue-level API can pick it up from scratch.
              off(source) if @started
            end
          end
        end
      end
      removed
    end

    def _bridge_activate(source)
      on(source) do |bits|
        _deliver_bridge(source, bits)
      end
    end

    # Runs in the dispatcher task, outside every lock, like any other
    # handler: a slow peripheral handler must not block on/off or a
    # registration. A registration racing a delivery may or may not
    # receive it, which is within the contract either way.
    def _deliver_bridge(source, bits)
      insts = @bridge_instances
      list = insts ? insts[source] : nil
      return if list.nil?
      # Snapshot: a handler may unregister itself mid-delivery, and a
      # live indexed walk would then skip its neighbor. An instance
      # removed after the snapshot may still see this delivery --
      # in-flight, which the contract allows.
      list = list.dup
      list_size = list.size
      i = 0
      while i < list_size
        instance = list[i]
        matched = instance ? bits & instance.event_type : 0
        # Only the conditions this instance asked for; an event kind
        # nobody registered for is dropped, exactly as a GPIO event
        # with no handler is.
        if instance && 0 < matched
          begin
            instance.call(matched)
          rescue => e
            # Isolation is per handler, not per source: the neighbors
            # still get their bits.
            puts "IRQ: handler for source #{source} raised #{e.class}: #{e.message}"
          end
        end
        i += 1
      end
      nil
    end

    # -- Event-bridge callback layer (internal) ----------------------
    #
    # on/off are the engine under IRQ.start and the peripheral
    # protocol; they are private and may change without notice. One
    # dispatcher task serves every source that has a handler, so the
    # cost does not grow with sources.
    #
    # The dispatcher is created on first use and lives for as long as
    # any handler is registered. The off that removes the last handler
    # asks it to retire, and it retires by returning from its own
    # loop, never by Task#close on a live task
    # (close does not unwind, so a forced close would skip ensure
    # blocks -- including a user handler's). Retiring matters: the
    # scheduler only finishes when no task is left waiting, so a
    # dispatcher parked in pop forever would keep the VM alive after
    # the program is done with interrupts. The retired task is closed
    # by the next on, because Task.new roots every task it creates
    # and a task cannot close itself.

    def on(source, &handler)
      unless RUBY_ENGINE == "mruby"
        raise NotImplementedError, "IRQ.on needs PicoRuby: FemtoRuby cannot spawn the dispatcher task"
      end
      raise ArgumentError, "IRQ.on requires a block" if handler.nil?
      unless respond_to?(:bind)
        raise NotImplementedError, "this build has no event bridge"
      end
      _with_dispatch_lock do
        handlers = (@on_handlers ||= {}) # steep:ignore UnannotatedEmptyCollection
        if handlers[source]
          raise ArgumentError, "IRQ source #{source} already has a handler"
        end
        queue = @on_queue
        if queue.nil?
          queue = Task::Queue.new
          @on_queue = queue
        end
        # Register before binding: a token can reach the dispatcher the
        # moment the source is bound, and a token with no handler would
        # be given back rather than delivered.
        handlers[source] = handler
        # One C call, so no window in which another task's manual
        # IRQ.bind could land between a check and a bind and then be
        # silently stolen. Any failure -- a refused binding, but also
        # the C side rejecting the source itself -- must take the
        # registration back out: a dead entry would keep
        # handlers.empty? false forever, and the dispatcher would
        # never retire.
        bound = false
        begin
          bound = _bind_if_unbound(source, queue)
        ensure
          handlers.delete(source) unless bound
        end
        unless bound
          raise ArgumentError, "IRQ source #{source} is already bound to a queue"
        end
        # Reap dispatchers that retired at a previous last off. They
        # ended by returning from their own loop, so DORMANT here is
        # final and close only frees them; one that is still on its way
        # out is left for a later on.
        retired = @on_retired
        if retired
          i = 0
          while i < retired.size
            if retired[i].status == :DORMANT
              retired[i].close
              retired.delete_at(i)
            else
              i += 1
            end
          end
        end
        # The task comes last: a token pushed before it exists just
        # waits in the queue, so nothing is lost, and a failed bind
        # above never has a task to clean up.
        if @on_task.nil?
          @on_task = Task.new(name: "irq_dispatcher") do
            # Explicit receiver: a Task block runs with self == main, not
            # the lexical self, so an implicit call would NoMethodError
            # and kill the dispatcher at birth, silently.
            IRQ._dispatch_loop(queue, handlers)
          end
        end
      end
      source
    end

    def off(source)
      # The engine guard is not only symmetry with IRQ.on: reading a
      # module ivar crashes mruby/c, and off must be safe to call from
      # an ensure block on either VM. The bridge guard keeps a build
      # without the bridge at "nothing to remove" instead of NoMethodError.
      return false unless RUBY_ENGINE == "mruby"
      return false unless respond_to?(:bind)
      removed = false
      _with_dispatch_lock do
        handlers = @on_handlers
        if handlers && !handlers.delete(source).nil?
          removed = true
          # One C call, conditional on the binding still being ours:
          # written as take-then-unbind in Ruby, a scheduler entry
          # between the two can push a fresh token, and another task's
          # public IRQ.bind can replace the binding -- which the unbind
          # would then tear down along with the new consumer's events.
          queue = @on_queue
          _unbind_if_bound(source, queue) if queue
          # Last handler gone: ask the dispatcher to retire. -1 is no
          # source (sources are validated 0 <= src), so the loop can
          # tell the request from a token. It is a request, not an
          # order -- the dispatcher re-checks under the lock, and an on
          # that lands before it wakes simply keeps it. One request is
          # enough, and the flag keeps rapid on/off cycles from filling
          # the queue with them; the loop clears it on arrival.
          if handlers.empty? && !@on_task.nil? && !@on_retire_pending && queue
            queue.push(-1)
            @on_retire_pending = true
          end
        end
      end
      removed
    end

    # Serializes start/stop and the bridge registry. Separate from the
    # dispatch lock because holders of this one call on/off, which
    # take the dispatch lock inside -- one queue-mutex is not
    # reentrant. The ordering is always api -> dispatch, never the
    # reverse, so the two cannot deadlock.
    def _with_api_lock
      lock = @api_lock
      raise "IRQ: api lock missing" if lock.nil?
      lock.pop
      begin
        yield
      ensure
        lock.push(true)
      end
    end

    # Serializes every on/off/delivery state transition. A Task::Queue
    # holding one token is the mutex: pop parks the waiter in WAITING,
    # so the holder keeps getting scheduled -- a try-lock spun with
    # Task.pass was rejected because a high-priority spinner starves
    # the low-priority holder forever. The queue itself is created at
    # load time, below, while the VM is still single-tasked, which is
    # what makes its own creation race-free.
    def _with_dispatch_lock
      lock = @on_lock
      # respond_to?(:bind) held, so the load-time bootstrap ran.
      raise "IRQ: dispatch lock missing" if lock.nil?
      lock.pop
      begin
        yield
      ensure
        lock.push(true)
      end
    end

    def _dispatch_loop(queue, handlers)
      while true
        src = queue.pop
        break if src.nil? # never expected: the queue is not closed
        if src == -1
          # A retire request from the off that removed the last
          # handler. Re-checked under the lock: an on that arrived
          # since keeps this dispatcher, and the stale request is
          # discarded.
          retire = false
          _with_dispatch_lock do
            # The one in-flight request has arrived; the next off may
            # send another.
            @on_retire_pending = false
            if handlers.empty?
              # Hand this task to the next on for closing -- Task.new
              # rooted it, and a task cannot close itself.
              task = @on_task
              if task
                retired = (@on_retired ||= []) # steep:ignore UnannotatedEmptyCollection
                retired << task
              end
              @on_task = nil
              retire = true
            end
          end
          break if retire
          next
        end
        # @type var bits: Integer
        bits = 0
        # @type var h: Proc?
        h = nil
        _with_dispatch_lock do
          # Take and handler snapshot are one atomic step. Split, they
          # misdeliver: preempted between the two, an off plus an on
          # can swap in a new handler, and bits taken before the off
          # would reach it -- not the in-flight delivery the contract
          # allows, but a delivery across generations.
          bits = take(src)
          h = handlers[src]
          if h.nil? && 0 < bits
            # A stale token: off removed the handler after this token
            # was pushed. The bits are not this dispatcher's to drop --
            # they may belong to whoever binds the source next -- so
            # put them back as a fresh signal; late binding holds them
            # until then.
            simulate(src, bits)
          end
        end
        # The handler runs outside the lock, so it may itself call on
        # or off, and a slow handler never blocks them. A spurious
        # token (bits == 0) stands for data a previous delivery
        # already saw; there is nothing to hand over.
        if h && 0 < bits
          begin
            h.call(bits)
          rescue => e
            # One failing handler must not stop delivery for every
            # other source.
            puts "IRQ: handler for source #{src} raised #{e.class}: #{e.message}"
          end
        end
      end
    end

    # Everything below IRQ.start's public surface is internal and may
    # change without notice. _dispatch_loop stays public only because
    # the dispatcher task must call it with an explicit receiver.
    if RUBY_ENGINE == "mruby"
      private :on, :off, :_stop_locked, :_register_bridge,
              :_unregister_bridge, :_bridge_activate, :_deliver_bridge,
              :_with_api_lock, :_with_dispatch_lock
    end
  end

  # The dispatch and api locks (see _with_dispatch_lock and
  # _with_api_lock). Module bodies run while the VM is still
  # single-tasked, so this cannot race; the guards keep it off
  # FemtoRuby (module ivars crash mruby/c) and off builds without the
  # task scheduler.
  if RUBY_ENGINE == "mruby" && Object.const_defined?(:Task)
    lock = Task::Queue.new
    lock.push(true)
    @on_lock = lock
    lock = Task::Queue.new
    lock.push(true)
    @api_lock = lock
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
