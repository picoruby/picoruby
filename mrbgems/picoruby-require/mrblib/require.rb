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
    sandbox = Sandbox.new('require')
    load_paths(name_with_ext).each do |load_path|
      path = File.expand_path(name_with_ext, load_path)
      if File.file?(path)
        begin
          sandbox.load_file(path)
          $LOADED_FEATURES << name_with_ext unless required?(name_with_ext)
          return true
        rescue => e
          sandbox.terminate
          raise e
        end
      end
    end
    raise LoadError, "cannot load such file -- #{name_with_ext}"
  end

  def require_file(name)
    load_paths(name).each do |load_path|
      ["mrb", "rb"].each do |ext|
        path = File.expand_path("#{name}.#{ext}", load_path)
        if File.file?(path)
          return (required?(path) ? false : load_file(path))
        end
      end
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
