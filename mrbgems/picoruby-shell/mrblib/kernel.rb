module Kernel
  def system(command) # steep:ignore MethodArityMismatch
    # @type var command: String
    Shell::Job.new(*command.split).exec
  end
end
