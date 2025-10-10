# Kernel#require will be overridden by picoruby-require.
# But this implementation is necessary not to raise LoadError during picogem_init
module Kernel
  def require(path)
    false
  end
end
