# stdlib psych's per-event marks — the ground truth for the engine's
# end marks. Prints JSON: fixture -> [[callback, sl, sc, el, ec], ...]
require "psych"
require "json"

class MarkDumper < Psych::Handler
  attr_reader :marks
  def initialize
    @marks = []
    @cur = nil
  end
  def event_location(sl, sc, el, ec)
    @cur = [sl, sc, el, ec]
  end
  Psych::Handler.instance_methods(false).each do |m|
    next if m == :event_location
    define_method(m) { |*args| @marks << [m.to_s, *@cur]; @cur = nil }
  end
end

out = {}
ARGV.each do |path|
  d = MarkDumper.new
  parser = Psych::Parser.new(d)
  begin
    parser.parse(File.read(path))
    out[File.basename(path)] = d.marks
  rescue => e
    out[File.basename(path)] = [["ERROR", e.class.name, e.message]]
  end
end
puts JSON.pretty_generate(out)
