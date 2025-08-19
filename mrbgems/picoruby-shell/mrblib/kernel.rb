module Kernel
  def system(command)
    Shell::Job.new.exec(*command.split)
  end
end

class Object
  include Kernel
end
