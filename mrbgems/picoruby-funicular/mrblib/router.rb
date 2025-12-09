module Funicular
  class Router
    attr_reader :routes, :current_component, :current_path

    def initialize(container)
      @container = container
      @routes = {}
      @default_route = nil
      @current_component = nil
      @current_path = nil
      @callback_id = nil
    end

    # Add a route
    def add_route(path, component_class)
      @routes[path] = component_class
    end

    # Set default route (used when hash is empty)
    def set_default(path)
      @default_route = path
    end

    # Start listening to hash changes
    def start
      # Set up hashchange listener
      @callback_id = JS.global.addEventListener('hashchange') do |event|
        handle_route_change
      end

      # Handle initial route
      handle_route_change
    end

    # Stop listening to hash changes
    def stop
      if @callback_id
        JS::Object.removeEventListener(@callback_id)
        @callback_id = nil
      end

      unmount_current_component
    end

    # Navigate to a path programmatically
    def navigate(path)
      # Update hash (will trigger hashchange event)
      JS.global[:location][:hash] = path
    end

    # Get current path from hash
    def current_hash_path
      # @type var hash: String
      hash = JS.global[:location][:hash].to_poro
      # Remove leading '#' if present
      if hash && !hash.empty? && hash[0] == '#'
        hash[1..-1] || ''
      else
        hash || ''
      end
    end

    private

    # Handle route change
    def handle_route_change
      path = current_hash_path

      # Use default route if path is empty
      if path.empty? && @default_route
        navigate(@default_route)
        return
      end

      # Find matching route
      component_class = @routes[path]

      unless component_class
        puts "[Router] No route found for: #{path}"
        return
      end

      # Don't remount if already on this path
      return if @current_path == path

      # Unmount current component
      unmount_current_component

      # Mount new component
      @current_path = path
      @current_component = component_class.new
      # @type ivar @current_component: Funicular::Component
      @current_component.mount(@container)

      puts "[Router] Navigated to: #{path}"
    end

    # Unmount current component
    def unmount_current_component
      @current_component&.unmount
      @current_component = nil
      @current_path = nil
    end
  end
end
