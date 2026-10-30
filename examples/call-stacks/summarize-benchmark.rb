require 'csv'

directory = ARGV.fetch(0)
rows = CSV.read(File.join(directory, 'measurements.csv'), headers: true)
groups = rows.group_by { |row| row['mode'] }
median = lambda do |values|
  sorted = values.sort
  (sorted[(sorted.size - 1) / 2] + sorted[sorted.size / 2]) / 2.0
end
baseline = median.call(groups.fetch('off').map { |r| r['wall_ns'].to_f / r['calls'].to_i })
puts '| Mode | Repeats | Calls/repeat | Median ns/call | Min–max ns/call | Median target CPU ns/call | Relative to off |'
puts '| --- | ---: | ---: | ---: | ---: | ---: | ---: |'
%w[off no-stack app-stack].each do |mode|
  samples = groups.fetch(mode)
  raise 'invalid target result' unless samples.all? { |r| r['valid'] == '1' }
  wall = samples.map { |r| r['wall_ns'].to_f / r['calls'].to_i }
  cpu = samples.map { |r| r['cpu_ns'].to_f / r['calls'].to_i }
  value = median.call(wall)
  puts format('| %s | %d | %s | %.2f | %.2f–%.2f | %.2f | %.2fx |',
              mode, samples.size, samples.map { |r| r['calls'] }.uniq.join('/'),
              value, wall.min, wall.max, median.call(cpu), value / baseline)
end
counts = CSV.read(File.join(directory, 'event-counts.csv'), headers: true)
puts "\nEvent accounting includes 100 warmup + 1500 timed calls per traced repeat:"
%w[no-stack app-stack].each do |mode|
  samples = counts.select { |r| r['mode'] == mode }
  expected = samples.sum { |r| r['expected_events'].to_i }
  received = samples.sum { |r| r['received_events'].to_i }
  dropped = samples.sum { |r| r['agent_dropped_events'].to_i }
  raise 'unaccounted events' unless expected == received + dropped
  puts format('%s: expected=%d received=%d agent-dropped=%d (%.3f%%)',
              mode, expected, received, dropped, 100.0 * dropped / expected)
end
