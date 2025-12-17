class MessagesController < ApplicationController
  before_action :require_login

  def destroy
    message = Message.find(params[:id])

    # Only allow users to delete their own messages
    if message.user_id != current_user.id
      render json: { error: "Unauthorized" }, status: :forbidden
      return
    end

    message.destroy
    render json: { success: true }
  rescue ActiveRecord::RecordNotFound
    render json: { error: "Message not found" }, status: :not_found
  end
end
