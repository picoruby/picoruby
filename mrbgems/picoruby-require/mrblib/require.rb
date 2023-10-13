class LoadError < StandardError; end

$REQUIRED_FILES = []

class Object

  def require(name)
    result = PicoGem.require(name)
    return result if result != nil
    return false if required?(name)
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
    $REQUIRED_FILES << path
    true
  end

  def require_file(name)
    ENV['LOAD_PATH']&.split(":").each do |load_path|
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
    $REQUIRED_FILES.include?(name)
  end
end

PicoGem.require "sandbox"
PicoGem.require "filesystem-fat"
PicoGem.require "vfs"

