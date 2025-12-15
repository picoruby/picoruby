class MessageComponent < Funicular::Component
  styles do
    message "flex items-start space-x-3"
    avatar_img "flex-shrink-0 w-10 h-10 rounded-full object-cover"
    avatar_placeholder "flex-shrink-0 w-10 h-10 bg-blue-500 rounded-full flex items-center justify-center text-white font-bold"

    message_content "flex-1"
    message_header "flex items-baseline space-x-2"
    message_author "font-semibold text-gray-900"
    message_time "text-xs text-gray-500"
    message_text "text-gray-800 mt-1"
  end

  def render
    div(class: s.message) do
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
        end
        div(class: s.message_text) { props[:message]["content"] }
      end
    end
  end
end
