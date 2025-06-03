module PNGBIT
  MAGIC = "PNGBIT"  # 6 bytes

  def self.show(filename)
    file = File.open(filename)

    magic = file.read(6)
    if magic != MAGIC || file.size < 10
      raise "Invalid file format"
    end

    width_bytes = file.read(2)&.bytes
    height_bytes = file.read(2)&.bytes
    if width_bytes.nil? || height_bytes.nil?
      raise "Invalid file format: width or height not found"
    end
    # Big endian
    width = (width_bytes[0] << 8) | width_bytes[1]
    height = (height_bytes[0] << 8) | height_bytes[1]

    y = 0
    while y < height
      line = ""
      x = 0
      prev_color = nil
      while x < width
        b = file.read(1)&.ord
        break unless b # Corrupted file or EOF (unexpeted though...)
        if b == 0
          line += "\e[C"  # skip transparent pixel
          prev_color = nil
        else
          if b != prev_color
            line += "\e[48;5;" + b.to_s + "m "
            prev_color = b
          else
            line += " "
          end
        end
        x += 1
      end
      print line + "\e[0m"
      print "\e[B\e[#{width}D"  # move to next line
      y += 1
    end

    file.close
  end
end
