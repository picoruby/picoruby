# PicoLine
#
# HighLine-like interface for PicoRuby
# Author: Hitoshi HASUMI
# License: The MIT License
# Acknowledgement: https://github.com/JEG2/highline
#
# Usage:
#  require "picoline"
#
#  cli = PicoLine.new
#
#  name = cli.ask("What's you name?")
#  puts "You are #{name}"
#  #=> What's you name?
#  #<= pekepek
#  #=> You are pekepek
#
#  answer = cli.ask("Question?  ") do |q|
#    q.default = "default answer"
#  end
#  puts "You said '#{answer}'"
#  #=> Question? [default answer]
#  #<=
#  #=> You said 'default answer'

require "editor"

class PicoLine
  class Question
    attr_accessor :default, :default_hint_show

    def initialize
      @default = ""
      @default_hint_show = true
    end
  end

  def initialize
    @answer = ""
  end

  def ask(prompt, allow_empty: false)
    q = Question.new
    yield q if block_given?
    if q.default_hint_show && !q.default.to_s.empty?
      prompt += " [#{q.default}]"
    end
    prompt += " "
    editor = Editor::Line.new
    editor.prompt = prompt
    while true
      editor.clear_buffer
      answer = editor.start do |_editor, buffer, c|
        if c == 10 || c == 13
          puts
          break buffer.lines[0].chomp
        end
      end
      answer = q.default.to_s if answer.to_s.empty?
      break if !answer.to_s.empty? || allow_empty
    end
    return(@answer = answer)
  end
end

