# wc - count lines, words, and characters
#
# Usage:
#   wc          # count from stdin
#   wc -l       # lines only
#   wc -w       # words only
#   wc -c       # chars only
# In a pipeline, characters are read via `getc` to exercise the
# PipeIN#getc refinement; lines/words are accumulated from the same
# stream so a single pass produces all three counts.

count_lines = false
count_words = false
count_chars = false

Shell::ARGV.each do |arg|
  if arg.start_with?("-")
    arg[1, 255]&.each_char do |ch|
      case ch
      when 'l' then count_lines = true
      when 'w' then count_words = true
      when 'c' then count_chars = true
      end
    end
  end
end

# No flag => report all three (lines, words, chars), like coreutils wc.
unless count_lines || count_words || count_chars
  count_lines = true
  count_words = true
  count_chars = true
end

lines = 0
words = 0
chars = 0
in_word = false

while c = getc
  chars += 1
  if c == "\n"
    lines += 1
    in_word = false
  elsif c == " " || c == "\t"
    in_word = false
  else
    unless in_word
      words += 1
      in_word = true
    end
  end
end

out = [] #: Array[String]
out << lines.to_s if count_lines
out << words.to_s if count_words
out << chars.to_s if count_chars
puts out.join(" ")
