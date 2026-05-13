#!/usr/bin/env ruby

require "fileutils"

DEFAULT_OUTPUT_DIR = "/private/tmp/picoruby-serial-transfer-samples"

SAMPLES = [
  ["sample_12kb.rb", 12 * 1024],
  ["sample_100kb.rb", 100 * 1024],
  ["sample_1mb.rb", 1024 * 1024],
].freeze

output_dir = ARGV[0] || DEFAULT_OUTPUT_DIR
FileUtils.mkdir_p(output_dir)

def build_sample(name, target_size)
  header = <<~RUBY
    module SerialTransferSample
      NAME = #{name.inspect}
      TARGET_SIZE = #{target_size}

      def self.summary
        "\#{NAME} loaded (\#{TARGET_SIZE} bytes target)"
      end
    end

  RUBY

  footer = <<~RUBY
    puts SerialTransferSample.summary
  RUBY

  body = +""
  while header.bytesize + body.bytesize + footer.bytesize < target_size
    remaining = target_size - header.bytesize - body.bytesize - footer.bytesize
    line = "# #{name} padding #{body.bytesize}\n"
    body << line.byteslice(0, remaining)
  end

  sample = header + body + footer
  sample = sample.byteslice(0, target_size) if sample.bytesize > target_size
  sample
end

SAMPLES.each do |name, target_size|
  path = File.join(output_dir, name)
  content = build_sample(name, target_size)
  File.binwrite(path, content)
  puts "#{path} (#{content.bytesize} bytes)"
end
