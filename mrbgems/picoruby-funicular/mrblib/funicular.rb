require 'js'

module Funicular
  VERSION = '0.1.0'

  def self.version
    VERSION
  end

  # Start Funicular application
  # Usage:
  #   Funicular.start(MyComponent, container: 'app')
  #   Funicular.start(MyComponent, container: 'app', props: { name: 'John' })
  def self.start(component_class, container: 'app', props: {})
    container_element = if container.is_a?(String)
      JS.document.getElementById(container)
    else
      container
    end

    unless container_element
      raise "Container element not found: #{container}"
    end

    instance = component_class.new(props)
    instance.mount(container_element)
    instance
  end
end
