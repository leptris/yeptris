require "json"
psych = JSON.parse(`ruby dump_psych.rb #{ARGV.join(" ")}`)
yep = JSON.parse(`./dump_yeptris #{ARGV.map { |a| File.expand_path(a) }.join(" ")}`)
bad = 0
psych.each do |k, pe|
  ye = yep[File.expand_path(k)] || []
  puts "== #{k}  (psych #{pe.size}, yep #{ye.size})"
  [pe.size, ye.size].max.times do |i|
    a, b = pe[i], ye[i]
    if a && b
      puts format("  %-16s psych %s  yep %s%s", a[0], a[1..].inspect, b[1..].inspect, a == b ? "" : "   <<<")
      bad += 1 unless a == b
    else
      puts "  MISSING psych=#{a.inspect} yep=#{b.inspect}"
      bad += 1
    end
  end
end
puts bad.zero? ? "ALL MATCH" : "#{bad} MISMATCHES"
