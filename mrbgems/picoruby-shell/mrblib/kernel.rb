module Kernel
  def system(command)
    Shell::Job.new(*command.split).exec
  end
end

class Object
  include Kernel
end
