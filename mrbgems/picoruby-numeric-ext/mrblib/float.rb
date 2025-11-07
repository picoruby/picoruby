class Float
  def %(right)
    left = self.to_f
    while right < left
      left -= right
    end
    left
  end

  def round(digits = 0)
    factor = 10.0 ** digits
    scaled = self * factor
    int_part = scaled.to_i
    remainder = scaled - int_part
    if 0.5 <= remainder.abs
      int_part += (0 < scaled ? 1 : -1)
    end
    result = int_part / factor
    if digits <= 0
      result.to_i
    else
      result
    end
  end

  def floor(digits = 0)
    digits ||= 0
    factor = 10.0 ** digits
    scaled = self * factor
    int_part = scaled.to_i
    if self < 0 && scaled != int_part
      int_part -= 1
    end
    result = int_part / factor
    if digits <= 0
      result.to_i
    else
      result
    end
  end

  def ceil(digits = 0)
    factor = 10.0 ** digits
    scaled = self * factor
    int_part = scaled.to_i
    if 0 < self && scaled != int_part
      int_part += 1
    end
    result = int_part / factor
    if digits <= 0
      result.to_i
    else
      result
    end
  end

end
