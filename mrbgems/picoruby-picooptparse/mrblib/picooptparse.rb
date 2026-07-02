# PicoOptionParser is a minimal, declarative command-line option parser for
# PicoRuby. CRuby's optparse is too featureful for microcontrollers, so this
# trades flexibility for a small footprint and low allocation.
#
# Options are declared up front; +parse+ scans an argv Array and returns either
# the +:help+ symbol (when a help flag is seen) or a Result wrapping the parsed
# values.
#
#   parser = PicoOptionParser.new
#   parser.flag("--help", "-h", help: true, desc: "show help")
#   parser.flag("--no-color", default: true, desc: "disable color")
#   parser.on("--mode", type: :symbol, choices: [:fast, :slow], default: :fast)
#   parser.on("--retries", type: :integer, default: 3)
#
#   result = parser.parse(ARGV)
#   if result == :help
#     puts parser.usage("myprog")
#   else
#     result[:mode]    #=> :fast
#     result[:retries] #=> 3
#   end
#
# Supported syntax: "--opt value", "--opt=value", and short aliases "-x".
# Not supported: short-option bundling ("-abc") or attached short values
# ("-xval"). +parse+ raises ArgumentError on unknown options, missing values,
# bad integers, and invalid choices.
class PicoOptionParser
  HELP = :help

  # Declarative spec for a single option. Plain data object; no metaprogramming.
  class Option
    attr_reader :key, :names, :type, :choices, :default, :desc, :help, :negate

    def initialize(key, names, type, choices, default, desc, help, negate)
      @key = key
      @names = names
      @type = type
      @choices = choices
      @default = default
      @desc = desc
      @help = help
      @negate = negate
    end
  end

  # Parsed values, accessed by their derived key symbols.
  class Result
    def initialize(values)
      @values = values
    end

    def [](key)
      @values[key]
    end

    def fetch(key)
      @values.fetch(key)
    end

    def to_h
      @values
    end
  end

  def initialize
    @options = [] #: Array[Option]
    @by_name = {} #: Hash[String, Option]
  end

  # Declare a value-taking option.
  def on(*names, type: :string, choices: nil, default: nil, desc: nil, key: nil)
    register(names, type, choices, default, desc, false, key)
  end

  # Declare a boolean flag. A name beginning with "--no-" negates (sets the key
  # to false). Pass help: true to make +parse+ short-circuit to :help.
  def flag(*names, default: false, desc: nil, help: false, key: nil)
    register(names, :boolean, nil, default, desc, help, key)
  end

  # Parse an argv Array. Returns :help if a help flag is seen, otherwise a
  # Result. Raises ArgumentError on malformed input.
  def parse(args)
    values = {} #: Hash[Symbol, untyped]
    i = 0
    osize = @options.size
    while i < osize
      opt = @options[i]
      values[opt.key] = opt.default
      i += 1
    end

    i = 0
    asize = args.size
    while i < asize
      arg = args[i]
      name = arg
      inline = nil #: String?
      if arg.start_with?("--")
        eq = arg.index("=")
        if eq
          name = arg[0, eq].to_s
          inline = arg[(eq + 1), arg.size - eq - 1]
        end
      end

      opt = @by_name[name]
      raise ArgumentError, "unknown option: #{name}" if opt.nil?
      return HELP if opt.help

      if opt.type == :boolean
        raise ArgumentError, "#{name} takes no value" unless inline.nil?
        values[opt.key] = opt.negate ? false : true
      else
        if inline.nil?
          value = args[i + 1]
          raise ArgumentError, "#{name} requires a value" if value.nil?
          i += 1
        else
          value = inline
        end
        values[opt.key] = coerce(opt, name, value)
      end
      i += 1
    end

    Result.new(values)
  end

  # Build a multi-line usage String. Caller decides whether to print it.
  def usage(program)
    lines = ["Usage: #{program} [options]"]
    i = 0
    osize = @options.size
    while i < osize
      lines << "  #{format_option(@options[i])}"
      i += 1
    end
    lines.join("\n")
  end

  private def register(names, type, choices, default, desc, help, key)
    long = nil #: String?
    i = 0
    nsize = names.size
    while i < nsize
      n = names[i]
      long = n if long.nil? && n.start_with?("--")
      i += 1
    end
    negate = !long.nil? && long.start_with?("--no-")
    base = long || names[0]
    base = base[5, base.size - 5].to_s if negate # strip "--no-"
    derived = key || normalize_key(base)
    default = false if type == :boolean && default.nil? && !negate

    opt = Option.new(derived, names, type, choices, default, desc, help, negate)
    @options << opt
    j = 0
    while j < nsize
      @by_name[names[j]] = opt
      j += 1
    end
    nil
  end

  private def normalize_key(name)
    s = name
    s = s[2, s.size - 2].to_s if s.start_with?("--")
    s = s[1, s.size - 1].to_s if s.start_with?("-")
    s.tr("-", "_").to_sym
  end

  private def coerce(opt, name, value)
    case opt.type
    when :integer
      number = value.to_i
      if number.to_s != value && value != "0"
        raise ArgumentError, "#{name} requires an Integer"
      end
      number
    when :symbol
      validate_choice(opt, name, value, value.to_sym)
    else # :string
      validate_choice(opt, name, value, value)
    end
  end

  private def validate_choice(opt, name, raw, coerced)
    choices = opt.choices
    return coerced if choices.nil?
    i = 0
    csize = choices.size
    while i < csize
      return coerced if choices[i] == coerced
      i += 1
    end
    raise ArgumentError, "invalid #{name}: #{raw}"
  end

  private def format_option(opt)
    names = opt.names.join(", ")
    meta = metavar(opt)
    body = meta.empty? ? names : "#{names} #{meta}"
    body = "#{body}  #{opt.desc}" unless opt.desc.nil?
    body = "#{body} (default: #{opt.default})" unless opt.default.nil?
    body
  end

  private def metavar(opt)
    return "" if opt.type == :boolean
    return opt.choices.join("|") unless opt.choices.nil?
    opt.type == :integer ? "N" : "VALUE"
  end
end
