module MIDIBASE
  class Router
    class Emitter
      def initialize(router, source)
        @router = router
        @source = source
      end

      def handle(event, timestamp_us: nil, **_context)
        @router.emit_midi(@source, event, timestamp_us || Machine.uptime_us)
      end

      def handle_midi(event, _source, _priority, timestamp_us)
        @router.emit_midi(@source, event, timestamp_us)
      end
    end

    def initialize
      @routes = []
    end

    def connect(source, sink, priority: 0, only: nil, except: nil)
      unless sink.respond_to?(:handle)
        raise ArgumentError, "MIDI sink must respond to handle"
      end
      @routes << [source, sink, priority, only, except, sink.respond_to?(:handle_midi)]
      self
    end

    def output(source)
      Emitter.new(self, source)
    end

    def emit(source, event, timestamp_us: nil)
      emit_midi(source, event, timestamp_us || Machine.uptime_us)
    end

    # Allocation-conscious path for real-time MIDI producers. Context is
    # positional so mruby does not build keyword argument Hashes per event.
    def emit_midi(source, event, timestamp_us)
      command = event[0]
      unless command.is_a?(Symbol)
        raise ArgumentError, "MIDI event must start with a command Symbol"
      end
      i = 0
      routes = @routes
      routes_size = routes.size # memoize for performance
      while i < routes_size
        route = routes[i]
        if route[0] == source && route_accepts?(route, command)
          if route[5]
            route[1].handle_midi(event, source, route[2], timestamp_us)
          else
            route[1].handle(
              event,
              source: source,
              priority: route[2],
              timestamp_us: timestamp_us
            )
          end
        end
        i += 1
      end
      event
    end

    private def route_accepts?(route, command)
      # route[3]: only
      # route[4]: except
      only = route[3]
      if only
        accepted = false
        i = 0
        only_size = only.size
        while i < only_size
          if only[i] == command
            accepted = true
            break
          end
          i += 1
        end
        return false unless accepted
      end
      except = route[4]
      if except
        i = 0
        except_size = except.size
        while i < except_size
          return false if except[i] == command
          i += 1
        end
      end
      true
    end
  end
end
