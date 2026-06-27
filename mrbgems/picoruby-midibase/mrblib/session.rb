module MIDIBASE
  class Session
    attr_reader :shutdown_error

    def initialize(*resources)
      i = 0
      resources_size = resources.size
      while i < resources_size
        resource = resources[i]
        unless resource.respond_to?(:stop) || resource.respond_to?(:join)
          raise ArgumentError, "managed resource must respond to stop or join"
        end
        i += 1
      end
      @resources = resources
      @stopped = false
      @handler_installed = false
      @previous_int_handler = nil
      @shutdown_error = nil
    end

    def run
      raise ArgumentError, "block required" unless block_given?
      raise RuntimeError, "session already stopped" if @stopped

      session = self
      # The shell dispatches SIGINT before stopping the sandbox's main task.
      @previous_int_handler = Signal.trap(:INT) { session.stop }
      @handler_installed = true
      begin
        while !@stopped
          yield
        end
      ensure
        shutdown
      end
      shutdown_error = @shutdown_error
      raise shutdown_error.message if shutdown_error
      self
    end

    def stop
      shutdown
      self
    end

    def stopped?
      @stopped
    end

    private def shutdown
      return if @stopped
      @stopped = true
      resources = @resources
      resources_size = resources.size
      i = 0
      while i < resources_size
        resource = resources[i]
        invoke_shutdown(resource, :stop) if resource.respond_to?(:stop)
        invoke_shutdown(resource, :join) if resource.respond_to?(:join)
        i += 1
      end
    ensure
      restore_int_handler
    end

    private def invoke_shutdown(resource, method_name)
      resource.__send__(method_name)
    rescue Exception => e
      @shutdown_error ||= e
    end

    private def restore_int_handler
      return unless @handler_installed
      Signal.trap(:INT, @previous_int_handler || "DEFAULT")
      @handler_installed = false
    end
  end
end
