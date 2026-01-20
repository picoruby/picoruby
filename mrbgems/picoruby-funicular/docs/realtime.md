# ActionCable-compatible WebSocket

Bring real-time features to your application with Funicular's built-in, ActionCable-compatible WebSocket client. It allows your Pure Ruby frontend to communicate seamlessly with your Rails backend for live updates, notifications, chat, and more.

## Table of Contents

- [Overview](#overview)
- [Creating a Consumer](#creating-a-consumer)
- [Subscribing to Channels](#subscribing-to-channels)
- [Sending Messages](#sending-messages)
- [Handling Disconnection](#handling-disconnection)
- [Unsubscribing](#unsubscribing)
- [Complete Example: Chat Application](#complete-example-chat-application)
- [Best Practices](#best-practices)

## Overview

The `Funicular::Cable` module provides a simple API to create consumers and subscribe to channels, handling all the complexities of the ActionCable protocol, including automatic reconnection.

### Protocol Compatibility

Funicular's WebSocket client is fully compatible with Rails ActionCable, supporting:
- Channel subscriptions
- Message broadcasting
- Channel actions (perform)
- Automatic reconnection
- Ping/pong keepalive

## Creating a Consumer

Create a consumer for your Rails app's cable endpoint:

```ruby
class ChatComponent < Funicular::Component
  def component_mounted
    # Create consumer pointing to Rails ActionCable endpoint
    @consumer = Funicular::Cable.create_consumer("/cable")

    # Subscribe to channels (see next section)
  end

  def component_unmounted
    # Clean up (disconnect) when component is unmounted
    @consumer&.disconnect
  end
end
```

### Consumer URL

The URL can be:
- Relative: `/cable` (uses current host)
- Absolute: `ws://localhost:3000/cable` or `wss://example.com/cable`

## Subscribing to Channels

Subscribe to a channel and provide a callback to handle incoming messages:

```ruby
class ChatComponent < Funicular::Component
  def component_mounted
    @consumer = Funicular::Cable.create_consumer("/cable")

    # Subscribe to ChatChannel with room parameter
    @subscription = @consumer.subscriptions.create(
      channel: "ChatChannel",
      room: "lobby"
    ) do |message|
      # This block is called whenever a message is received from the server
      handle_message(message)
    end
  end

  def handle_message(message)
    puts "New message: #{message}"
    patch(messages: state.messages + [message])
  end
end
```

### Channel Parameters

Pass parameters to identify the channel subscription:

```ruby
# Single room chat
@subscription = @consumer.subscriptions.create(
  channel: "ChatChannel",
  room: "general"
) do |message|
  # Handle message
end

# User-specific notifications
@subscription = @consumer.subscriptions.create(
  channel: "NotificationChannel",
  user_id: current_user.id
) do |notification|
  patch(notifications: state.notifications + [notification])
end

# Post comments
@subscription = @consumer.subscriptions.create(
  channel: "CommentsChannel",
  post_id: props[:post_id]
) do |comment|
  patch(comments: state.comments + [comment])
end
```

### Rails Backend (ActionCable Channel)

On the Rails side, define the corresponding channel:

```ruby
# app/channels/chat_channel.rb
class ChatChannel < ApplicationCable::Channel
  def subscribed
    stream_from "chat_#{params[:room]}"
  end

  def unsubscribed
    # Cleanup
  end

  def speak(data)
    # Broadcast message to all subscribers
    ActionCable.server.broadcast(
      "chat_#{params[:room]}",
      {
        user: current_user.username,
        content: data["message"],
        timestamp: Time.now.iso8601
      }
    )
  end
end
```

## Sending Messages

Use the `perform` method to send messages/actions to the server:

```ruby
class ChatComponent < Funicular::Component
  def handle_send_message
    # Send "speak" action to ChatChannel
    @subscription.perform("speak", message: state.message_input)

    # Clear input
    patch(message_input: "")
  end

  def render
    div do
      # Message list
      state.messages.each do |msg|
        div { "#{msg['user']}: #{msg['content']}" }
      end

      # Input form
      input(
        value: state.message_input,
        oninput: ->(e) { patch(message_input: e.target[:value]) }
      )
      button(onclick: -> { handle_send_message }) { "Send" }
    end
  end
end
```

### Action Parameters

The `perform` method sends an action to the Rails channel:

```ruby
# Simple action
@subscription.perform("speak", message: "Hello!")

# Action with multiple parameters
@subscription.perform("update_typing_status", is_typing: true, user_id: 123)

# Action with complex data
@subscription.perform("edit_message", {
  message_id: 456,
  content: "Updated content",
  edited_at: Time.now.to_s
})
```

The Rails channel receives these as method calls:

```ruby
class ChatChannel < ApplicationCable::Channel
  def speak(data)
    # data = { "message" => "Hello!" }
    ActionCable.server.broadcast(...)
  end

  def update_typing_status(data)
    # data = { "is_typing" => true, "user_id" => 123 }
  end
end
```

## Handling Disconnection

The consumer automatically reconnects when the connection is lost. You can handle connection events:

```ruby
class ChatComponent < Funicular::Component
  def component_mounted
    @consumer = Funicular::Cable.create_consumer("/cable")

    # The consumer automatically reconnects on disconnection
    # You can track connection state if needed
    patch(connected: true)

    @subscription = @consumer.subscriptions.create(
      channel: "ChatChannel",
      room: "lobby"
    ) do |message|
      # Ensure we're still connected when handling messages
      if state.connected
        handle_message(message)
      end
    end
  end

  def component_unmounted
    patch(connected: false)
    @consumer&.disconnect
  end
end
```

## Unsubscribing

Unsubscribe from a channel when no longer needed:

```ruby
class MultiChannelComponent < Funicular::Component
  def switch_channel(new_room)
    # Unsubscribe from old channel
    @subscription&.unsubscribe

    # Subscribe to new channel
    @subscription = @consumer.subscriptions.create(
      channel: "ChatChannel",
      room: new_room
    ) do |message|
      handle_message(message)
    end

    patch(current_room: new_room)
  end

  def component_unmounted
    # Clean up subscriptions
    @subscription&.unsubscribe
    @consumer&.disconnect
  end
end
```

## Complete Example: Chat Application

```ruby
class ChatComponent < Funicular::Component
  def initialize_state
    {
      messages: [],
      message_input: "",
      current_room: "lobby",
      online_users: [],
      connected: false
    }
  end

  def component_mounted
    # Create WebSocket consumer
    @consumer = Funicular::Cable.create_consumer("/cable")

    # Subscribe to chat channel
    @chat_subscription = @consumer.subscriptions.create(
      channel: "ChatChannel",
      room: state.current_room
    ) do |message|
      handle_chat_message(message)
    end

    # Subscribe to presence channel
    @presence_subscription = @consumer.subscriptions.create(
      channel: "PresenceChannel",
      room: state.current_room
    ) do |data|
      handle_presence_update(data)
    end

    patch(connected: true)
  end

  def component_unmounted
    @chat_subscription&.unsubscribe
    @presence_subscription&.unsubscribe
    @consumer&.disconnect
    patch(connected: false)
  end

  def handle_chat_message(message)
    patch(messages: state.messages + [message])
  end

  def handle_presence_update(data)
    patch(online_users: data["users"])
  end

  def handle_send_message
    return if state.message_input.strip.empty?

    @chat_subscription.perform("speak", {
      message: state.message_input,
      user: current_user.username
    })

    patch(message_input: "")
  end

  def handle_message_input(event)
    new_value = event.target[:value]
    patch(message_input: new_value)

    # Send typing indicator
    @chat_subscription.perform("typing", {
      user: current_user.username,
      is_typing: !new_value.empty?
    })
  end

  def render
    div(class: "chat-container") do
      # Header
      div(class: "chat-header") do
        h2 { "Chat Room: #{state.current_room}" }
        span(class: state.connected ? "status-online" : "status-offline") do
          state.connected ? "Connected" : "Disconnected"
        end
      end

      # Online users
      div(class: "online-users") do
        h3 { "Online (#{state.online_users.length})" }
        state.online_users.each do |user|
          div(class: "user") { user }
        end
      end

      # Message list
      div(class: "messages", ref: :message_list) do
        state.messages.each do |msg|
          div(class: "message", key: msg["id"]) do
            strong { "#{msg['user']}: " }
            span { msg["content"] }
            small(class: "timestamp") { msg["timestamp"] }
          end
        end
      end

      # Input form
      div(class: "message-input") do
        input(
          value: state.message_input,
          oninput: :handle_message_input,
          placeholder: "Type a message...",
          class: "input"
        )
        button(
          onclick: -> { handle_send_message },
          disabled: state.message_input.strip.empty? || !state.connected,
          class: "send-button"
        ) { "Send" }
      end
    end
  end

  def current_user
    # Access current user from session or props
    @current_user ||= props[:current_user]
  end
end
```

### Rails Backend

```ruby
# app/channels/chat_channel.rb
class ChatChannel < ApplicationCable::Channel
  def subscribed
    stream_from "chat_#{params[:room]}"
    PresenceChannel.broadcast_presence(params[:room])
  end

  def unsubscribed
    PresenceChannel.broadcast_presence(params[:room])
  end

  def speak(data)
    ActionCable.server.broadcast(
      "chat_#{params[:room]}",
      {
        id: SecureRandom.uuid,
        user: current_user.username,
        content: data["message"],
        timestamp: Time.now.iso8601
      }
    )
  end

  def typing(data)
    ActionCable.server.broadcast(
      "chat_#{params[:room]}_typing",
      {
        user: data["user"],
        is_typing: data["is_typing"]
      }
    )
  end
end

# app/channels/presence_channel.rb
class PresenceChannel < ApplicationCable::Channel
  def subscribed
    stream_from "presence_#{params[:room]}"
    add_user_to_room
  end

  def unsubscribed
    remove_user_from_room
  end

  def self.broadcast_presence(room)
    users = # ... fetch online users for room
    ActionCable.server.broadcast(
      "presence_#{room}",
      { users: users.map(&:username) }
    )
  end

  private

  def add_user_to_room
    # Track user presence
    PresenceChannel.broadcast_presence(params[:room])
  end

  def remove_user_from_room
    # Remove user from tracking
    PresenceChannel.broadcast_presence(params[:room])
  end
end
```

## Best Practices

### 1. Clean Up Subscriptions

Always unsubscribe and disconnect in `component_unmounted`:

```ruby
def component_unmounted
  @subscription&.unsubscribe
  @consumer&.disconnect
end
```

### 2. Handle Connection State

Provide feedback when disconnected:

```ruby
def render
  if !state.connected
    div(class: "alert") { "Reconnecting..." }
  end

  # Rest of UI
end
```

### 3. Use Specific Channels

Create focused channels instead of one large channel:

```ruby
# ✅ Good: Specific channels
@consumer.subscriptions.create(channel: "ChatChannel", room: "lobby")
@consumer.subscriptions.create(channel: "NotificationChannel", user_id: user.id)

# ❌ Avoid: Generic catch-all channel
@consumer.subscriptions.create(channel: "AppChannel")
```

### 4. Validate Data on Server

Never trust client data:

```ruby
# Rails channel
def speak(data)
  # ✅ Validate and sanitize
  message = data["message"].to_s.strip[0..1000]  # Limit length
  return if message.empty?

  ActionCable.server.broadcast(...)
end
```

### 5. Use Keys for Dynamic Lists

When rendering messages from WebSocket, use `key` prop:

```ruby
state.messages.map do |msg|
  div(key: msg["id"]) { msg["content"] }  # Efficient re-rendering
end
```

### 6. Debounce Typing Indicators

```ruby
def handle_typing
  # Clear existing timer
  JS.global.clearTimeout(@typing_timer) if @typing_timer

  # Send typing=true
  @subscription.perform("typing", is_typing: true)

  # Set timer to send typing=false after 1s of inactivity
  @typing_timer = JS.global.setTimeout(1000) do
    @subscription.perform("typing", is_typing: false)
  end
end
```
