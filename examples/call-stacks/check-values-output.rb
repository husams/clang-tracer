# Compare optional text emitted inside hooks against typed normal CLI events.
directory, arguments, result, expected_count = ARGV
expected_count = Integer(expected_count || '24')
want_arguments = arguments == 'true'
want_result = result == 'true'
values = {}
File.foreach(File.join(directory, 'target.log')) do |line|
  match = line.match(/\A(PRE|POST) probe=(\d+) call=(\d+) (args=\[.*\]|result=.*)\n?\z/)
  next unless match
  phase, probe, call, text = match.captures
  raise 'arguments printed while disabled or in POST' if text.start_with?('args=') && (!want_arguments || phase != 'PRE')
  raise 'result printed while disabled or in PRE' if text.start_with?('result=') && (!want_result || phase != 'POST')
  key = [probe, call, phase.downcase]
  raise 'duplicate value line' if values.key?(key)
  values[key] = text
end
headers = File.readlines(File.join(directory, 'events.log')).grep(/^cursor=/)
raise 'incorrect event count' unless headers.size == expected_count
checked = 0
headers.each do |header|
  key = [header[/probe=(\d+)/,1], header[/call=(\d+)/,1], header[/phase=(pre|post)/,1]]
  expected = key[2] == 'pre' ? header[/args=\[.*?\]/] : header[/result=\S+/]
  enabled = key[2] == 'pre' ? want_arguments : want_result
  if enabled
    raise "callback values differ from original event #{key}" unless values.fetch(key) == expected
    checked += 1
  else
    raise "unexpected optional line #{key}" if values.key?(key)
  end
  raise 'lost event' unless header.include?('lost(agent=0,server=0)')
end
puts "values: arguments=#{want_arguments}, result=#{want_result}, #{checked} exact hook/event value matches; disabled phases emit no values"
