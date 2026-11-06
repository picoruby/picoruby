class LoadError < StandardError
  if RUBY_ENGINE == "mruby"
    # The feature that could not be loaded, as given to require or load.
    # mruby/c exceptions carry no instance variables.
    attr_accessor :path
  end
end

module Kernel

  def require(name)
    return false if required?(name)
    result = extern(name)
    if result != nil
      $LOADED_FEATURES << name
      return !!result
    end
    require_file(name)
  end

  def load(path) # steep:ignore MethodArityMismatch
    result = extern(path, true)
    if result != nil
      $LOADED_FEATURES << path
      return !!result
    end
    load_file(path)
  end

  # private

  def load_error(name)
    e = LoadError.new("cannot load such file -- #{name}")
    e.path = name if e.respond_to?(:path=)
    e
  end

  def required?(name)
    $LOADED_FEATURES.include?(name)
  end

  def load_paths(name)
    if name.start_with?("/")
      [""] # Absolute paths are not relative to any load path
    else
      $LOAD_PATH || []
    end
  end

  def load_file(name_with_ext)
    path = ""
    lps = load_paths(name_with_ext)
    lpi = 0
    while lpi < lps.size
      path = File.expand_path(name_with_ext, lps[lpi])
      File.file?(path) ? break : path = ""
      lpi += 1
    end
    if path.empty?
      raise load_error(name_with_ext)
    else
      sandbox = Sandbox.new('require')
      begin
        if sandbox.respond_to?(:run_inline)
          # mruby VM: run the file on this task. The sandbox task cannot be
          # waited on from a frame the VM entered through C (const_missing,
          # inherited, a sort block, ...): the scheduler never switches to
          # it there, and wait would spin forever. See Sandbox#run_inline.
          sandbox.load_file(path, inline: true)
        else
          sandbox.load_file(path)
          err = sandbox.error and raise err
        end
        $LOADED_FEATURES << name_with_ext unless required?(name_with_ext)
      ensure
        # close, not terminate: terminate leaves the task in the scheduler
        # and registered as a GC root, so every require leaks one task.
        sandbox.close
      end
      true
    end
  end

  def require_file(name)
    lps = load_paths(name)
    lpi = 0
    exts = if name.end_with?(".rb") || name.end_with?(".mrb")
             [""]
           else
             [".mrb", ".rb"]
           end
    while lpi < lps.size
      ei = 0
      while ei < exts.size
        path = File.expand_path("#{name}#{exts[ei]}", lps[lpi])
        if File.file?(path)
          return (required?(path) ? false : load_file(path))
        end
        ei += 1
      end
      lpi += 1
    end
    raise load_error(name)
  end

end

$LOADED_FEATURES = ["require"]
# The Array exists from here on; code that sets the load path changes
# it in place (`clear`, `<<`), as `$LOAD_PATH` is read-only in CRuby.
$LOAD_PATH = []

if RUBY_ENGINE == 'mruby/c'
  class Object
    include Kernel
  end
  begin
    require "posix-io"
  rescue LoadError
    # maybe non-posix
  end
  require "sandbox"
end
