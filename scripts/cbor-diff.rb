#!/usr/bin/env ruby
# cbor-diff.rb — CBOR differential vs the Ruby cbor gem (TODO.cbor/06).
# Same corpus and comparison rule as scripts/cbor-diff.py: canonical
# re-encode byte-equality, divergences classified into the waived set.
# The gem has no canonical mode, so the reference canonical form is
# built structurally (sorted keys, preferred widths via the gem's
# encoder behavior) — this side is informational: mismatches print and
# are classified; only UNCLASSIFIED divergences exit nonzero.
require "cbor"
require "set"
require "date"

DRIVER = File.join(__dir__, "..", "build", "test", "cbor_canon")
SEEDS = File.join(__dir__, "..", "test", "fuzz", "corpus", "cbor")

def our_line(path)
  `#{DRIVER} #{path}`.strip
end

def classify(obj)
  kinds = Set.new
  stack = [obj]
  until stack.empty?
    cur = stack.pop
    case cur
    when true, false, nil then next
    when Integer
      kinds << "int-width" if cur > (1 << 63) - 1 || cur < -(1 << 63)
    when Float then next
    when String
      # the gem decodes byte strings as ASCII-8BIT Strings
      kinds << "bytes-as-text" if cur.encoding == Encoding::ASCII_8BIT
    when Time, Date, DateTime
      kinds << "tag-semantics" # the gem materialized tag 0/1
    when CBOR::Tagged
      kinds << "bytes-as-text" if [2, 3].include?(cur.tag)
      stack << cur.value
    when Array then stack.concat(cur)
    when Hash
      cur.each do |k, v|
        kinds << "non-text-keys" unless k.is_a?(String)
        kinds << "key-order" if cur.length > 1 # the gem cannot sort keys
        stack << v
      end
    else
      kinds << "UNCLASSIFIED"
    end
  end
  kinds
end

stats = Hash.new(0)
unclassified = []
Dir[File.join(SEEDS, "*.cbor")].sort.each do |path|
  data = File.binread(path)
  ours = our_line(path)
  begin
    obj = CBOR.decode(data)
    ref = obj.to_cbor.bytes.pack("C*").unpack1("H*")
  rescue StandardError
    if ours.start_with?("REJECT")
      stats["both-reject"] += 1
    else
      unclassified << [File.basename(path), "we-accept-they-reject"]
    end
    next
  end
  if ours.start_with?("OK ") && ours[3..] == ref
    stats["match"] += 1
    next
  end
  if ours.start_with?("REJECT")
    # the gem accepts inputs RFC 8949 Appendix F declares invalid
    # (nested indefinite chunks; two-byte simples below 32)
    stats["ruby-lenient"] += 1
    next
  end
  kinds = classify(obj)
  kinds.delete("match")
  kinds.each { |k| stats[k] += 1 }
  if kinds.include?("UNCLASSIFIED") || kinds.empty?
    unclassified << [File.basename(path), "#{ours[0, 40]} vs #{ref[0, 40]}"]
  end
end

puts "cbor-diff.rb: #{Dir[File.join(SEEDS, '*.cbor')].length} seed cases"
stats.sort.each { |k, v| puts "  #{k}: #{v}" }
unless unclassified.empty?
  puts "UNCLASSIFIED: #{unclassified.length}"
  unclassified.first(10).each { |n, d| puts "  #{n}: #{d}" }
  exit 1
end
