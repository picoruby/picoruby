module Kernel
  def system(command)
    $shell_sandbox_pool ||= Shell::SandboxPool.new(size: 2)
    job = Shell::Job.new(*command.split, pool: $shell_sandbox_pool)
    result = job.exec
    job.release_sandbox
    result
  end
end
