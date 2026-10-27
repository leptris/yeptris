# win-attach-probe.rb — #504 layer bisect. argv[0] selects the layer:
#   attach    bare require + roundtrip
#   model     + require "lutaml/model" (the expressir path)
#   cli       + Expressir::Cli.start(%w(version)) — the exact hung call
$stderr.sync = true
layer = ARGV[0] || "attach"
puts "layer=#{layer} attach: requiring yeptris..."
require "yeptris"
require "yeptris/yaml"
puts "layer=#{layer} attach: OK (#{Yeptris::VERSION})"
doc = Yeptris::YAML.load("a: 1\nb: [x, y]\n")
raise "load mismatch" unless doc == { "a" => 1, "b" => %w[x y] }
puts "layer=#{layer} roundtrip: OK"
if layer == "model" || layer == "cli"
  puts "layer=#{layer} requiring lutaml/model..."
  require "lutaml/model"
  puts "layer=#{layer} lutaml/model: OK"
end
if layer == "cli"
  puts "layer=#{layer} requiring expressir + cli start..."
  require "expressir"
  require "expressir/cli"
  Expressir::Cli.start(%w(version))
  puts "layer=#{layer} expressir cli: OK"
end
puts "layer=#{layer} DONE"
