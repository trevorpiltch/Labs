# Load the Rails application.
require_relative "application"
require "i18n-js"

# Initialize the Rails application.
Rails.application.initialize!

Rails.application.configure do 
    config.action_mailer.delivery_method = :smtp
    config.action_mailer.smtp_settings = {
        address: "smpt.gmail.com",
        port: 587, 
        domain: "piltch.com", 
        authentication: "plain", 
        user_name: "trevor@piltch.com", 
        password: "secret", 
        enablestartls_auto: true
    }
    # config.middleware.use I18nJS
end 