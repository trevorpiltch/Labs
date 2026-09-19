require "test_helper"

class SupportRequestMailerTest < ActionMailer::TestCase
  test "respond" do
    support_request = SupportRequest.last
    mail = SupportRequestMailer.respond(support_request)
    assert_equal "Re: Respond", mail.subject
    assert_equal ["test@example.com"], mail.to
    assert_match "Hi", mail.body.encoded
  end

end
