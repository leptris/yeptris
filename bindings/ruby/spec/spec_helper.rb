# frozen_string_literal: true

# In-repo development default: test against the local build.
ENV["YEPTRIS_LIB_PATH"] ||= File.expand_path(
  "../../../build-validate/src/libyeptris.dylib", __dir__
)

require "yeptris"

RSpec.configure do |config|
  config.expect_with :rspec do |c|
    c.syntax = :expect
  end
  config.disable_monkey_patching!
  config.order = :random
end
