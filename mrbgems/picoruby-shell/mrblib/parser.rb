require 'data'

class Shell
  class Tokenizer
    SPECIAL_CHARS = ['|', '&', ';', '<', '>', ' ', "\t", "\n", "'", '"']

    def initialize(input)
      @input = input
      @position = 0
    end

    def next_token
      skip_whitespace
      return nil if eof?

      start_pos = @position
      char = @input[@position]
      case char
      when '|', '&', ';'
        create_token(:special, start_pos, 1)
      when '<', '>'
        if @position + 1 < @input.length && @input[@position + 1] == char
          create_token(:special, start_pos, 2)
        else
          create_token(:special, start_pos, 1)
        end
      when '"', "'"
        tokenize_quoted_string(char, start_pos)
      when ' ', "\t", "\n"
        tokenize_whitespace(start_pos)
      else
        tokenize_word(start_pos)
      end
    end

    private

    def tokenize_whitespace(start_pos)
      while !eof? && [' ', "\t", "\n"].include?(@input[@position] || '')
        @position += 1
      end
      create_token(:special, start_pos, @position - start_pos)
    end

    def skip_whitespace
      while !eof? && [' ', "\t", "\n"].include?(@input[@position] || '')
        @position += 1
      end
    end

    def eof?
      @input.length <= @position
    end

    def create_token(type, start_pos, length)
      token = { type: type, pos: start_pos, length: length }
      @position = start_pos + length
      token
    end

    def tokenize_quoted_string(quote_char, start_pos)
      @position += 1
      while @position < @input.length && @input[@position] != quote_char
        @position += 1
      end
      @position += 1 if @position < @input.length  # include closing quote
      create_token(:quoted_string, start_pos, @position - start_pos)
    end

    def tokenize_word(start_pos)
      while @position < @input.length && !SPECIAL_CHARS.include?(@input[@position] || '')
        @position += 1
      end
      create_token(:word, start_pos, @position - start_pos)
    end
  end

  class Parser
    Node = Data.define(:type, :data, :token)

    def initialize(input)
      @tokenizer = Tokenizer.new(input)
      @current_token = nil
      @input = input
    end

    def parse
      advance
      return nil unless @current_token
      parse_program
    end

    private

    def parse_program
      commands = []
      current = @current_token
      return nil unless current
      start_pos = current[:pos]
      while @current_token
        commands << parse_command
        break unless consume(:special, '|')
      end

      current = @current_token
      end_pos = current ? current[:pos] : @input.length
      token = @input[start_pos...end_pos] || ""

      if commands.length > 1
        Node.new(:pipeline, { commands: commands }, token)
      else
        commands.first
      end
    end

    def parse_command
      name = expect_word_or_quoted_string()
      args = []
      redirects = []
      start_pos = name[:pos]

      while @current_token
        current = @current_token
        break unless current
        token_type = current[:type]
        case token_type
        when :word, :quoted_string
          args << parse_argument
        when :special
          if ['>', '<', '>>', '<<'].include?(token_value)
            redir = parse_redirection
            redirects << redir if redir
          else
            break
          end
        else
          break
        end
      end

      current = @current_token
      end_pos = current ? current[:pos] : @input.length
      token = @input[start_pos...end_pos] || ""

      name_value = token_value(name)
      Node.new(:command, { name: name_value, args: args, redirects: redirects }, token)
    end

    def parse_argument
      token = expect_word_or_quoted_string()
      token_value(token)
    end

    def parse_redirection
      current = @current_token
      return nil unless current
      start_pos = current[:pos]
      type_token = expect(:special)
      type = case token_value(type_token)
             when '>' then :output
             when '>>' then :append
             when '<' then :input
             when '<<' then :here_document
             else raise("Unexpected redirection type: #{token_value(type_token)}")
             end

      skip_whitespace
      target = expect_word_or_quoted_string()
      current = @current_token
      end_pos = current ? current[:pos] : @input.length

      full_token = (@input[start_pos...end_pos] || "").strip
      Node.new(:redirection, { type: type, target: token_value(target) }, full_token)
    end

    def skip_whitespace
      while @current_token
        current = @current_token
        break unless current
        token_type = current[:type]
        break unless token_type == :special && token_value.strip.empty?
        advance
      end
    end

    def advance
      @current_token = @tokenizer.next_token
    end

    def consume(type, value = nil)
      current = @current_token
      return nil unless current
      token_type = current[:type]
      return nil unless token_type == type
      return nil unless value.nil? || token_value == value
      token = current
      advance
      token
    end

    def expect(type, value = nil)
      token = consume(type, value)
      raise "Expected #{type} #{value}, got #{@current_token}" unless token
      token
    end

    def expect_word_or_quoted_string
      token = consume(:word) || consume(:quoted_string)
      raise "Expected word or quoted string, got #{@current_token}" unless token
      token
    end

    def token_value(token = @current_token)
      return "" unless token
      value = @input[token[:pos], token[:length]] || ""
      if token[:type] == :quoted_string && value.length >= 2
        # Remove surrounding quotes
        value[1...-1] || ""
      else
        value
      end
    end
  end
end
