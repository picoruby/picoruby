require 'env'

class Time
  class TimeMethods
  end

  # getlocal and localtime take a zone object too: one with utc_to_local,
  # as ActiveSupport::TimeZone and TZInfo::Timezone are. It stands for the
  # offset it has at this time, which the time then keeps; CRuby keeps the
  # object itself, and #zone returns it.
  alias __getlocal getlocal
  alias __localtime localtime

  def getlocal(zone = nil)
    __getlocal(__zone_offset(zone))
  end

  def localtime(zone = nil)
    __localtime(__zone_offset(zone))
  end

  # The zone argument as getlocal takes it: a zone object becomes its offset
  # in seconds. utc_to_local returns the local time either as a Time with
  # that offset or as the wall clock in a UTC Time; this is right for both.
  def __zone_offset(zone)
    return zone if zone.nil? || zone.is_a?(String) || zone.is_a?(Integer) || zone.is_a?(Symbol)
    return zone unless zone.respond_to?(:utc_to_local)
    local = zone.utc_to_local(getutc)
    local.to_i + local.utc_offset - to_i
  end

  # Comparable is not on every VM; between? and clamp are what Time needs
  def between?(min, max)
    min <= self && self <= max
  end

  def clamp(min, max)
    return min if self < min
    return max if max < self
    self
  end

  MONTHNAMES = [nil, "January", "February", "March", "April", "May", "June", "July",
                "August", "September", "October", "November", "December"]
  DAYNAMES = ["Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"]

  def sunday?    = wday == 0
  def monday?    = wday == 1
  def tuesday?   = wday == 2
  def wednesday? = wday == 3
  def thursday?  = wday == 4
  def friday?    = wday == 5
  def saturday?  = wday == 6

  alias day mday
  alias month mon

  def nsec
    usec * 1000
  end

  def subsec
    u = usec
    return 0 if u == 0
    Object.const_defined?(:Rational) ? Rational(u, 1000000) : u / 1000000.0
  end

  def to_a
    [sec, min, hour, mday, mon, year, wday, yday, isdst, zone]
  end

  def succ
    self + 1
  end

  def to_time
    self
  end

  # the time rounded, floored or ceiled to `digits` decimals of a second
  def round(digits = 0)
    __adjust(digits, :round)
  end

  def floor(digits = 0)
    __adjust(digits, :floor)
  end

  def ceil(digits = 0)
    __adjust(digits, :ceil)
  end

  def __adjust(digits, how)
    return self if digits >= 6
    scale = 10 ** (6 - digits)
    us = usec
    rem = us % scale
    return self if rem == 0
    delta = case how
            when :floor then -rem
            when :ceil then scale - rem
            else (rem * 2 >= scale ? scale - rem : -rem)
            end
    self + delta / 1000000.0
  end

  # "2025-03-04T05:06:07+09:00", with `digits` decimals of a second
  def iso8601(digits = 0)
    s = strftime("%Y-%m-%dT%H:%M:%S")
    s << "." << strftime("%#{digits}N") if digits > 0
    s << (utc? ? "Z" : strftime("%:z"))
  end
  alias xmlschema iso8601

  # ---- strftime ----
  # The conversions of CRuby's strftime with the flags - _ 0 ^ # and a
  # width; %z takes one or two colons. Written in Ruby so every target
  # has the same output.
  def strftime(fmt)
    out = ""
    i = 0
    n = fmt.size
    while i < n
      c = fmt[i]
      if c != "%"
        out << c.to_s
        i += 1
        next
      end
      i += 1
      break if i >= n
      pad = nil
      upcase = false
      swapcase = false
      colons = 0
      while i < n && "-_0^#:".include?(fmt[i].to_s)
        case fmt[i]
        when "-" then pad = :none
        when "_" then pad = " "
        when "0" then pad = "0"
        when "^" then upcase = true
        when "#" then swapcase = true
        when ":" then colons += 1
        end
        i += 1
      end
      width = nil
      while i < n && fmt[i].to_s >= "0" && fmt[i].to_s <= "9"
        width = (width || 0) * 10 + fmt[i].to_i
        i += 1
      end
      conv = fmt[i].to_s
      i += 1
      piece = __strftime_conv(conv, pad, width, colons)
      if piece.nil?
        out << "%" << conv
        next
      end
      piece = piece.upcase if upcase
      # "#" turns names upper case and AM/PM and the zone lower case
      piece = (conv == "p" || conv == "Z") ? piece.downcase : piece.upcase if swapcase
      out << piece
    end
    out
  end

  def __strftime_num(value, default_width, default_pad, pad, width)
    pad = default_pad if pad.nil?
    width = default_width if width.nil?
    s = value.abs.to_s
    s = s.rjust(width - (value < 0 ? 1 : 0), pad) unless pad == :none
    value < 0 ? "-" + s : s
  end

  def __strftime_str(value, pad, width)
    return value if width.nil?
    value.rjust(width, pad == "0" ? "0" : " ")
  end

  def __strftime_offset(colons, pad, width)
    off = utc_offset
    sign = off < 0 ? "-" : "+"
    off = off.abs
    h = off / 3600
    m = (off % 3600) / 60
    s = off % 60
    body = case colons
           when 0 then sprintf("%02d%02d", h, m)
           when 1 then sprintf("%02d:%02d", h, m)
           else sprintf("%02d:%02d:%02d", h, m, s)
           end
    return sign + body if width.nil? || width <= body.size + 1
    sign + body.rjust(width - 1, pad == " " ? " " : "0")
  end

  def __strftime_frac(digits)
    s = nsec.to_s.rjust(9, "0")
    digits <= 9 ? s[0, digits].to_s : s + "0" * (digits - 9)
  end

  def __strftime_conv(conv, pad, width, colons)
    case conv
    when "Y" then __strftime_num(year, 4, "0", pad, width)
    when "C" then __strftime_num(year / 100, 2, "0", pad, width)
    when "y" then __strftime_num(year % 100, 2, "0", pad, width)
    when "m" then __strftime_num(mon, 2, "0", pad, width)
    when "B" then __strftime_str(MONTHNAMES[mon].to_s, pad, width)
    when "b", "h" then __strftime_str(MONTHNAMES[mon].to_s[0, 3].to_s, pad, width)
    when "d" then __strftime_num(mday, 2, "0", pad, width)
    when "e" then __strftime_num(mday, 2, " ", pad, width)
    when "j" then __strftime_num(yday, 3, "0", pad, width)
    when "H" then __strftime_num(hour, 2, "0", pad, width)
    when "k" then __strftime_num(hour, 2, " ", pad, width)
    when "I" then __strftime_num((hour % 12) == 0 ? 12 : hour % 12, 2, "0", pad, width)
    when "l" then __strftime_num((hour % 12) == 0 ? 12 : hour % 12, 2, " ", pad, width)
    when "P" then __strftime_str(hour < 12 ? "am" : "pm", pad, width)
    when "p" then __strftime_str(hour < 12 ? "AM" : "PM", pad, width)
    when "M" then __strftime_num(min, 2, "0", pad, width)
    when "S" then __strftime_num(sec, 2, "0", pad, width)
    when "L" then __strftime_frac(width || 3)
    when "N" then __strftime_frac(width || 9)
    when "z" then __strftime_offset(colons, pad, width)
    when "Z" then __strftime_str(zone.to_s, pad, width)
    when "A" then __strftime_str(DAYNAMES[wday].to_s, pad, width)
    when "a" then __strftime_str(DAYNAMES[wday].to_s[0, 3].to_s, pad, width)
    when "u" then __strftime_num(wday == 0 ? 7 : wday, 1, "0", pad, width)
    when "w" then __strftime_num(wday, 1, "0", pad, width)
    when "G" then __strftime_num(__cweek[0], 4, "0", pad, width)
    when "g" then __strftime_num(__cweek[0] % 100, 2, "0", pad, width)
    when "V" then __strftime_num(__cweek[1], 2, "0", pad, width)
    when "U" then __strftime_num((yday - 1 + 7 - wday) / 7, 2, "0", pad, width)
    when "W" then __strftime_num((yday - 1 + 7 - ((wday + 6) % 7)) / 7, 2, "0", pad, width)
    when "s" then __strftime_num(to_i, 1, "0", pad, width)
    when "n" then "\n"
    when "t" then "\t"
    when "%" then "%"
    when "c" then strftime("%a %b %e %H:%M:%S %Y")
    when "D", "x" then strftime("%m/%d/%y")
    when "F" then strftime("%Y-%m-%d")
    when "T", "X" then strftime("%H:%M:%S")
    when "R" then strftime("%H:%M")
    when "r" then strftime("%I:%M:%S %p")
    when "v" then strftime("%e-%^b-%Y")
    when "+" then strftime("%a %b %e %H:%M:%S %Z %Y")
    else nil
    end
  end

  # the ISO 8601 week-based year and week number
  def __cweek
    w = wday == 0 ? 7 : wday
    thursday = yday - w + 4
    y = year
    if thursday < 1
      y -= 1
      thursday += __days_in_year(y)
    elsif thursday > __days_in_year(y)
      thursday -= __days_in_year(y)
      y += 1
    end
    [y, (thursday - 1) / 7 + 1]
  end

  def __days_in_year(y)
    (y % 4 == 0 && y % 100 != 0) || y % 400 == 0 ? 366 : 365
  end
end
