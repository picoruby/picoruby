module Funicular
  class Router
    attr_reader :routes, :current_component, :current_path

    def initialize(container)
      @container = container
      @routes = {}
      @default_route = nil
      @current_component = nil
      @current_path = nil
      @popstate_callback_id = nil
    end

    # Add a route
    def add_route(path, component_class)
      @routes[path] = component_class
    end

    # Set default route (used when path is empty)
    def set_default(path)
      @default_route = path
    end

    # Start listening to popstate
    def start
      # Set up popstate listener
      @popstate_callback_id = JS.global.addEventListener('popstate') do |event|
        handle_route_change
      end

      # Handle initial route
      if current_location_path == '/' && @default_route
        # Use replaceState to not add a new entry to the history
        JS.global.history.replaceState(JS::Bridge.to_js({}), '', @default_route)
      end
      handle_route_change
    end

    # Stop listening to popstate
    def stop
      if @popstate_callback_id
        JS::Object.removeEventListener(@popstate_callback_id)
        @popstate_callback_id = nil
      end

      unmount_current_component
    end

    # Navigate to a path programmatically using History API
    def navigate(path)
      JS.global.history.pushState(JS::Bridge.to_js({}), '', path)
      # Manually trigger route change because pushState doesn't fire popstate
      handle_route_change
    end

    # Get current path from location
    def current_location_path
      js_path_obj = JS.global.location.pathname
      path = js_path_obj.to_s
      path.empty? ? '/' : path
    end

    private

    # Handle route change
    def handle_route_change
      path = current_location_path

      # Find matching route
      component_class, params = find_route(path)

      unless component_class
        # Maybe render a 404 component?
        return
      end

      # Don't remount if already on this path
      return if @current_path == path

      # Unmount current component
      unmount_current_component

      # Mount new component
      @current_path = path
      @current_component = component_class.new(params)
      # @type ivar @current_component: Funicular::Component
      @current_component.mount(@container)
    end

    # Unmount current component
    def unmount_current_component
      @current_component&.unmount
      @current_component = nil
      @current_path = nil
    end

    private

    def find_route(path)
      path_segments = path.split('/').reject { |s| s.empty? }
      params = {}
      @routes.each do |route_pattern, component_class|
        pattern_segments = route_pattern.split('/').reject { |s| s.empty? }
        next if pattern_segments.length != path_segments.length

        match = true

        pattern_segments.each_with_index do |pattern_segment, index|
          path_segment = path_segments[index]

          if pattern_segment.start_with?(':')
            param_name = pattern_segment[1..-1]&.to_sym
            if param_name.nil?
              raise "Invalid parameter name in route pattern: #{route_pattern}"
            end
            params[param_name] = path_segment
          elsif pattern_segment != path_segment
            match = false
            break
          end
        end

        return [component_class, params] if match
      end

      [nil, params] # No route found
    end
  end
end
