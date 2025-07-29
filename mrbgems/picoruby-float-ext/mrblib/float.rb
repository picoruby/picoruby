class Float
  def %(right)
    left = self.to_f
    while left > right
      left -= right
    end
    left
  end

  def ceil_to_i
    n = self.to_i
    (self > n) ? (n + 1) : n
  end

  def round
    if 0 <= self
      (self + 0.5).to_i
    else
      (self - 0.5).to_i
    end
  end

end
