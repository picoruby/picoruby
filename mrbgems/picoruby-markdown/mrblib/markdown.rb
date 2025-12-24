require 'yaml'

class Markdown
  def initialize(text)
    @front_matter = {}
    lines = text.lines
    if lines[0] && lines[0].start_with?("---\n")
      parts = text.split("---\n", 3)
      if parts.length >= 3
        @front_matter = YAML.load(parts[1]) || {}
        lines = parts[2].lines
      end
    end

    @footnotes = {}
    @footnote_order = []
    remaining_lines = []
    lines.each do |line|
      stripped = line.strip
      if stripped.start_with?('[^') && stripped.include?(']:')
        key_end = stripped.index(']:')
        # @type var key_end: Integer
        key = stripped[2..key_end-1] || ''
        description = stripped[key_end+2..-1]&.strip || ''
        unless @footnotes[key]
          @footnote_order << key
        end
        @footnotes[key] = description
      else
        remaining_lines << line
      end
    end
    @lines = remaining_lines
  end

  attr_reader :front_matter, :footnotes

  def to_html
    html = ""
    in_code_block = false
    lines_to_skip = 0

    @lines.each_with_index do |line, index|
      if lines_to_skip > 0
        lines_to_skip -= 1
        next
      end

      # Code block parsing
      if line.start_with?("```")
        if in_code_block
          html << "</code></pre>\n"
          in_code_block = false
        else
          language = line[3..-1].strip
          if language.empty?
            html << "<pre><code>"
          else
            html << "<pre><code class=\"language-#{language}\">"
          end
          in_code_block = true
        end
        next
      end

      if in_code_block
        html << line
        next
      end

      stripped_line = line.strip

      if stripped_line.empty?
        next
      end

      # Table parsing
      if stripped_line.start_with?('|')
        separator_line = @lines[index + 1]&.strip
        if separator_line && separator_line.start_with?('|') && separator_line.include?('-')
          is_separator = separator_line[1..-2].split('|').all? { |s| s.strip.start_with?('-') }
          if is_separator
            header_columns = stripped_line[1..-2].split('|').map(&:strip)

            html << "<table>\n<thead>\n<tr>\n"
            header_columns.each { |h| html << "<th>#{h}</th>" }
            html << "</tr>\n</thead>\n<tbody>\n"

            i = index + 2
            while i < @lines.length && @lines[i].strip.start_with?('|')
              body_line = @lines[i].strip
              body_columns = body_line[1..-2].split('|').map(&:strip)
              html << "<tr>"
              body_columns.each { |b| html << "<td>#{b}</td>" }
              html << "</tr>\n"
              i += 1
            end

            html << "</tbody></table>\n"

            lines_to_skip = (i - 1) - index
            next
          end
        end
      end

      # Heading parsing
      if stripped_line.start_with?('#')
        level = 0
        level += 1 while stripped_line[level] == '#'
        content = stripped_line[level..-1].strip
        html << "<h#{level}>#{content}</h#{level}>\n"
      # Paragraph
      else
        # List parsing
        is_ulist_start = ['* ', '- ', '+ '].any? { |m| stripped_line.start_with?(m) }
        is_olist_start = is_ordered_list_item(stripped_line)
        if is_ulist_start || is_olist_start
          list_html, lines_consumed = process_list(index, is_ulist_start ? :ul : :ol)
          html << list_html
          lines_to_skip = lines_consumed - 1
          next
        end

        processed_line = process_inline_formats(stripped_line)
        html << "<p>#{processed_line}</p>\n"
      end
    end

    if @footnotes.any?
      html << "<div class=\"footnotes\">\n<hr>\n<ol>\n"
      @footnote_order.each_with_index do |key, index|
        html << "<li id=\"fn-#{key}\">#{@footnotes[key]} <a href=\"#fnref-#{key}\" rev=\"footnote\">&#8617;</a></li>\n"
      end
      html << "</ol>\n</div>\n"
    end

    html
  end

  private

  def process_list(start_index, type, base_indent = 0)
    list_html = ""
    tag = type == :ul ? "ul" : "ol"
    list_html << "<#{tag}>\n"

    i = start_index
    lines_consumed = 0

    while i < @lines.length
      line = @lines[i]
      current_indent = count_leading_spaces(line)
      stripped = line.strip

      # If indent is less than base, we've exited this list level
      if current_indent < base_indent && !stripped.empty?
        break
      end

      is_ulist_item = ['* ', '- ', '+ '].any? { |m| stripped.start_with?(m) }
      is_olist_item = is_ordered_list_item(stripped)

      is_current_type_item = (type == :ul && is_ulist_item) || (type == :ol && is_olist_item)

      # Check if this is a nested list (deeper indent)
      if !stripped.empty? && current_indent > base_indent && (is_ulist_item || is_olist_item)
        # This is a nested list, process it recursively
        nested_type = is_ulist_item ? :ul : :ol
        nested_html, nested_consumed = process_list(i, nested_type, current_indent)
        list_html << nested_html
        i += nested_consumed
        lines_consumed += nested_consumed
        next
      end

      if is_current_type_item && current_indent == base_indent
        marker_length = stripped.index('. ') ? stripped.index('. ') + 2 : 2
        content = stripped[marker_length..-1].strip
        processed_content = process_inline_formats(content)
        list_html << "<li>#{processed_content}"

        # Check if the next line is a nested list
        if i + 1 < @lines.length
          next_line = @lines[i + 1]
          next_indent = count_leading_spaces(next_line)
          next_stripped = next_line.strip

          if next_indent > current_indent && !next_stripped.empty?
            next_is_ulist = ['* ', '- ', '+ '].any? { |m| next_stripped.start_with?(m) }
            next_is_olist = is_ordered_list_item(next_stripped)

            if next_is_ulist || next_is_olist
              # Process nested list
              nested_type = next_is_ulist ? :ul : :ol
              nested_html, nested_consumed = process_list(i + 1, nested_type, next_indent)
              list_html << "\n" << nested_html
              i += nested_consumed + 1
              lines_consumed += nested_consumed + 1
              list_html << "</li>\n"
              next
            end
          end
        end

        list_html << "</li>\n"
        i += 1
        lines_consumed += 1
      elsif stripped.empty?
        peek_line = @lines[i+1]&.strip
        if peek_line && (is_ordered_list_item(peek_line) || ['* ', '- ', '+ '].any? { |m| peek_line.start_with?(m) })
          i += 1
          lines_consumed += 1
        else
          break
        end
      else
        break
      end
    end

    list_html << "</#{tag}>\n"
    return list_html, lines_consumed
  end

  def count_leading_spaces(line)
    count = 0
    line.each_char do |char|
      if char == ' '
        count += 1
      elsif char == "\t"
        count += 4  # Treat tab as 4 spaces
      else
        break
      end
    end
    count
  end

  def is_ordered_list_item(line)
    return false if line.empty?
    i = 0
    while i < line.length && "0123456789".include?(line[i] || '')
      i += 1
    end
    i > 0 && line[i..i+1] == '. '
  end

  def process_inline_formats(line)
    line = replace_footnote_references(line)
    line = replace_links_and_images(line)

    # The order of these operations is important for correctness.
    line = gsub_with_pairing(line, '`', '<code>', '</code>', :escape_html)
    line = gsub_with_pairing(line, '**', '<strong>', '</strong>')
    line = gsub_with_pairing(line, '__', '<strong>', '</strong>')
    line = gsub_with_pairing(line, '*', '<em>', '</em>')
    line = gsub_with_pairing(line, '_', '<em>', '</em>')

    line
  end

  def replace_links_and_images(line)
    output = ""
    scanner = line
    while scanner.length > 0
      # Check for image first `![`
      start_pos = scanner.index('![')
      type = :image

      if start_pos.nil?
        # If no image, check for link `[`
        start_pos = scanner.index('[')
        type = :link
      end

      # If no links or images are found, we're done with this line
      break if start_pos.nil?

      # Append the text before the marker
      output << scanner[0...start_pos] # steep:ignore
      scanner = scanner[start_pos..-1]

      # Find the closing bracket of the text/alt part
      end_bracket_pos = scanner.index(']')
      # If there's no closing bracket, it's not a valid link/image
      break unless end_bracket_pos

      text = scanner[(type == :image ? 2 : 1)...end_bracket_pos]

      # Check for the opening parenthesis of the URL part
      if scanner[end_bracket_pos + 1] == '('
        scanner = scanner[end_bracket_pos + 2..-1] # Move past `](`
        end_paren_pos = scanner.index(')')
        # If there's no closing parenthesis, it's not valid
        break unless end_paren_pos

        url_part = scanner[0...end_paren_pos] || ''
        scanner = scanner[end_paren_pos + 1..-1] # Move past `)`

        # Parse URL and optional title
        url, title = url_part.split(' ', 2)
        if title
          # Clean up title from quotes
          if title.start_with?('"') && title.end_with?('"')
            title = title[1...-1]
          end
        end

        title_attr = title ? " title=\"#{title}\"" : ""

        if type == :image
          output << "<img src=\"#{url}\" alt=\"#{text}\"#{title_attr}>"
        else # :link
          output << "<a href=\"#{url}\"#{title_attr}>#{text}</a>"
        end
      else
        # It's not a link/image, just text in brackets, so output as is
        output << scanner[0..end_bracket_pos] # steep:ignore
        scanner = scanner[end_bracket_pos + 1..-1]
      end
    end

    output << scanner # Append any remaining part of the string
    output
  end

  def replace_footnote_references(line)
    processed_line = line
    @footnote_order.each_with_index do |key, index|
      ref_tag = "[^#{key}]"
      sup_tag = "<sup><a href=\"#fn-#{key}\" id=\"fnref-#{key}\" rel=\"footnote\">#{index + 1}</a></sup>"
      processed_line = processed_line.gsub(ref_tag, sup_tag)
    end
    processed_line
  end

  # This helper finds the first complete pair of markers and replaces it,
  # then continues with the rest of the string. It doesn't handle nesting.
  def gsub_with_pairing(line, marker, open_tag, close_tag, process_content = false)
    output = ""
    scanner = line
    while (start_index = scanner.index(marker))
      # Append text before the first marker
      output << scanner[0...start_index] # steep:ignore

      # Move scanner past the first marker
      scanner = scanner[start_index + marker.length..-1]

      if (end_index = scanner.index(marker))
        # Found a closing marker
        content = scanner[0...end_index] || ''
        content = escape_html(content) if process_content
        output << open_tag << content << close_tag

        # Move scanner past the closing marker
        scanner = scanner[end_index + marker.length..-1]
      else
        # No closing marker found, so the first marker is literal
        output << marker
        break
      end
    end
    # Append the remainder of the string
    output << scanner
    output
  end

  def escape_html(text)
    # A simple escape method.
    # It's not complete, but good enough for code blocks for now.
    text.gsub('&', '&amp;').gsub('<', '&lt;').gsub('>', '&gt;').gsub('"', '&quot;')
  end
end
