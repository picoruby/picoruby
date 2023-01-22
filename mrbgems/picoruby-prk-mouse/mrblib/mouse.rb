class Mouse
  def initialize(params)
    @driver = params[:driver]
  end

  attr_accessor :task_proc
  attr_reader :driver

  def task(&block)
    @task_proc = block
  end
end
