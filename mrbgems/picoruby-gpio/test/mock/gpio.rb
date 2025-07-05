# Mock for GPIO C extension
class GPIO
  # Constants
  IN          = 1
  OUT         = 2
  PULL_UP     = 4
  PULL_DOWN   = 8
  OPEN_DRAIN  = 16
  HIGH_Z      = 32
  ALT         = 64

  # Using global variables as a substitute for class variables
  $mock_gpio_dir ||= {}
  $mock_gpio_pull ||= {}
  $mock_gpio_function ||= {}
  $mock_gpio_open_drain ||= {}

  def _init(pin)
    # no-op
  end

  def self.set_dir_at(pin, dir)
    $mock_gpio_dir[pin] = dir
  end

  def self.pull_up_at(pin)
    $mock_gpio_pull[pin] = :up
  end

  def self.pull_down_at(pin)
    $mock_gpio_pull[pin] = :down
  end

  def self.set_function_at(pin, function)
    $mock_gpio_function[pin] = function
  end

  def self.open_drain_at(pin)
    $mock_gpio_open_drain[pin] = true
  end

  # Helper methods for testing
  def self.mock_get_dir(pin)
    $mock_gpio_dir[pin]
  end

  def self.mock_get_pull(pin)
    $mock_gpio_pull[pin]
  end

  def self.mock_get_function(pin)
    $mock_gpio_function[pin]
  end

  def self.mock_is_open_drain(pin)
    $mock_gpio_open_drain[pin]
  end

  def self.mock_reset
    $mock_gpio_dir = {}
    $mock_gpio_pull = {}
    $mock_gpio_function = {}
    $mock_gpio_open_drain = {}
  end
end
