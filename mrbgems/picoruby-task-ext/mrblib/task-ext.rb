require "sandbox"

class Task
  TASKS = {}

  def self.[](name)
    TASKS[name]
  end

  def self.current
    # TODO
  end

  def self.remove(name)
    # TODO removing API in mruby/c's rrt0
    TASKS.delete name
  end

  def initialize(name)
    if TASKS[name]
      raise RuntimeError.new("task :#{name} already exists")
    else
      @sandbox = Sandbox.new
      TASKS[name] = self
    end
  end

  def load_mrb(mrb)
    # TODO
  end

  def compile_and_run(script, wrap_rescue = true) # TODO kwarg `wrap_rescue: false`
    if wrap_rescue
      script =
        "begin\n" +
        script +
        "\nrescue => e\nputs e.class, e.message, 'Task stopped!'\nend"
    end
    if @sandbox.compile(script)
      if @sandbox.execute
      else
        puts "Execution failed"
      end
    else
      puts "Compilation failed"
    end
    self
  end

  def suspend
    @sandbox.suspend
    self
  end

end
