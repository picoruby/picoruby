module PSG
  class Driver
    def initialize(type, **opt)
      if Driver.initialized?
        raise RuntimeError, "PSG::Driver already initialized"
      end
      case type
      when :pwm
        if opt[:left].nil? || opt[:right].nil?
          raise ArgumentError, "Missing required options for PWM driver"
        end
        Driver.select_pwm(opt[:left], opt[:right])
      when :mcp4921, :mcp4922
        if opt[:mosi].nil? || opt[:sck].nil? || opt[:cs].nil? || opt[:ldac].nil?
          raise ArgumentError, "Missing required options for MCP4922 driver"
        end
        dac = case type
              when :mcp4921
                1
              when :mcp4922
                2
              else
                -1
              end
        Driver.select_mcp492x(dac, opt[:mosi], opt[:sck], opt[:cs], opt[:ldac])
      else
        raise ArgumentError, "Unsupported driver type: #{type}"
      end
    end
  end
end
