# Folds the RUBY_ENGINE branches of a PicoRuby source for one engine.
#
# Spinel answers RUBY_ENGINE with "spinel" and drops the branch a
# `RUBY_ENGINE == "..."` check rules out before it analyzes: it keeps the
# mruby/c side of `if RUBY_ENGINE == 'mruby' ... else ... end`, which the
# VM the generated C runs on never executes. The workspace gives Spinel a
# copy of each mrblib file folded for mruby instead: a comparison of
# RUBY_ENGINE with a String reads `true` or `false`, the branch it rules
# out is blanked, and `case RUBY_ENGINE` keeps the matching `when` only.
# Columns and line numbers do not move, so Spinel's positions map back to
# the original file one to one.
require "prism"

module PicorubyAot
  module EngineFold
    module_function

    # The source with its RUBY_ENGINE branches folded for `engine`.
    def fold(src, engine: "mruby")
      return src unless src.include?("RUBY_ENGINE")
      result = Prism.parse(src)
      return src if result.failure?
      folder = Folder.new(engine)
      result.value.accept(folder)
      out = src.b   # Prism offsets are bytes
      folder.blanks.each { |r| blank!(out, r) }
      folder.edits.each do |range, text|
        next if folder.blanks.any? { |b| b.cover?(range.begin) }
        write!(out, range, text)
      end
      out.force_encoding(src.encoding)
    end

    # Every non-newline byte of the range becomes a space.
    def blank!(out, range)
      range.each { |i| out.setbyte(i, 0x20) unless out.getbyte(i) == 0x0a }
    end

    # The text, padded with spaces to the range (the text is never longer:
    # `true` / `false` for a comparison, a String literal for the constant).
    def write!(out, range, text)
      padded = text.ljust(range.size)
      padded = padded[0, range.size]
      out[range] = padded
    end

    class Folder < Prism::Visitor
      attr_reader :blanks, :edits

      def initialize(engine)
        @engine = engine
        @blanks = []   # ranges of bytes to blank
        @edits = []    # [range, text]
      end

      # `RUBY_ENGINE == "x"` / `!= "x"` (the constant bare or `::RUBY_ENGINE`):
      # true, false, or nil when the call is something else.
      def comparison(node)
        return nil unless node.is_a?(Prism::CallNode) && %i[== !=].include?(node.name)
        return nil unless engine_const?(node.receiver)
        args = node.arguments&.arguments
        return nil unless args && args.size == 1 && args[0].is_a?(Prism::StringNode)
        same = args[0].unescaped == @engine
        node.name == :== ? same : !same
      end

      def engine_const?(node)
        (node.is_a?(Prism::ConstantReadNode) && node.name == :RUBY_ENGINE) ||
          (node.is_a?(Prism::ConstantPathNode) && node.parent.nil? && node.name == :RUBY_ENGINE)
      end

      def range_of(loc)
        loc.start_offset...loc.end_offset
      end

      # A branch's bytes up to its `end` keyword: an `else` and an `elsif`
      # node span the `end` that closes the whole `if`, which stays.
      def branch_range(node)
        stop = node.respond_to?(:end_keyword_loc) && node.end_keyword_loc ? node.end_keyword_loc.start_offset : node.location.end_offset
        node.location.start_offset...stop
      end

      def visit_call_node(node)
        value = comparison(node)
        @edits << [range_of(node.location), value.to_s] unless value.nil?
        super
      end

      # The branch the folded predicate rules out is blanked: the `then`
      # statements, or the `else` / `elsif` chain that follows.
      def visit_if_node(node)
        fold_branches(node, node.if_keyword_loc, node.subsequent, comparison(node.predicate))
        super
      end

      def visit_unless_node(node)
        value = comparison(node.predicate)
        fold_branches(node, node.keyword_loc, node.else_clause, value.nil? ? nil : !value)
        super
      end

      # A modifier (`x if RUBY_ENGINE == "y"`) has its statement before the
      # keyword: a dead one goes whole, a live one keeps the statement alone.
      def fold_branches(node, keyword, rest, live)
        return if live.nil?
        st = node.statements
        if node.end_keyword_loc.nil? && st && keyword && st.location.start_offset < keyword.start_offset
          @blanks << (live ? (keyword.start_offset...node.location.end_offset) : range_of(node.location))
        elsif live
          @blanks << branch_range(rest) if rest
        else
          @blanks << range_of(st.location) if st
        end
      end

      # `case RUBY_ENGINE when "a" ... when "b" ... else ... end`: the
      # constant reads as the engine's name, the `when` clauses that do not
      # name it and, when one does, the `else` are blanked.
      def visit_case_node(node)
        if engine_const?(node.predicate)
          @edits << [range_of(node.predicate.location), @engine.inspect]
          matched = false
          node.conditions.each do |w|
            strings = w.conditions.all? { |c| c.is_a?(Prism::StringNode) }
            hit = strings && w.conditions.any? { |c| c.unescaped == @engine }
            if hit
              matched = true
            else
              @blanks << range_of(w.location)
            end
          end
          @blanks << branch_range(node.else_clause) if matched && node.else_clause
        end
        super
      end
    end
  end
end
