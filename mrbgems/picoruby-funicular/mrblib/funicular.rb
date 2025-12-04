require 'js'

module Funicular
  VERSION = '0.1.0'

  def self.version
    VERSION
  end

  @router = nil

  def self.router
    @router
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
  end
end
