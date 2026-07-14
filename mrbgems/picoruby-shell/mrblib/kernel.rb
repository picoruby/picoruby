module Kernel
  def system(command) # steep:ignore MethodArityMismatch
    # @type var command: String
    job = Shell::Job.new(*command.split)
    begin
      job.exec
    ensure
      job.close
    end
  end
end
