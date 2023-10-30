class LoadError < StandardError; end

$LOADED_FEATURES = []

class Object

  def require(name)
    return false if required?(name)
    result = PicoGem.require(name)
    if result != nil
      # @type var result: bool
      $LOADED_FEATURES << name
      return result
    end
    require_file(name)
  end

  def required?(name)
    required_file?(name) or PicoGem.required?(name)
  end

  def load(path)
    unless File.exist?(path)
      raise LoadError, "cannot load such file -- #{path}"
    end
    load_file(path)
  end

  # private

  def load_file(path)
    sandbox = Sandbox.new
    sandbox.load_file(path)
    $LOADED_FEATURES << path unless required_file?(path)
    true
  end

  def require_file(name)
    ENV['LOAD_PATH']&.split(":")&.each do |load_path|
      ["mrb", "rb"].each do |ext|
        path = File.expand_path("#{name}.#{ext}", load_path)
        if File.exist?(path)
          return (required_file?(path) ? false : load_file(path))
        end
      end
    end
    raise LoadError, "cannot load such file -- #{name}"
  end

  def required_file?(name)
    $LOADED_FEATURES.include?(name)
  end
end

PicoGem.require "sandbox"
PicoGem.require "filesystem-fat"
PicoGem.require "vfs"

