class MessageComponent < Funicular::Component
  styles do
    message "flex items-start space-x-3 overflow-hidden transition-[opacity,max-height] duration-500 ease-out opacity-100 max-h-screen"
    avatar_img "flex-shrink-0 w-10 h-10 rounded-full object-cover"
    avatar_placeholder "flex-shrink-0 w-10 h-10 bg-blue-500 rounded-full flex items-center justify-center text-white font-bold"

    message_content "flex-1"
    message_header "flex items-baseline space-x-2"
    message_author "font-semibold text-gray-900"
    message_time "text-xs text-gray-500"
    message_text "text-gray-800 mt-1"

    delete_button "ml-2 text-xs text-red-500 hover:text-red-700 cursor-pointer"
  end

  def render
    div(class: s.message, id: "message-#{props[:message]['id']}") do
      # Avatar
      if props[:message]["user"]["has_avatar"]
        img(src: "/users/#{props[:message]['user']['id']}/avatar", class: s.avatar_img)
      else
        div(class: s.avatar_placeholder) do
          span { props[:message]["user"]["display_name"][0].upcase }
        end
      end

      div(class: s.message_content) do
        div(class: s.message_header) do
          span(class: s.message_author) { props[:message]["user"]["display_name"] }
          span(class: s.message_time) { props[:message]["created_at"] }

          # Show delete button only for own messages
          if props[:current_user] && props[:current_user].id == props[:message]["user"]["id"]
            link_to "/messages/#{props[:message]['id']}", method: :delete, class: s.delete_button do
              span { "Delete" }
            end
          end
        end
        div(class: s.message_text) { props[:message]["content"] }
      end
    end
  end

  # Override handle_link_response to handle successful deletion
  def handle_link_response(response, path, method)
    if method.to_s.downcase.to_sym == :delete && !response.error?
      # Extract message ID from path (e.g., "/messages/123")
      message_id = path.split('/').last.to_i
      # The parent component will receive the delete event via Action Cable
      # and handle the UI update.
      # props[:on_delete].call(message_id) if props[:on_delete]
    end
    super
  end
end
