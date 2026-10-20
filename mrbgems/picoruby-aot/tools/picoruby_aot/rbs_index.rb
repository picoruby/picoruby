# The types a gem's RBS declares, for the wait search. A parameter, an
# instance variable or a method's value that Spinel holds as a box may
# still be declared a core kind (`Array[String]`, `Hash[Symbol, untyped]`,
# `String`) or a class of the gem, and the declaration says where a call
# on it can land: `patches.each` on an `Array[Patch]` is Array#each, not
# IndexedDB::Store#each. Trusting the declaration is what Spinel does with
# the same file under --rbs, and what steep checks against the CRuby run.

require "rbs"

module PicorubyAot
  class RbsIndex
    def initialize(sigfiles)
      @methods = {}   # "Owner#name" / "Owner.name" => { params: [type...], return: type }
      @ivars = {}     # "Owner" => { "@x" => type }
      @aliases = {}   # "Owner::name" (and "name" at the top) => type
      sigfiles.each { |f| load_sig(f) }
    end

    # The declared type of the i-th positional parameter (required ones
    # first, then optional), or nil.
    def param_type(owner, name, singleton, index)
      m = @methods["#{owner}#{singleton ? '.' : '#'}#{name}"]
      m && m[:params][index]
    end

    def return_type(owner, name, singleton)
      m = @methods["#{owner}#{singleton ? '.' : '#'}#{name}"]
      m && m[:return]
    end

    def ivar_type(owner, ivar)
      h = @ivars[owner]
      h && h[ivar]
    end

    # A type alias by its name, looked up from `scope` outward.
    def alias_type(name, scope)
      parts = scope.to_s.split("::")
      while true
        t = @aliases[(parts + [name]).join("::")]
        return t if t
        break if parts.empty?
        parts.pop
      end
      @aliases[name]
    end

    private

    def load_sig(path)
      buf = RBS::Buffer.new(name: path, content: File.read(path))
      _, _, decls = RBS::Parser.parse_signature(buf)
      decls.each { |d| collect(d, []) }
    end

    def collect(decl, scope)
      case decl
      when RBS::AST::Declarations::Class, RBS::AST::Declarations::Module
        full = (scope + [decl.name.to_s]).join("::")
        decl.members.each do |m|
          case m
          when RBS::AST::Declarations::Class, RBS::AST::Declarations::Module
            collect(m, scope + [decl.name.to_s])
          when RBS::AST::Declarations::TypeAlias
            @aliases["#{full}::#{m.name}"] = m.type
          when RBS::AST::Members::MethodDefinition
            f = m.overloads.first.method_type.type
            params = (f.required_positionals + f.optional_positionals).map(&:type)
            kind = m.kind == :singleton ? "." : "#"
            @methods["#{full}#{kind}#{m.name}"] = { params: params, return: f.return_type }
          when RBS::AST::Members::InstanceVariable
            (@ivars[full] ||= {})[m.name.to_s] = m.type
          when RBS::AST::Members::AttrReader, RBS::AST::Members::AttrAccessor, RBS::AST::Members::AttrWriter
            (@ivars[full] ||= {})["@#{m.name}"] = m.type
            unless m.is_a?(RBS::AST::Members::AttrWriter)
              @methods["#{full}##{m.name}"] = { params: [], return: m.type }
            end
          end
        end
      when RBS::AST::Declarations::TypeAlias
        @aliases[decl.name.to_s] = decl.type
      end
    end
  end
end
