module Kernel
  def system(command)
    # @type var command: String
    Shell::Job.new(*command.split).exec
  end
end
