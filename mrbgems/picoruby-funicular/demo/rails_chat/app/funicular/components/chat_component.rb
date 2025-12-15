class ChatComponent < Funicular::Component
  styles do
    # Layout
    layout "flex h-screen bg-gray-100"
    sidebar "w-64 bg-gray-800 text-white flex flex-col"
    main_content "flex-1 flex"

    # Sidebar components
    sidebar_header "p-4 bg-gray-900 flex justify-between items-center"
    sidebar_title "text-xl font-bold"
    settings_button "text-gray-400 hover:text-white"
    channels_list "flex-1 overflow-y-auto"

    channel_item base: "p-4 hover:bg-gray-700 cursor-pointer",
                 active: "bg-gray-700"
    channel_name "font-semibold"
    channel_desc "text-sm text-gray-400 truncate"

    # User info (sidebar footer)
    user_info "p-4 bg-gray-900 border-t border-gray-700"
    user_name "text-sm font-semibold"
    user_handle "text-xs text-gray-400"
    logout_button "mt-2 text-sm text-red-400 hover:text-red-300"

    # Chat area
    chat_container "flex-1 flex flex-col"
    chat_header "bg-white border-b border-gray-200 p-4"
    chat_title "text-xl font-bold text-gray-800"
    chat_subtitle "text-sm text-gray-600"

    messages_area "flex-1 overflow-y-auto p-4 space-y-4"
    loading "text-center text-gray-500"

    # Message
    message "flex items-start space-x-3"
    avatar_img "flex-shrink-0 w-10 h-10 rounded-full object-cover"
    avatar_placeholder "flex-shrink-0 w-10 h-10 bg-blue-500 rounded-full flex items-center justify-center text-white font-bold"

    message_content "flex-1"
    message_header "flex items-baseline space-x-2"
    message_author "font-semibold text-gray-900"
    message_time "text-xs text-gray-500"
    message_text "text-gray-800 mt-1"

    # Message input
    input_area "bg-white border-t border-gray-200 p-4"
    input_form "flex space-x-2"
    message_input "flex-1 px-4 py-2 border border-gray-300 rounded-lg focus:outline-none focus:ring-2 focus:ring-blue-500"
    send_button "px-6 py-2 bg-blue-600 text-white rounded-lg hover:bg-blue-700 font-semibold"

    # Empty state
    empty_state "flex-1 flex items-center justify-center text-gray-500"
  end

  def initialize_state
    {
      channels: [],
      current_channel: nil,
      messages: [],
      message_input: "",
      current_user: nil,
      loading: true,
      stats: []
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

  def component_updated
    update_chart if @chart && !state.stats.empty?
  end

  def component_will_unmount
    @subscription.unsubscribe if @subscription
    @stats_subscription.unsubscribe if @stats_subscription
    @chart.destroy if @chart
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
    patch(current_channel: channel, messages: [], loading: true, stats: [])

    # Subscribe to ActionCable channel
    if @subscription
      @subscription.unsubscribe
    end
    if @stats_subscription
      @stats_subscription.unsubscribe
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

    # Subscribe to StatsChannel
    @stats_subscription = consumer.subscriptions.create(
      { channel: "StatsChannel", channel_id: channel.id }
    ) do |data|
      case data["type"]
      when "stats_update"
        patch(stats: data["stats"])
        initialize_chart_if_needed
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

  def initialize_chart_if_needed
    return if @chart
    return unless @refs[:chart_canvas]
    return if state.stats.empty?

    canvas = @refs[:chart_canvas]

    config = {
      type: 'bar',
      data: {
        labels: [],
        datasets: [{
          label: 'Messages (1 year)',
          data: [],
          backgroundColor: 'rgba(59, 130, 246, 0.5)',
          borderColor: 'rgb(59, 130, 246)',
          borderWidth: 1
        }]
      },
      options: {
        responsive: true,
        maintainAspectRatio: false,
        scales: {
          y: {
            beginAtZero: true,
            ticks: {
              stepSize: 1
            }
          }
        },
        plugins: {
          legend: {
            display: false
          }
        }
      }
    }

    js_config = JS::Bridge.to_js(config)
    @chart = JS.global[:Chart].new(canvas, js_config)
    update_chart
  end

  def update_chart
    return unless @chart

    labels = state.stats.map { |s| s["username"] }
    data = state.stats.map { |s| s["count"] }

    @chart[:data][:labels] = JS::Bridge.to_js(labels)
    @chart[:data][:datasets][0][:data] = JS::Bridge.to_js(data)
    @chart.update("none")
  end

  def render
    div(class: s.layout) do
      # Sidebar - Channel list
      div(class: s.sidebar) do
        div(class: s.sidebar_header) do
          h2(class: s.sidebar_title) { "Channels" }
          button(onclick: -> { Funicular.router.navigate("/settings") }, class: s.settings_button) do
            span { "⚙️" }
          end
        end

        div(class: s.channels_list) do
          state.channels.each do |channel|
            is_active = state.current_channel && state.current_channel.id == channel.id
            div(
              onclick: -> { select_channel(channel) },
              class: s.channel_item(is_active)
            ) do
              div(class: s.channel_name) { "# #{channel.name}" }
              div(class: s.channel_desc) { channel.description }
            end
          end
        end

        if state.current_user
          div(class: s.user_info) do
            div(class: s.user_name) { state.current_user.display_name }
            div(class: s.user_handle) { "@#{state.current_user.username}" }
            button(onclick: :handle_logout, class: s.logout_button) do
              span { "Logout" }
            end
          end
        end
      end

      # Main content area (chat + stats)
      div(class: s.main_content) do
        # Chat area
        div(class: s.chat_container) do
        if state.current_channel
          # Chat header
          div(class: s.chat_header) do
            h3(class: s.chat_title) { "# #{state.current_channel.name}" }
            div(class: s.chat_subtitle) { state.current_channel.description }
          end

          # Messages area
          div(ref: :messages_container, class: s.messages_area) do
            if state.loading
              div(class: s.loading) { "Loading messages..." }
            else
              state.messages.each do |message|
                div(class: s.message) do
                  # Avatar
                  if message["user"]["has_avatar"]
                    img(src: "/users/#{message['user']['id']}/avatar", class: s.avatar_img)
                  else
                    div(class: s.avatar_placeholder) do
                      span { message["user"]["display_name"][0].upcase }
                    end
                  end
                  div(class: s.message_content) do
                    div(class: s.message_header) do
                      span(class: s.message_author) { message["user"]["display_name"] }
                      span(class: s.message_time) { message["created_at"] }
                    end
                    div(class: s.message_text) { message["content"] }
                  end
                end
              end
            end
          end

          # Message input
          div(class: s.input_area) do
            form(onsubmit: :handle_send_message, class: s.input_form) do
              input(
                type: "text",
                value: state.message_input,
                oninput: :handle_message_input,
                placeholder: "Type a message...",
                class: s.message_input
              )
              button(
                type: "submit",
                class: s.send_button
              ) do
                span { "Send" }
              end
            end
          end
        else
          div(class: s.empty_state) do
            span { "Select a channel to start chatting" }
          end
        end
        end

        # Stats chart area
        if state.current_channel
          div(class: "w-80 bg-white border-l border-gray-200 p-4 flex flex-col") do
            h3(class: "text-lg font-bold text-gray-800 mb-4") { "Activity (1 year)" }

            div(class: "flex-1 flex items-center justify-center") do
              if state.stats.empty?
                div(class: "text-center text-gray-500") do
                  span { "No messages yet" }
                end
              else
                div(class: "w-full h-64") do
                  canvas(ref: :chart_canvas)
                end
              end
            end

            div(class: "mt-4 text-xs text-gray-500 text-center") do
              span { "Messages per user in the last year" }
            end
          end
        end
      end
    end
  end
end

