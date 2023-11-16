class Machine
  def self.using_delay(&block)
    $machine_delay = true
    block.call
  ensure
    $machine_delay = false
  end
end

class Object
  alias sleep_ms_orig sleep_ms

  def sleep_ms(ms)
    if $machine_delay
      Machine.delay_ms(ms)
    else
      sleep_ms_orig(ms)
    end
  end
end
