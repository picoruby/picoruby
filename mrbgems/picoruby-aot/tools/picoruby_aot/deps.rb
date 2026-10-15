# The host gems the generator needs, with a message that says what to do
# when one is missing. `prism` parses the Ruby (a default gem since Ruby
# 3.3; any version from 1.0 reads the nodes used here), `rbs` parses the
# signature files the stubs are made from.

module PicorubyAot
  HOST_GEMS = {
    "prism" => "prism",
    "rbs" => "rbs",
  }.freeze

  def self.require_host_gems
    missing = []
    HOST_GEMS.each do |feature, gem_name|
      begin
        require feature
      rescue LoadError
        missing << gem_name
      end
    end
    return if missing.empty?
    warn "picoruby-aot: the host gem(s) #{missing.join(', ')} are not installed."
    warn "  gem install #{missing.join(' ')}   # or add them to the Gemfile and run under bundler"
    exit 1
  end
end

PicorubyAot.require_host_gems
