module MIDIBASE
  class Router
    class Emitter
      def initialize(router, source)
        @router = router
        @source = source
      end

      def handle(event, timestamp_us: nil, **_context)
        @router.emit(@source, event, timestamp_us: timestamp_us)
      end
    end

    def initialize
      @routes = []
    end

    def connect(source, sink, priority: 0, only: nil, except: nil)
      unless sink.respond_to?(:handle)
        raise ArgumentError, "MIDI sink must respond to handle"
      end
      @routes << [source, sink, priority, only, except]
      self
    end

    def output(source)
      Emitter.new(self, source)
    end

    def emit(source, event, timestamp_us: nil)
      command = event[0]
      unless command.is_a?(Symbol)
        raise ArgumentError, "MIDI event must start with a command Symbol"
      end
      timestamp_us ||= Machine.uptime_us
      i = 0
      routes = @routes
      routes_size = routes.size # memoize for performance
      while i < routes_size
        route = routes[i]
        if route[0] == source && route_accepts?(route, command)
          route[1].handle(
            event,
            source: source,
            priority: route[2],
            timestamp_us: timestamp_us
          )
        end
        i += 1
      end
      event
    end

    private def route_accepts?(route, command)
      only = route[3]
      except = route[4]
      return false if only && !only.include?(command)
      return false if except && except.include?(command)
      true
    end
  end
end
