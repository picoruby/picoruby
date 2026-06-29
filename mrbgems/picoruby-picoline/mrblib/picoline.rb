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

  def initialize(editor: nil)
    @answer = ""
    @editor = editor
    @reading = false
    @messages = [] #: Array[String]
    # Editor::Line must render notifications in its own task to serialize
    # cursor-position queries with terminal input.
    @idle_handler = -> { flush_messages }
  end

  # Reads one command line while allowing other PicoRuby tasks to run.
  # Editor::Line owns terminal echo, cursor movement, history, and deletion.
  def readline(prompt)
    editor = line_editor
    editor.prompt = prompt
    editor.clear_buffer
    editor.idle_handler = @idle_handler
    answer = nil #: String?
    @reading = true
    # Editor::Line requires a callback to return control characters.
    editor.start do |current_editor, buffer, c|
      if c == 4 # Ctrl-D
        if buffer.empty?
          puts
          break
        else
          puts
          answer = buffer.dump.chomp
          current_editor.save_history
          break
        end
      elsif c == 10 || c == 13
        puts
        current_answer = buffer.dump.chomp
        answer = current_answer
        current_editor.save_history unless current_answer.empty?
        break
      end
    end
    answer
  ensure
    @reading = false
    @editor&.idle_handler = nil
  end

  # Prints asynchronous output without corrupting an active input line.
  def say(message)
    if @reading
      @messages << message
    else
      puts message
    end
    self
  end

  def ask(prompt, allow_empty: false)
    q = Question.new
    yield q if block_given?
    if q.default_hint_show && !q.default.to_s.empty?
      prompt += " [#{q.default}]"
    end
    prompt += " "
    editor = line_editor
    editor.prompt = prompt
    editor.idle_handler = @idle_handler
    @reading = true
    while true
      editor.clear_buffer
      answer = nil
      editor.start do |_editor, buffer, c|
        if c == 10 || c == 13
          puts
          answer = buffer.lines[0].chomp
          break
        end
      end
      answer = q.default.to_s if answer.to_s.empty?
      break if !answer.to_s.empty? || allow_empty
    end
    return(@answer = answer or raise)
  ensure
    @reading = false
    @editor&.idle_handler = nil
  end

  private def flush_messages
    messages = @messages
    return if messages.empty?
    editor = @editor
    return unless editor
    editor.feed_at_bottom
    while 0 < messages.size
      puts messages.shift
    end
    editor.refresh
  end

  private def line_editor
    @editor ||= Editor::Line.new
  end
end
