class LoadError < StandardError; end

$LOADED_FEATURES = ["require"]

class Object

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
    unless File.exist?(path)
      raise LoadError, "cannot load such file -- #{path}"
    end
    load_file(path)
  end

  # private

  def required?(name)
    $LOADED_FEATURES.include?(name)
  end

  def load_file(path)
    sandbox = Sandbox.new
    sandbox.load_file(path)
    $LOADED_FEATURES << path unless required?(path)
    true
  end

  def require_file(name)
    $LOAD_PATH&.each do |load_path|
      ["mrb", "rb"].each do |ext|
        path = File.expand_path("#{name}.#{ext}", load_path)
        if File.exist?(path)
          return (required?(path) ? false : load_file(path))
        end
      end
    end
    raise LoadError, "cannot load such file -- #{name}"
  end

end

require "sandbox"
require "filesystem-fat"
require "vfs"

