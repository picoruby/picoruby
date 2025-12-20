module Funicular
  class EnvironmentInquirer
    def initialize(env)
      @environment = env.to_s
    end

    def to_s
      @environment
    end

    def inspect
      @environment.inspect
    end

    def ==(other)
      @environment == other.to_s
    end

    def method_missing(method_name, *args, &block)
      if method_name.to_s.end_with?("?")
        env_name = method_name.to_s.chomp("?")
        return @environment == env_name
      else
        super
      end
    end

    def respond_to_missing?(method_name, include_private = false)
      method_name.to_s.end_with?("?") || super
    end
  end

end

