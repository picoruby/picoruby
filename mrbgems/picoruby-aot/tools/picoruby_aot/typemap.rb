# The types Spinel inferred, looked up by Prism source position.
#
# `spinel --emit-types` identifies a node by its file and its Prism
# location (1-based line, 0-based column, exclusive end). Prism gives the
# generator the same numbers, so a node's type is one hash lookup away.

module PicorubyAot
  class TypeMap
    # Spinel's type vocabulary folded to what the generated C cares about:
    # the three unboxed kinds, nil, and "everything else is an mrb_value".
    SCALARS = {
      "int" => :int,
      "float" => :float,
      "bool" => :bool,
      "nil" => :nil,
    }.freeze

    def initialize(json)
      @by_pos = {}
      @defs = []
      (json["types"] || []).each do |r|
        key = [File.expand_path(r["file"]), r["line"], r["col"], r["end_line"], r["end_col"]]
        # Several records can share one position (a StatementsNode and its
        # only statement, a call and its receiver at the same column); keep
        # them all, keyed by kind as well.
        (@by_pos[key] ||= {})[r["kind"]] = r
        @defs << r if r["kind"] == "DefNode"
      end
      @diagnostics = json["diagnostics"] || []
    end

    def record(file, node)
      loc = node.location
      key = [File.expand_path(file), loc.start_line, loc.start_column, loc.end_line, loc.end_column]
      recs = @by_pos[key]
      return nil unless recs
      recs[node.class.name.split("::").last] || recs.values.first
    end

    # :int / :float / :bool / :nil / :value / nil (unknown to Spinel)
    def kind(file, node)
      r = record(file, node)
      return nil unless r
      SCALARS.fetch(r["type"], :value)
    end

    # The unboxed kind the node's value can safely take: :int or :float
    # when Spinel proved that no nil reaches it ("nullable":false; an
    # Integer and an Integer? share the tag "int", so the type alone is not
    # enough), :bool always (Spinel's bool has no nil form), else nil. A
    # dump from a spinel that predates the field answers nil for every
    # Integer and Float, which keeps the generator on its guarded path.
    def scalar(file, node)
      r = record(file, node)
      return nil unless r
      k = SCALARS[r["type"]]
      return nil if k.nil? || k == :nil
      return k if k == :bool
      r["nullable"] == false ? k : nil
    end

    def raw_type(file, node)
      r = record(file, node)
      r && r["type"]
    end

    def def_record(file, def_node)
      record(file, def_node)
    end
  end
end
