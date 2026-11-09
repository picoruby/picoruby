class LoadError < StandardError
  if RUBY_ENGINE == "mruby"
    # The feature that could not be loaded, as given to require or load.
    # mruby/c exceptions carry no instance variables.
    attr_accessor :path
  end
end

module Kernel

  def require(name)
    return false if __required?(name)
    result = extern(name)
    if result != nil
      $LOADED_FEATURES << name
      return !!result
    end
    __require_file(name)
  end

  def load(path) # steep:ignore MethodArityMismatch
    result = extern(path, true)
    if result != nil
      $LOADED_FEATURES << path
      return !!result
    end
    __require_load_file(path, true)
  end

  # The helpers of require and load. They are called on the receiver of
  # require, which is any object, so a name that a class may well define
  # for itself (load_paths, load_file, required?) would be taken from it:
  # load in a class with its own load_paths called that one.

  def __require_load_error(name)
    e = LoadError.new("cannot load such file -- #{name}")
    e.path = name if e.respond_to?(:path=)
    e
  end

  def __required?(name)
    $LOADED_FEATURES.include?(name)
  end

  # Where to look for name: nowhere but itself when absolute, the working
  # directory for ./ and ../, else the load path. load (cwd: true) goes on
  # to the working directory after the load path, as CRuby's does.
  def __require_load_paths(name, cwd = false)
    if name.start_with?("/")
      [""] # Absolute paths are not relative to any load path
    elsif name.start_with?("./") || name.start_with?("../")
      [File.expand_path(".")]
    elsif cwd
      ($LOAD_PATH || []) + [File.expand_path(".")]
    else
      $LOAD_PATH || []
    end
  end

  def __require_load_file(name_with_ext, cwd = false)
    path = ""
    lps = __require_load_paths(name_with_ext, cwd)
    lpi = 0
    while lpi < lps.size
      path = File.expand_path(name_with_ext, lps[lpi])
      File.file?(path) ? break : path = ""
      lpi += 1
    end
    if path.empty?
      raise __require_load_error(name_with_ext)
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
        $LOADED_FEATURES << name_with_ext unless __required?(name_with_ext)
      ensure
        # close, not terminate: terminate leaves the task in the scheduler
        # and registered as a GC root, so every require leaks one task.
        sandbox.close
      end
      true
    end
  end

  def __require_file(name)
    lps = __require_load_paths(name)
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
          return (__required?(path) ? false : __require_load_file(path))
        end
        ei += 1
      end
      lpi += 1
    end
    raise __require_load_error(name)
  end

end

$LOADED_FEATURES = ["require"]
# The Array exists from here on; code that sets the load path changes
# it in place (`clear`, `<<`), as `$LOAD_PATH` is read-only in CRuby.
$LOAD_PATH = []
# $: and $" are the same Arrays under their short names
$: = $LOAD_PATH
$" = $LOADED_FEATURES

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
