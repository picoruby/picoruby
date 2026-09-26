# Regexp and MatchData are implemented in C (src/mruby/regexp.c and
# src/mrubyc/regexp.c). Only the option constants live here.

class Regexp
  IGNORECASE = 1
  EXTENDED   = 2
  MULTILINE  = 4
end
