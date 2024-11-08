require 'env'

class Time
  class TimeMethods
  end

  def sunday?    = wday == 0
  def monday?    = wday == 1
  def tuesday?   = wday == 2
  def wednesday? = wday == 3
  def thursday?  = wday == 4
  def friday?    = wday == 5
  def saturday?  = wday == 6

  def -(other)
    case other
    when Integer, Float
      Time.at((self.to_i - other.to_i).to_i)
    when Time
      (self.to_i - other.to_i).to_f
    else
      raise TypeError.new("can't convert into an exact number")
    end
  end

  def +(sec)
    case sec
    when Integer, Float
      Time.at(self.to_i + sec.to_i)
    else
      raise TypeError.new("can't convert into an exact number")
    end
  end
end
