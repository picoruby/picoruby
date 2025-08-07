class Rapicco
  class Parser
    def initialize
      @font = :go12
      @title_font = :go16
      @bold_color = :red
      @align = :left
      @lines = []
      @in_code_block = false
    end

    attr_accessor :font, :title_font, :bold_color, :align


    # Parse one raw line (with or without "\n"). Blank lines are ignored.
    def parse(raw_line)
      return if raw_line.nil? || raw_line.empty?
      line = raw_line.chomp
      return if line.empty? && !@in_code_block

      # -------- code fence check ----------------------------------
      if line.start_with?('```')
        @in_code_block = !@in_code_block
        return                          # fence lines themselves are ignored
      end

      if @in_code_block
        # treat entire line as verbatim code, no inline parsing
        @lines << {
          code: line
        }
        return
      end

      # -------- note -----------------------------------
      if line.start_with?('> ')
        @lines << { note: line }
        return
      end

      # -------- comment ---------------------------------
      if line.start_with?('<!--')
        return
      end

      idx = 0
      prefix_char = line[idx]

      # ---- defaults by prefix -------------------------------------
      bullet = false
      align  = @align
      scale  = 1
      font   = nil
      line_color = :white
      skip   = nil

      case prefix_char
      when '#'
        align = :center
        scale = 1
        idx  += 1
      when '-'
        bullet = true
        idx   += 1
      end

      idx += 1 while idx < line.length && line[idx] == ' '

      # ---- attribute block ---------------------------------------
      if idx < line.length && line[idx] == '{'
        idx += 1
        attr_s = ""
        while idx < line.length && line[idx] != '}'
          attr_s << line[idx].to_s
          idx += 1
        end
        idx += 1  # skip }

        attrs = self.class.parse_attrs(attr_s)

        font        = attrs[:font].to_sym        if attrs.has_key?(:font)
        scale       = attrs[:scale].to_i         if attrs.has_key?(:scale)
        align       = attrs[:align].to_sym       if attrs.has_key?(:align)
        bullet      = (attrs[:bullet] == 'true') if attrs.has_key?(:bullet)
        line_color  = attrs[:color].to_sym       if attrs.has_key?(:color)
        skip        = attrs[:skip].to_i          if attrs.has_key?(:skip)

        idx += 1 while idx < line.length && line[idx] == ' '
      end

      # ---- inline text -------------------------------------------
      text_segments = self.class.parse_inline(line[idx..-1].to_s, line_color, @bold_color)

      if skip
        line = { skip: skip }
      else
        # @type var line: Hash[Symbol, bool | Integer | Symbol | parser_dump_t]
        line = {
          font: (font || default_font(prefix_char)),
          text: text_segments
        }
        line[:bullet] = true if bullet
        line[:scale] = scale if 1 < scale
        line[:align] = align.to_sym if align != :left
      end
      @lines << line
    end

    # Return accumulated array **and clear internal state**
    def dump
      out = @lines
      @lines = []
      out
    end

    # ---------------- class helper methods ------------------------
    def self.parse_attrs(str)
      h = {}
      str.tr(',', ' ').split(' ').each do |pair|
        eq = pair.index('=')
        next unless eq
        key = pair[0, eq]&.to_sym
        val = pair[eq + 1, pair.length - eq - 1]
        h[key] = val
      end
      h
    end

    def self.parse_inline(src, base_color, bold_color)
      segs = []
      buf  = ""
      i    = 0

      while i < src.length
        c = src[i]

        # escape sequences: \*, \{, \}
        if c == '\\' && i + 1 < src.length && ["*", "{", "}"].include?(src[i + 1].to_s)
          buf << src[i + 1].to_s
          i += 2
          next
        end

        # span start
        if c == '*'
          # flush plain buffer
          unless buf.empty?
            seg = { div: buf }
            seg[:color] = base_color
            segs << seg
            buf = ""
          end

          i += 1
          span_color = bold_color || base_color

          # optional {color}
          if i < src.length && src[i] == '{'
            i += 1
            color_buf = ""
            while i < src.length && src[i] != '}'
              color_buf << src[i].to_s
              i += 1
            end
            span_color = color_buf.to_sym unless color_buf.empty?
            i += 1 # skip }
          end

          # collect span text until closing *
          span_txt = ""
          while i < src.length && src[i] != '*'
            if src[i] == '\\' && i + 1 < src.length && src[i + 1] == '*'
              span_txt << '*'
              i += 2
            else
              span_txt << src[i].to_s
              i += 1
            end
          end
          i += 1 # skip closing *

          # @type var seg: Hash[Symbol, Symbol|String]
          seg = { div: span_txt }
          seg[:color] = span_color
          segs << seg
          next
        end

        # regular char
        buf << c.to_s
        i += 1
      end

      # @type var seg: Hash[Symbol, Symbol|String]
      seg = { div: buf }
      seg[:color] = base_color if base_color
      segs << seg unless buf.empty?
      segs
    end

    def default_font(prefix_char)
      prefix_char == '#' ? @title_font : @font
    end
  end
end
