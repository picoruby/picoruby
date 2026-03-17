class LoadError < StandardError; end

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

  def load(path)
    result = extern(path, true)
    if result != nil
      $LOADED_FEATURES << path
      return !!result
    end
    load_file(path)
  end

  # private

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
      raise LoadError, "cannot load such file -- #{name_with_ext}"
    else
      sandbox = Sandbox.new('require')
      begin
        sandbox.load_file(path)
        err = sandbox.error and raise err
        $LOADED_FEATURES << name_with_ext unless required?(name_with_ext)
      ensure
        sandbox.terminate
      end
      true
    end
  end

  def require_file(name)
    lps = load_paths(name)
    lpi = 0
    while lpi < lps.size
      exts = ["mrb", "rb"]
      ei = 0
      while ei < exts.size
        path = File.expand_path("#{name}.#{exts[ei]}", lps[lpi])
        if File.file?(path)
          return (required?(path) ? false : load_file(path))
        end
        ei += 1
      end
      lpi += 1
    end
    raise LoadError, "cannot load such file -- #{name}"
  end

end

$LOADED_FEATURES = ["require"]

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
