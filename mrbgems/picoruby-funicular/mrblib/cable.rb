module Funicular
  module Cable
    # Create a new Consumer instance connected to the specified URL
    # @param url [String] WebSocket URL (e.g., "ws://localhost:3000/cable")
    # @return [Consumer]
    def self.create_consumer(url)
      Consumer.new(url)
    end

    # Consumer manages the WebSocket connection and subscriptions
    class Consumer
      attr_reader :url, :subscriptions

      def initialize(url)
        @url = url
        @subscriptions = Subscriptions.new(self)
        @websocket = nil
        @connected = false
        @reconnect_attempts = 0
        @reconnect_timer = nil
        @pending_commands = []
        @suspend_timer = nil
        @suspended = false
        connect
        setup_visibility_handler
      end

      # Establish WebSocket connection
      def connect
        puts "[Cable] Creating WebSocket connection to #{@url}"
        @websocket = WebSocket.new(@url)
        puts "[Cable] WebSocket object created, setting up handlers"

        @websocket.onopen do |event|
          @connected = true
          @reconnect_attempts = 0
          puts "[Cable] Connected to #{@url}"
          flush_pending_commands
        end

        @websocket.onmessage do |event|
          begin
            # event is a JS::Object wrapping MessageEvent
            # Access data property and convert to Ruby string
            data_obj = event[:data]
            data_str = data_obj.to_s # Always convert JS::Object to Ruby String
            handle_message(data_str)
          rescue => e
            puts "[Cable] Error in onmessage: #{e.class}: #{e.message}"
          end
        end

        @websocket.onerror do |event|
          puts "[Cable] Error occurred"
        end

        @websocket.onclose do |event|
          @connected = false
          puts "[Cable] Disconnected"
          schedule_reconnect
        end
      end

      # Send a command to the server
      # @param command [Hash] Command data
      def send_command(command)
        if @connected && @websocket&.open?
          json = JSON.generate(command)
          puts "[Cable] Sending command: #{json}"
          @websocket.send(json)
        else
          puts "[Cable] Queuing command (not connected): #{command.inspect}"
          @pending_commands << command
        end
      end

      # Disconnect and clean up
      def disconnect
        @websocket&.close if @websocket
        @websocket = nil
        @connected = false
      end

      private

      # Handle incoming message
      def handle_message(data)
        message = JSON.parse(data)
        type = message["type"]
        identifier = message["identifier"]

        case type
        when "ping"
          # Heartbeat - no action needed (silent)
        when "welcome"
          puts "[Cable] Welcome message received"
        when "confirm_subscription"
          @subscriptions.notify_subscription_confirmed(identifier)
        when "reject_subscription"
          @subscriptions.notify_subscription_rejected(identifier)
        else
          # Regular message
          @subscriptions.notify_message(identifier, message["message"])
        end
      end

      # Flush pending commands after reconnection
      def flush_pending_commands
        return if @pending_commands.empty?

        puts "[Cable] Flushing #{@pending_commands.size} pending commands"
        @pending_commands.each do |command|
          json = JSON.generate(command)
          @websocket.send(json)
        end
        @pending_commands.clear
      end

      # Schedule reconnection with exponential backoff
      def schedule_reconnect
        begin
          return if @suspended
          delay = calculate_backoff_delay
          puts "[Cable] Reconnecting in #{delay} seconds (attempt #{@reconnect_attempts + 1})"
          sleep delay
          puts "[Cable] Sleep completed, attempting reconnection..."
          return if @suspended
          @reconnect_attempts += 1
          connect
        rescue => e
          puts "[Cable] Error in schedule_reconnect: #{e.class}: #{e.message}"
        end
      end

      # Calculate exponential backoff delay (1s, 2s, 4s, 8s, 16s, 32s, max 60s)
      def calculate_backoff_delay
        base_delay = 1
        max_delay = 60
        delay = base_delay * (2 ** @reconnect_attempts)
        delay < max_delay ? delay : max_delay
      end

      # Setup Page Visibility API handler
      def setup_visibility_handler
        JS.global.document.addEventListener("visibilitychange") do
          if JS.global.document[:hidden]
            schedule_suspend
          else
            cancel_suspend
            ensure_connected
          end
        end
      end

      # Schedule suspension after 30 seconds in background
      def schedule_suspend
        return if @suspended || @suspend_timer
        puts "[Cable] Page hidden, scheduling suspension in 30 seconds"
        @suspend_timer = sleep(30) do
          suspend_connection
        end
      end

      # Cancel scheduled suspension
      def cancel_suspend
        if @suspend_timer
          puts "[Cable] Page visible, canceling suspension"
          # Note: There's no way to cancel sleep in PicoRuby, so we just mark it
          @suspend_timer = nil
        end
      end

      # Suspend connection and subscriptions
      def suspend_connection
        return if @suspended
        return unless @suspend_timer
        @suspend_timer = nil
        @suspended = true
        puts "[Cable] Suspending connection (page in background)"
        disconnect
      end

      # Ensure connection is established
      def ensure_connected
        if @suspended
          @suspended = false
          puts "[Cable] Resuming connection (page visible)"
          connect
        end
      end
    end

    # Subscriptions manages a collection of Subscription instances
    class Subscriptions
      def initialize(consumer)
        @consumer = consumer
        @subscriptions = {}
      end

      # Create a new subscription
      # @param params [Hash] Channel parameters (e.g., {channel: "ChatChannel", room: "lobby"})
      # @yield [message] Block to handle incoming messages
      # @return [Subscription]
      def create(params, &block)
        puts "[Cable] Creating subscription with params: #{params.inspect}"
        identifier = JSON.generate(params)
        puts "[Cable] Generated identifier: #{identifier}"
        puts "[Cable] Identifier length: #{identifier.length}"
        subscription = Subscription.new(@consumer, identifier, params, &block)
        @subscriptions[identifier] = subscription
        subscription.subscribe
        subscription
      end

      # Find a subscription by identifier
      def find(identifier)
        @subscriptions[identifier]
      end

      # Remove a subscription
      def remove(subscription)
        @subscriptions.delete(subscription.identifier)
      end

      # Notify subscription confirmed
      def notify_subscription_confirmed(identifier)
        subscription = @subscriptions[identifier]
        return unless subscription
        subscription.notify_connected
      end

      # Notify subscription rejected
      def notify_subscription_rejected(identifier)
        subscription = @subscriptions[identifier]
        return unless subscription
        subscription.notify_rejected
      end

      # Notify message received
      def notify_message(identifier, message)
        subscription = @subscriptions[identifier]
        return unless subscription
        subscription.notify_received(message)
      end
    end

    # Subscription represents a subscription to a specific channel
    class Subscription
      attr_reader :consumer, :identifier, :params

      def initialize(consumer, identifier, params, &block)
        @consumer = consumer
        @identifier = identifier
        @params = params
        @callbacks = {
          received: block,
          connected: nil,
          disconnected: nil,
          rejected: nil
        }
      end

      # Subscribe to the channel
      def subscribe
        command = {
          command: "subscribe",
          identifier: @identifier
        }
        @consumer.send_command(command)
      end

      # Unsubscribe from the channel
      def unsubscribe
        command = {
          command: "unsubscribe",
          identifier: @identifier
        }
        @consumer.send_command(command)
        @consumer.subscriptions.remove(self)
      end

      # Perform an action on the channel
      # @param action [String] Action name
      # @param data [Hash] Additional data
      def perform(action, data = {})
        payload = data.merge(action: action)
        command = {
          command: "message",
          identifier: @identifier,
          data: JSON.generate(payload)
        }
        @consumer.send_command(command)
      end

      # Register connected callback
      def on_connected(&block)
        @callbacks[:connected] = block
      end

      # Register disconnected callback
      def on_disconnected(&block)
        @callbacks[:disconnected] = block
      end

      # Register rejected callback
      def on_rejected(&block)
        @callbacks[:rejected] = block
      end

      # Internal: notify subscription connected
      def notify_connected
        puts "[Cable] Subscription confirmed: #{@identifier}"
        @callbacks[:connected]&.call
      end

      # Internal: notify subscription rejected
      def notify_rejected
        puts "[Cable] Subscription rejected: #{@identifier}"
        @callbacks[:rejected]&.call
      end

      # Internal: notify message received
      def notify_received(message)
        @callbacks[:received]&.call(message)
      end
    end
  end
end
