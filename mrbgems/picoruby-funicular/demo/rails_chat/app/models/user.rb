class User < ApplicationRecord
  has_secure_password

  has_many :messages, dependent: :destroy

  validates :username, presence: true, uniqueness: true
  validates :display_name, presence: true

  before_validation :set_default_display_name

  # Prevent binary avatar data from being logged
  def inspect
    attrs = attributes.except("avatar").map do |k, v|
      "#{k}: #{v.inspect}"
    end
    avatar_info = avatar.present? ? "avatar: <#{avatar.bytesize} bytes>" : "avatar: nil"
    "#<User #{attrs.join(', ')}, #{avatar_info}>"
  end

  private

  def set_default_display_name
    self.display_name ||= username
  end
end
