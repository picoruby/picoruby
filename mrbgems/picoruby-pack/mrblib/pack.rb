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
      when 'n'
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

  def unpack(format)
    Array.unpack(format, self)
  end
end

class Array
  def self.unpack(format, packed_string)
    result = []
    str_index = 0
    format_index = 0
    while format_index < format.size
      f = format[format_index]
      case f
      when 'C'
        s = packed_string[str_index, 1]
        if s.is_a? String
          result << s.ord
          str_index += 1
        else
          raise ArgumentError, "not enough data for format 'C'"
        end
      when 'N'
        s = packed_string[str_index, 4] || ''
        s1, s2, s3, s4 = s[0], s[1], s[2], s[3]
        if s1.is_a?(String) && s2.is_a?(String) && s3.is_a?(String) && s4.is_a?(String) 
          val = (s1.ord << 24) | (s2.ord << 16) | (s3.ord << 8) | (s4.ord)
          result << val
          str_index += 4
        else
          raise ArgumentError, "not enough data for format 'N'"
        end
      when 'n'
        s = packed_string[str_index, 2] || ''
        s1, s2 = s[0], s[1]
        if s1.is_a?(String) && s2.is_a?(String)
          val = (s1.ord << 8) | (s2.ord)
          result << val
          str_index += 2
        else
          raise ArgumentError, "not enough data for format 'n'"
        end
      else
        raise ArgumentError, "Unsupported format character: #{f}"
      end
      format_index += 1
    end
    result
  end

  def pack(format)
    String.pack(format, *self)
  end
end
