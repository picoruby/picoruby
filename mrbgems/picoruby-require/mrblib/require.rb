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
    File.file?(path) and return load_file(path)
    raise LoadError, "cannot load such file -- #{path}"
  end

  # private

  def required?(name)
    $LOADED_FEATURES.include?(name)
  end

  def load_file(path)
    sandbox = Sandbox.new('require')
    begin
      sandbox.load_file(path)
    rescue => e
      sandbox.terminate
      raise e
    end
    $LOADED_FEATURES << path unless required?(path)
    true
  end

  def require_file(name)
    load_paths = if name.start_with?("/")
                   [""]
                 else
                   $LOAD_PATH || []
                 end
    load_paths.each do |load_path|
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

if RUBY_ENGINE == 'mruby/c'
  class Object
    include Kernel
  end
  $LOADED_FEATURES = ["require"]
  require "sandbox"
end
