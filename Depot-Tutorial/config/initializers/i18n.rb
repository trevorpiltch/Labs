# encoding utf-8
I18n.default_locale = :en

LANGUAGES = [
    ['English', 'en'], 
    ['Espa&ntilde;ol'.html_safe, 'es']
]

Rails.application.config.after_initialize do
  require "i18n-js/listen"
  # This will only run in development.
  I18nJS.listen
end