#!/usr/bin/env ruby

require 'bundler/inline'

gemfile do
  source 'https://rubygems.org'
  gem 'chunky_png'
end

module PNGBIT
  def self.rgb_to_xterm256(r, g, b)
    gray_avg = (r + g + b) / 3
    if (r - g).abs < 5 && (r - b).abs < 5 && (g - b).abs < 5
      gray_idx = ((gray_avg - 8) / 10.7).round
      return 232 + gray_idx.clamp(0, 23)
    end

    to_cube = ->(c) {
      if c < 48 then 0
      elsif c < 115 then 1
      else ((c - 35) / 40.0).round.clamp(0, 5)
      end
    }

    r6 = to_cube.call(r)
    g6 = to_cube.call(g)
    b6 = to_cube.call(b)
    16 + 36 * r6 + 6 * g6 + b6
  end

  DEFAULT_WIDTH = 150

  def self.run(infile, outfile = nil, target_width = nil)
    png = ChunkyPNG::Image.from_file(infile)

    target_width ||= DEFAULT_WIDTH
    aspect_ratio = png.height.to_f / png.width
    char_aspect_ratio = 0.5  # terminal adjustment
    target_height = (target_width * aspect_ratio * char_aspect_ratio).round

    resized = png.resample_bilinear(target_width, target_height)

    outfile ||= infile.sub(/\.\w+$/, '.bit')

    File.open(outfile, 'wb') do |f|
      f.write MAGIC
      f.write [resized.width].pack("n")
      f.write [resized.height].pack("n")
      resized.height.times do |y|
        resized.width.times do |x|
          pixel = resized[x, y]
          alpha = ChunkyPNG::Color.a(pixel)
          if alpha < 128
            f.write [0x00].pack("C")
          else
            r, g, b = ChunkyPNG::Color.r(pixel), ChunkyPNG::Color.g(pixel), ChunkyPNG::Color.b(pixel)
            code = rgb_to_xterm256(r, g, b)
            code = 1 if code == 0
            f.write [code].pack("C")
          end
        end
      end
    end
    outfile
  end
end

if __FILE__ == $0
  load "../mrblib/pngbit.rb"
  if ARGV[0].nil?
    puts "Usage: png2bit.rb <input.png> [output.bit] [target_width]"
    return
  end
  outfile = PNGBIT.run(ARGV[0], ARGV[1], ARGV[2]&.to_i)
  print "\e[H\e[2J"  # Clear screen
  PNGBIT.show(outfile)
end
