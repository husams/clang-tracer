# Compare metadata printed inside the native hooks with the normal CLI events.
directory, mode = ARGV
mode ||= 'complete'
hooks = {}
File.foreach(File.join(directory, 'target.log')) do |line|
  next unless line.match?(/\A(?:PRE|POST) probe=\d+ call=\d+ frame=/)
  match = line.match(/^(PRE|POST) probe=(\d+) call=(\d+) frame=(\d+) kind=(\d+) function=(.*?) \| module=(.*?) \| source=(.*):(\d+)$/)
  raise "invalid callback output: #{line}" unless match
  phase, probe, call, index, kind, function, mod, file, source_line = match.captures
  key = [probe, call, phase.downcase]
  (hooks[key] ||= {})[Integer(index)] = [Integer(kind), function, mod, file, Integer(source_line)]
end
events = []
event_files = mode == 'nonexported' ? %w[function-events.log method-events.log] : ['events.log']
event_files.flat_map { |file| File.readlines(File.join(directory, file)) }.each do |line|
  if line.start_with?('cursor=')
    events << {header: line, frames: []}
  elsif line.include?('entry-stack ')
    events.last[:stack] = line
  elsif line.match?(/^    #/)
    match = line.match(/^    #(\d+) (NATIVE|TARGET_ENTRY|JIT_BRIDGE) 0x[0-9a-f]+ (.*)$/)
    raise "invalid CLI frame: #{line}" unless match
    index, kind, rest = match.captures
    mod = file = '-'
    source_line = 0
    if rest =~ /^(.*?) \[([^\]]+)\+0x[0-9a-f]+\](?: (.*):(\d+))?$/
      function, mod, file, source_line = $1, $2, $3 || '-', Integer($4 || 0)
    else
      function = rest
    end
    numeric_kind = {'NATIVE'=>0, 'TARGET_ENTRY'=>1, 'JIT_BRIDGE'=>2}.fetch(kind)
    events.last[:frames] << [numeric_kind, function, mod, file, source_line]
  end
end
expected_count = mode == 'nonexported' ? 18 : 24
expected_tids = mode == 'nonexported' ? 1 : 3
raise 'incorrect event count' unless events.size == expected_count
raise 'incorrect thread count' unless events.map { |e| e[:header][/tid=(\d+)/,1] }.uniq.size == expected_tids
pairs = {}
matched = 0
comparisons = 0
events.each do |event|
  h = event[:header]
  raise 'event loss' unless h.include?('lost(agent=0,server=0)')
  key = [h[/probe=(\d+)/,1], h[/call=(\d+)/,1], h[/phase=(pre|post)/,1]]
  hook_frames = hooks.fetch(key)
  raise 'frame counts differ' unless hook_frames.size == event[:frames].size
  event[:frames].each_with_index do |frame, index|
    raise "callback/event metadata mismatch #{key} ##{index}" unless hook_frames.fetch(index) == frame
    comparisons += 1
  end
  expected_status = {'partial'=>'PARTIAL', 'truncated'=>'TRUNCATED', 'bridge-limit'=>'TRUNCATED'}.fetch(mode, 'COMPLETE')
  raise 'incorrect status' unless event[:stack].include?(expected_status)
  application_only = event[:stack].include?('application-only=true')
  if event[:stack].include?('captured=')
    captured = Integer(event[:stack][/captured=(\d+)/,1])
    hidden = Integer(event[:stack][/hidden=(\d+)/,1])
    raise 'filter accounting' unless captured == hidden + event[:frames].size
    raise 'full stack hid frames' if !application_only && hidden != 0
  end
  entry = event[:frames].first
  expected_file = mode == 'nonexported' ? '/target.cpp' : '/calculator.cpp'
  raise 'missing entry metadata' unless entry[0] == 1 && !entry[2].empty? && entry[3].end_with?(expected_file) && entry[4] > 0
  if mode == 'nonexported'
    raise 'wrong non-exported module' unless entry[2].end_with?('/target-pie')
    raise 'missing non-exported name' unless entry[1].match?(/\A(?:add\(|Calculator::multiply\()/)
  elsif mode == 'bridge-limit'
    raise 'native depth bound changed' unless captured == 3
    raise 'bridge filtering lost truncation' unless hidden == (h.include?('this=-') && application_only ? 1 : 0)
  elsif mode == 'truncated'
    raise 'depth exceeded' unless event[:frames].size == 2
  elsif mode == 'partial'
    raise 'partial stop caller' unless event[:frames].last[1].start_with?('no_cfi_iteration(')
  else
    raise 'missing DSO caller' unless event[:frames].any? { |f| f[1].start_with?('library_dispatch(') && f[2].end_with?('libcall-stacks-calculator.so') && f[4]>0 }
    raise 'missing executable caller' unless event[:frames].any? { |f| f[1].start_with?('run_iteration(') && f[2].match?(/\/call-stacks-(?:filter-)?target$/) && f[4]>0 }
    if h.include?('this=-') && !application_only
      raise 'missing explicit bridge' unless event[:frames].any? { |f| f[0]==2 && f[1]=='clangtracer_wrapper (verified bridge)' && f[2]=='-' && f[3]=='-' && f[4]==0 }
    end
  end
  if application_only
    raise 'visible internal frame' if event[:frames].any? { |f| f[0]==2 || f[1].start_with?('std::', 'void std::') || f[2].start_with?('/usr/lib') }
    if !%w[partial truncated bridge-limit nonexported].include?(mode)
      expected = h.include?('this=-') ? %w[stack_add StackCalculator::multiply library_dispatch run_iteration worker_loop] : %w[StackCalculator::multiply library_dispatch run_iteration worker_loop]
      actual = event[:frames].map { |f| f[1].split('(').first }
      raise "application frame selection #{actual}" unless actual == expected
    end
  end
  if key[2] == 'pre'
    pairs[key[0..1]] = event[:frames]
  else
    raise 'PRE/POST snapshot mismatch' unless pairs.delete(key[0..1]) == event[:frames]
    matched += 1
    if h.include?('this=-')
      value = ->(name) { Integer(h[/\b#{name}=int32:(-?\d+)/,1]) }
      raise 'incorrect original result' unless value.call('result') == value.call('left') + value.call('right')
    end
  end
end
raise 'incomplete pairing' unless matched * 2 == expected_count && pairs.empty?
puts "#{mode}: #{events.size} hooks/events, #{matched} PRE/POST pairs, #{expected_tids} TIDs, #{comparisons} exact function/module/file/line/kind comparisons; no event losses"
