module Kernel
  def system(command, *args, **opts)
    # @type var command: String
    Shell::Job.new(*command.split).exec
  end
end
