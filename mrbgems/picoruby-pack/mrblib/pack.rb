class String
  def self.pack(format, *args)
    result = ""
    format_index = 0
    args_index = 0

    while format_index < format.size
      f = format[format_index]
      arg = args[args_index]

      case f
      when 'C'
        result << arg.chr
      when 'N'
        result << (arg >> 24).chr
        result << ((arg >> 16) & 0xFF).chr
        result << ((arg >> 8) & 0xFF).chr
        result << (arg & 0xFF).chr
      else
        raise ArgumentError, "Unsupported format character: #{f}"
      end
      
      format_index += 1
      args_index += 1
    end

    result
  end
end

class Array
  def pack(format)
    String.pack(format, *self)
  end
end


