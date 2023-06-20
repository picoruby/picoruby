require "sandbox"

class Task

  TASKS = {}

  def self.list
    TASKS.values
  end

  def initialize(*args, &block)
    @proc = block
    @args = args
    @name = nil
    id = self.to_s
    Task::TASKS[id] = self
    @sandbox = Sandbox.new
    if @sandbox.compile "Task::TASKS['#{id}']._start"
      @sandbox.execute
    else
      puts "Compilation failed"
    end
  end

  attr_reader :name

  def name=(name)
    name.is_a? String or raise TypeError
    @name = name
  end

  def suspend
    @sandbox.suspend
    self
  end

  def terminate
    @sandbox.interrupt
    self
  end

  def join(limit = nil)
    @sandbox.wait(timeout: limit) ? self : nil
  end

  # private

  def _start
    @proc.call(*@args)
  end

end
