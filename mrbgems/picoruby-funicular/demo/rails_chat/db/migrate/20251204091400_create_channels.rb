class CreateChannels < ActiveRecord::Migration[8.1]
  def change
    create_table :channels do |t|
      t.string :name, null: false
      t.text :description

      t.timestamps
    end
    add_index :channels, :name, unique: true
  end
end
