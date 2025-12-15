class MessageListComponent < Funicular::Component
  styles do
    chat_container "flex-1 flex flex-col"
    chat_header "bg-white border-b border-gray-200 p-4"
    chat_title "text-xl font-bold text-gray-800"
    chat_subtitle "text-sm text-gray-600"

    messages_area "flex-1 overflow-y-auto p-4 space-y-4"
    loading "text-center text-gray-500"

    input_area "bg-white border-t border-gray-200 p-4"
    input_form "flex space-x-2"
    message_input "flex-1 px-4 py-2 border border-gray-300 rounded-lg focus:outline-none focus:ring-2 focus:ring-blue-500"
    send_button "px-6 py-2 bg-blue-600 text-white rounded-lg hover:bg-blue-700 font-semibold"

    empty_state "flex-1 flex items-center justify-center text-gray-500"
  end

  def component_updated
    scroll_to_bottom if props[:messages] && !props[:messages].empty?
  end

  def scroll_to_bottom
    sleep 0.1
    if @refs[:messages_container]
      container = @refs[:messages_container]
      container[:scrollTop] = container[:scrollHeight]
    end
  end

  def render
    div(class: s.chat_container) do
      if props[:current_channel]
        # Chat header
        div(class: s.chat_header) do
          h3(class: s.chat_title) { "# #{props[:current_channel].name}" }
          div(class: s.chat_subtitle) { props[:current_channel].description }
        end

        # Messages area
        div(ref: :messages_container, class: s.messages_area) do
          if props[:loading]
            div(class: s.loading) { "Loading messages..." }
          else
            props[:messages].each do |message|
              component(MessageComponent, { message: message })
            end
          end
        end

        # Message input
        div(class: s.input_area) do
          form(onsubmit: props[:on_send_message], class: s.input_form) do
            input(
              type: "text",
              value: props[:message_input],
              oninput: props[:on_message_input],
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
  end
end
