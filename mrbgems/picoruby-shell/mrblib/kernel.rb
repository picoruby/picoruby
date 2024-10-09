module Kernel
  def system(command)
    Shell::Command.new.exec(*command.split)
  end
end

class Object
  include Kernel
end
