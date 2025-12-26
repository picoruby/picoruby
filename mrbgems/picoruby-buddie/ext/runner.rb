path = JS.global.location&.pathname.to_s
#puts "path: #{path}"
if ['','/','/?'].include?(path) || path.start_with?("?")
  Buddie.run
else
  misc = path.split('/').reject(&:empty?).first
  #puts "misc: #{misc}"
  buddie = Buddie.new
  buddie.render_static_page("misc/#{misc}.md")
end
