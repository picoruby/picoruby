#!/usr/bin/env ruby
# Import the regex engine from its upstream repository into lib/.
#
#   ruby lib/import.rb <upstream runtime directory>
#
# The upstream directory holds ni_wregex.c and ni_regex.h. The script
# writes lib/regex_engine.c and lib/regex_engine.h:
#
# - It drops the upstream class glue and the build switch around it.
#   Only the compiler and the VM remain.
# - It renames the public identifiers to the regex_ / REGEX_ prefix.
# - It rewrites the comments that describe the upstream runtime.
#
# It fails when a name of the upstream project or its prefix survives.
# Run it after every upstream change; do not edit the output by hand.

require 'fileutils'

if ARGV.size != 1
  warn "usage: ruby #{$0} <upstream runtime directory>"
  exit 1
end

src_dir = ARGV[0]
lib_dir = __dir__

RENAMES = {
  'ni_regex_compile_words' => 'regex_compile_words',
  'ni_regex_scratch_words' => 'regex_scratch_words',
  'ni_regex_flag_bits'     => 'regex_flag_bits',
  'ni_regex_exec'          => 'regex_exec',
  'NI_REGEX_ICASE'         => 'REGEX_ICASE',
  'NI_REGEX_DOTALL'        => 'REGEX_DOTALL',
  'NI_REGEX_H'             => 'REGEX_ENGINE_H',
  'ni_malloc'              => 'regex_malloc',
  'ni_realloc'             => 'regex_realloc',
  'ni_free'                => 'regex_free',
  'ni_regex.h'             => 'regex_engine.h',
  'ni_wregex.c'            => 'regex_engine.c',
}.freeze

def rename(text)
  RENAMES.each do |from, to|
    text = text.gsub(/\b#{Regexp.escape(from)}\b/, to)
  end
  text
end

# Replace exactly one occurrence of a passage; fail when it is absent
# or ambiguous, so an upstream change does not slip through unnoticed.
def replace_once(text, from, to)
  n = text.scan(from).size
  raise "expected one match of #{from.inspect}, found #{n}" unless n == 1
  text.sub(from, to)
end

def check_clean(name, text)
  left = text.scan(/^.*(?:\bni_|\bNI_|nicht).*$/i)
  return if left.empty?
  warn "#{name}: upstream names survive:"
  left.each { |l| warn "  #{l}" }
  exit 1
end

# ---- regex_engine.c ----

c = File.read(File.join(src_dir, 'ni_wregex.c'))

# the paragraph on how the upstream runtime owns the program
c = replace_once(c,
  /^ \* The program is bytes in a Binary;.*?constructor or not\.\n/m,
  " * The program is a flat array of words that the host keeps with its\n" \
  " * Regexp object. Nothing here holds a value across calls.\n")

# the surface of the upstream class, which is dropped below
c = replace_once(c,
  /^ \* Surface \(all on the RegexVM class.*?newline\)\.\n/m,
  " * The host binds regex_compile_words and regex_exec (regex_engine.h).\n")

# the build switch: only the engine half is wanted here
c = replace_once(c,
  /^#ifdef NI_REGEX_TOOL\n.*?^#endif\n/m,
  "/* The compiler reaches the host allocator through these three names,\n" \
  " * which the host defines. The VM allocates nothing. */\n" \
  "void *regex_malloc(size_t n);\n" \
  "void *regex_realloc(void *p, size_t n);\n" \
  "void regex_free(void *p);\n")

# the upstream class glue after the VM
c = replace_once(c,
  /^#ifndef NI_REGEX_TOOL\n.*?^#endif \/\* NI_REGEX_TOOL \*\/\n/m,
  '')
c = c.sub(/\n+\z/, "\n")

c = rename(c)
check_clean('regex_engine.c', c)

# ---- regex_engine.h ----

h = File.read(File.join(src_dir, 'ni_regex.h'))

h = replace_once(h,
  /\A\/\* ni_regex\.h -- .*?\*\/\n/m,
  "/* regex_engine.h -- the regex engine's seam.\n" \
  " *\n" \
  " * The host compiles a pattern with regex_compile_words and runs the\n" \
  " * program with regex_exec. The compiler's only need is regex_malloc,\n" \
  " * regex_realloc and regex_free, which the host supplies; the VM needs\n" \
  " * nothing. This header depends on the C library only. */\n")

h = rename(h)
check_clean('regex_engine.h', h)

# ---- write ----

File.write(File.join(lib_dir, 'regex_engine.c'), c)
File.write(File.join(lib_dir, 'regex_engine.h'), h)
%w[ni_wregex.c ni_regex.h].each do |old|
  FileUtils.rm_f(File.join(lib_dir, old))
end
puts "wrote lib/regex_engine.c (#{c.lines.size} lines) and lib/regex_engine.h (#{h.lines.size} lines)"
