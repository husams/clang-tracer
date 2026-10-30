# Analyze output produced by public CLI commands; never controls the target.
directory, mode = ARGV
lines = File.readlines(File.join(directory, 'events.log'))
events = []
lines.each do |line|
  if line.start_with?('cursor=')
    events << {header: line, frames: []}
  elsif line.include?('entry-stack ')
    events.last[:stack] = line
  elsif line.match?(/^    #/)
    events.last[:frames] << line
  end
end
raise 'expected 24 events' unless events.size == 24
raise 'event loss' unless events.all? { |e| e[:header].include?('lost(agent=0,server=0)') }
tids = events.map { |e| e[:header][/tid=(\d+)/, 1] }.uniq
raise 'expected three native threads' unless tids.size == 3
get = ->(header, name) { Integer(header[/\b#{name}=int32:(-?\d+)/, 1]) }
pairs = {}
completed = Hash.new(0)
events.each do |e|
  h = e[:header]
  method = h.include?('this=0x')
  probe = h[/probe=(\d+)/, 1]
  key = [probe, h[/call=(\d+)/, 1], h[/tid=(\d+)/, 1]]
  if h.include?('phase=pre')
    pairs[key] = e
  else
    pre = pairs.delete(key)
    raise 'unpaired POST' unless pre
    if method
      raise 'member update' unless get.call(h, 'calls') == get.call(pre[:header], 'calls') + 1
      expected = get.call(h, 'value') * get.call(h, 'factor') + get.call(h, 'bias') + get.call(h, 'calls')
    else
      expected = get.call(h, 'left') + get.call(h, 'right')
    end
    raise 'incorrect result' unless get.call(h, 'result') == expected
    raise 'entry stacks changed between hooks' unless pre[:frames] == e[:frames]
    completed[[probe, key[2]]] += 1
  end
  if mode == 'disabled'
    raise 'unexpected stack' if e[:stack]
    next
  end
  wanted = {'partial'=>'PARTIAL', 'truncated'=>'TRUNCATED'}.fetch(mode, 'COMPLETE')
  raise "expected #{wanted}" unless e[:stack].include?(wanted)
  raise 'missing target entry' unless e[:frames].first.include?('TARGET_ENTRY')
  if mode == 'truncated'
    raise 'depth limit' unless e[:frames].size == 2
  elsif mode == 'partial'
    raise 'missing partial stop reason' unless e[:stack].include?('no native CFI')
    raise 'missing actual unsupported caller' unless e[:frames].last.include?('no_cfi_iteration')
  else
    raise 'missing library caller' unless e[:frames].any? { |f| f.include?('library_dispatch') }
    raise 'missing executable caller' unless e[:frames].any? { |f| f.include?('run_iteration') }
    raise 'missing native worker' unless e[:frames].any? { |f| f.include?('worker_loop') }
    if !method && !e[:stack].include?('application-only=true')
      raise 'missing explicit JIT bridge' unless e[:frames].any? { |f| f.include?('JIT_BRIDGE') }
      raise 'missing genuine enclosing target frame' unless e[:frames][1].include?('NATIVE') && e[:frames][1].include?('StackCalculator::multiply')
    end
  end
end
raise 'expected both probes paired on every thread' unless completed.size == 6
puts "#{mode}: 24 events, #{completed.values.sum} matched PRE/POST pairs, three TIDs, correct arguments/fields/results, entry stacks stable, no losses"
