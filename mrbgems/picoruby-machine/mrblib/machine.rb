class Machine
  def self.using_delay(&block)
    $machine_delay = :delay
    block.call
  ensure
    $machine_delay = nil
  end

  def self.using_busy_wait(&block)
    $machine_delay = :busy_wait
    block.call
  ensure
    $machine_delay = nil
  end
end

class Object
  alias sleep_ms_orig sleep_ms
  alias sleep_orig    sleep

  def sleep_ms(ms)
    case $machine_delay
    when :delay
      # delay_ms will hang in IRQ handler
      Machine.delay_ms(ms)
    when :busy_wait
      Machine.busy_wait_ms(ms)
    else
      sleep_ms_orig(ms)
    end
  end

  def sleep(sec)
    case $machine_delay
    when :delay
      # delay_ms will hang in IRQ handler
      Machine.delay_ms (sec * 1000).to_i
    when :busy_wait
      Machine.busy_wait_ms (sec * 1000).to_i
    else
      sleep_orig(sec)
    end
  end
end
