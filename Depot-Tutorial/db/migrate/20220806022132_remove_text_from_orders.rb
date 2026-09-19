class RemoveTextFromOrders < ActiveRecord::Migration[6.1]
  def up 
    remove_column :orders, :text
  end 

  def down 
    add_column :orders, :text
  end 
end
