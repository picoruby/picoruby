require 'js'

module Funicular
  VERSION = '0.1.0'

  def self.version
    VERSION
  end

  def self.env
    @env ||= EnvironmentInquirer.new(ENV['FUNICULAR_ENV'] || ENV['RAILS_ENV'] || 'development')
  end

  def self.env=(environment)
    case environment
    when EnvironmentInquirer
      @env = environment
    when nil
      @env = nil
    else
      @env = EnvironmentInquirer.new(environment)
    end
  end

  @router = nil

  def self.router
    @router
  end

  # Load schemas for models
  # Usage:
  #   Funicular.load_schemas({ User => "user", Session => "session" }) do
  #     Funicular.start(container: 'app') { |router| ... }
  #   end
  def self.load_schemas(models, &block)
    schemas_loaded = 0
    total_schemas = models.size

    check_completion = -> {
      if schemas_loaded >= total_schemas
        puts "[Funicular] All schemas loaded (#{schemas_loaded}/#{total_schemas})"
        block.call if block
      end
    }

    models.each do |model_class, schema_name|
      HTTP.get("/api/schema/#{schema_name}") do |response|
        if response.error?
          puts "[Schema] Failed to load #{schema_name} schema: #{response.error_message}"
        else
          model_class.load_schema(response.data)
          puts "[Schema] #{schema_name} model initialized"
          schemas_loaded += 1
          check_completion.call
        end
      end
    end
  end

  # Start Funicular application
  # Usage:
  #   Funicular.start(MyComponent, container: 'app')
  #   Funicular.start(MyComponent, container: 'app', props: { name: 'John' })
  def self.start(component_class = nil, container: 'app', props: {}, &block)
    container_element = if container.is_a?(String)
      JS.document.getElementById(container)
    else
      container
    end

    unless container_element
      raise "Container element not found: #{container}"
    end

    # If block is given, use router mode
    if block
      router = Router.new(container_element)
      @router = router
      block.call(router)
      router.start
      return router
    end

    # Otherwise, mount single component (backward compatible)
    if component_class
      instance = component_class.new(props)
      instance.mount(container_element)
      return instance
    end

    raise "Either component_class or block must be provided"
  rescue => e
    puts "Exception in Funicular.start: #{e.message}"
    puts e.backtrace
    raise e
  end

  # Form builder configuration
  class << self
    attr_accessor :form_builder_config

    def configure_forms
      @form_builder_config ||= {
        error_class: "text-red-600 text-sm mt-1",
        field_error_class: "border-red-500"
      }
      yield @form_builder_config if block_given?
    end
  end

  # Initialize default form configuration
  configure_forms
end
