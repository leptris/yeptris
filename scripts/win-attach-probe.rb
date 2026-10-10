# win-attach-probe.rb — the #504 mingw hang gate: require yeptris and
# round-trip a document. Exits 0 on success; a hang never exits and the
# caller's hard timeout convicts the version.
$stderr.sync = true
puts "attach: requiring yeptris..."
require "yeptris"
require "yeptris/yaml"
puts "attach: OK (#{Yeptris::VERSION})"
doc = Yeptris::YAML.load("a: 1\nb: [x, y]\n")
raise "load mismatch: #{doc.inspect}" unless doc == { "a" => 1, "b" => %w[x y] }
puts "roundtrip: OK (#{Yeptris::YAML.dump(doc).inspect})"
