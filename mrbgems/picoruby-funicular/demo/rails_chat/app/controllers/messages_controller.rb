class MessagesController < ApplicationController
  before_action :require_login

  def destroy
    message = Message.find(params[:id])

    # Only allow users to delete their own messages
    if message.user_id != current_user.id
      render json: { error: "Unauthorized" }, status: :forbidden
      return
    end

    channel_id = message.channel_id
    if message.destroy
      ActionCable.server.broadcast "chat_#{channel_id}", {
        type: "delete_message",
        message_id: message.id
      }
      render json: { success: true }
    else
      render json: { error: "Failed to delete message" }, status: :unprocessable_entity
    end
  rescue ActiveRecord::RecordNotFound
    render json: { error: "Message not found" }, status: :not_found
  end
end
