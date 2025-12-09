class ChatComponent < Funicular::Component
  def initialize_state
    {
      channels: [],
      current_channel: nil,
      messages: [],
      message_input: "",
      current_user: nil,
      loading: true
    }
  end

  def component_mounted
    # Check if logged in using Session model
    Session.current_user do |user, error|
      if error
        Funicular.router.navigate("/login")
      else
        patch(current_user: user)
        load_channels
      end
    end
  end

  def load_channels
    # Load channels using Channel model
    Channel.all do |channels, error|
      if error
        patch(loading: false)
      else
        patch(channels: channels, loading: false)
        if channels.size > 0 && !state.current_channel
          select_channel(channels[0])
        end
      end
    end
  end

  def select_channel(channel)
    patch(current_channel: channel, messages: [], loading: true)

    # Subscribe to ActionCable channel
    if @subscription
      @subscription.unsubscribe
    end

    consumer = Funicular::Cable.create_consumer("/cable")
    @subscription = consumer.subscriptions.create(
      { channel: "ChatChannel", channel_id: channel.id }
    ) do |data|
      case data["type"]
      when "initial_messages"
        patch(messages: data["messages"], loading: false)
        scroll_to_bottom
      when "new_message"
        messages = state.messages + [data["message"]]
        patch(messages: messages)
        scroll_to_bottom
      end
    end
  end

  def scroll_to_bottom
    sleep 0.1
    if @refs[:messages_container]
      container = @refs[:messages_container]
      container[:scrollTop] = container[:scrollHeight]
    end
  end

  def handle_message_input(event)
    patch(message_input: event.target[:value])
  end

  def handle_send_message(event)
    event.preventDefault

    if state.message_input.empty?
      return
    end

    content = state.message_input
    patch(message_input: "")

    @subscription.perform("send_message", { content: content })
  end

  def handle_logout(event)
    # Logout using Session model
    Session.logout do |success, error|
      Funicular.router.navigate("/login")
    end
  end

  def render
    div(class: "flex h-screen bg-gray-100") do
      # Sidebar - Channel list
      div(class: "w-64 bg-gray-800 text-white flex flex-col") do
        div(class: "p-4 bg-gray-900 flex justify-between items-center") do
          h2(class: "text-xl font-bold") { "Channels" }
          button(onclick: -> { Funicular.router.navigate("/settings") }, class: "text-gray-400 hover:text-white") do
            span { "⚙️" }
          end
        end

        div(class: "flex-1 overflow-y-auto") do
          state.channels.each do |channel|
            div(
              onclick: -> { select_channel(channel) },
              class: "p-4 hover:bg-gray-700 cursor-pointer #{state.current_channel && state.current_channel.id == channel.id ? 'bg-gray-700' : ''}"
            ) do
              div(class: "font-semibold") { "# #{channel.name}" }
              div(class: "text-sm text-gray-400 truncate") { channel.description }
            end
          end
        end

        if state.current_user
          div(class: "p-4 bg-gray-900 border-t border-gray-700") do
            div(class: "text-sm font-semibold") { state.current_user.display_name }
            div(class: "text-xs text-gray-400") { "@#{state.current_user.username}" }
            button(onclick: :handle_logout, class: "mt-2 text-sm text-red-400 hover:text-red-300") do
              span { "Logout" }
            end
          end
        end
      end

      # Main chat area
      div(class: "flex-1 flex flex-col") do
        if state.current_channel
          # Chat header
          div(class: "bg-white border-b border-gray-200 p-4") do
            h3(class: "text-xl font-bold text-gray-800") { "# #{state.current_channel.name}" }
            div(class: "text-sm text-gray-600") { state.current_channel.description }
          end

          # Messages area
          div(ref: :messages_container, class: "flex-1 overflow-y-auto p-4 space-y-4") do
            if state.loading
              div(class: "text-center text-gray-500") { "Loading messages..." }
            else
              state.messages.each do |message|
                div(class: "flex items-start space-x-3") do
                  # Avatar
                  if message["user"]["has_avatar"]
                    img(src: "/users/#{message['user']['id']}/avatar", class: "flex-shrink-0 w-10 h-10 rounded-full object-cover")
                  else
                    div(class: "flex-shrink-0 w-10 h-10 bg-blue-500 rounded-full flex items-center justify-center text-white font-bold") do
                      span { message["user"]["display_name"][0].upcase }
                    end
                  end
                  div(class: "flex-1") do
                    div(class: "flex items-baseline space-x-2") do
                      span(class: "font-semibold text-gray-900") { message["user"]["display_name"] }
                      span(class: "text-xs text-gray-500") { message["created_at"] }
                    end
                    div(class: "text-gray-800 mt-1") { message["content"] }
                  end
                end
              end
            end
          end

          # Message input
          div(class: "bg-white border-t border-gray-200 p-4") do
            form(onsubmit: :handle_send_message, class: "flex space-x-2") do
              input(
                type: "text",
                value: state.message_input,
                oninput: :handle_message_input,
                placeholder: "Type a message...",
                class: "flex-1 px-4 py-2 border border-gray-300 rounded-lg focus:outline-none focus:ring-2 focus:ring-blue-500"
              )
              button(
                type: "submit",
                class: "px-6 py-2 bg-blue-600 text-white rounded-lg hover:bg-blue-700 font-semibold"
              ) do
                span { "Send" }
              end
            end
          end
        else
          div(class: "flex-1 flex items-center justify-center text-gray-500") do
            span { "Select a channel to start chatting" }
          end
        end
      end
    end
  end
end

