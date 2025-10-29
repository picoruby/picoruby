class Shell
  class SandboxPool
    def initialize(size: 3)
      @pool = []
      @available = []
      @in_use = []

      size.times do |i|
        sandbox = Sandbox.new("pool_#{i}")
        @pool << sandbox
        @available << sandbox
      end
    end

    def acquire
      sandbox = @available.shift

      unless sandbox
        # Pool exhausted - create temporary sandbox
        # This happens when all pooled sandboxes are held by suspended jobs
        sandbox = Sandbox.new("temp_#{Time.now.to_i}")
      end

      @in_use << sandbox
      sandbox
    end

    def release(sandbox)
      # Remove from in_use array (mruby/c doesn't have Array#delete)
      @in_use.reject! { |sb| sb == sandbox }

      if @pool.include?(sandbox)
        # Pooled sandbox - reset and return to available
        sandbox.reset
        @available << sandbox unless @available.include?(sandbox)
      else
        # Temporary sandbox - terminate it
        sandbox.terminate
      end
    end

    def cleanup
      # Cleanup any temporary sandboxes that are no longer in use
      @in_use.reject! do |sandbox|
        unless @pool.include?(sandbox)
          if sandbox.state == :DORMANT
            sandbox.terminate
            true
          else
            false
          end
        else
          false
        end
      end
    end
  end
end
