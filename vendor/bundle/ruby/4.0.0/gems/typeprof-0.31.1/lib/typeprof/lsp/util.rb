module TypeProf::LSP
  def self.load_json_with_comments(path, **opts)
    json = File.read(path)

    state = :normal
    last_comma_index = nil
    trailing_commas = []
    json = json.gsub(%r(\\.|//|/\*|\*/|[",\n/}\]*]|(\s+)|[^\s\\"*/,]+)) do
      case $&
      when "//"
        state = :single_line_comment if state == :normal
      when "\n"
        state = :normal if state == :single_line_comment
        next "\n"
      when "/*"
        state = :multi_line_comment if state == :normal
      when "*/"
        state = :normal if state == :multi_line_comment
        next "  " if state == :normal
      when "\""
        case state
        when :normal
          last_comma_index = nil
          state = :string_literal
        when :string_literal
          state = :normal
        end
      when ","
        last_comma_index = $~.begin(0) if state == :normal
      when "}", "]"
        if state == :normal && last_comma_index
          trailing_commas << last_comma_index
          last_comma_index = nil
        end
      when $1
      else
        last_comma_index = nil if state == :normal
      end
      if state == :normal || state == :string_literal
        $&
      else
        " " * $&.size
      end
    end
    trailing_commas.each do |i|
      json[i] = " "
    end

    JSON.parse(json, **opts)
  end
end
