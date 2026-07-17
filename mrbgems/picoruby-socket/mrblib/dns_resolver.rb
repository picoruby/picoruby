if Object.const_defined?(:SocketDNSResolver)
  class SocketDNSResolver
    def self.resolve_host(host)
      new(host).resolve(host)
    end

    def resolve(host)
      status = __status
      if status == 1
        unless @event_queue.pop(timeout_ms: 10_000)
          __abandon
          raise SocketError, "DNS resolution timed out for #{host}"
        end
        status = __status
      end
      address = __address
      __release
      return address if status == 2 && address

      raise SocketError, "DNS resolution failed for #{host}"
    end
  end
end
