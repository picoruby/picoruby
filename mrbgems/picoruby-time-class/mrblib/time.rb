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
end
