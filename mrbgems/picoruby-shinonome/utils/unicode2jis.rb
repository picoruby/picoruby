module Unicode2JIS
  def self.make(src, dst)
    jis0208 = {}
    File.open(src) do |f|
      f.each_line do |line|
        next if line.start_with?("#")
        next if line.strip.empty?
        _, jis, unicode = line.split(/\s+/)
        jis0208[unicode] = jis
      end
    end

    # sort by unicode
    jis0208 = jis0208.sort_by{ |k, _| k }.to_h

    File.open(dst, "w") do |f|
      f.puts <<~EOS
        #include <stdint.h>
        typedef struct {
          uint16_t unicode;
          uint16_t jis;
        } unicode2jis_t;
      EOS
      f.puts "const unicode2jis_t unicode2jis[] = {"
      jis0208.each do |unicode, jis|
        f.puts "  {#{unicode}, #{jis}},"
      end
      f.puts "  {0, 0}"
      f.puts "};"
    end
  end
end
