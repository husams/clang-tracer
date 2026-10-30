# Inspect the framed public Protobuf recording from transport-pressure.sh.
# Generic wire reading avoids any runtime dependency on generated Ruby bindings.
require 'csv'

def varint(bytes, offset)
  value = 0
  10.times do |index|
    byte = bytes.getbyte(offset)
    raise 'truncated varint' unless byte
    offset += 1
    value |= (byte & 127) << (7 * index)
    return [value, offset] if byte < 128
  end
  raise 'invalid varint'
end

def fields(bytes)
  result = Hash.new { |hash, key| hash[key] = [] }
  offset = 0
  while offset < bytes.bytesize
    tag, offset = varint(bytes, offset)
    case tag & 7
    when 0
      value, offset = varint(bytes, offset)
    when 1, 5
      size = (tag & 7) == 1 ? 8 : 4
      value = bytes.byteslice(offset, size)
      raise 'short fixed field' unless value && value.bytesize == size
      offset += size
    when 2
      size, offset = varint(bytes, offset)
      value = bytes.byteslice(offset, size)
      raise 'short message' unless value && value.bytesize == size
      offset += size
    else
      raise 'unexpected wire type'
    end
    result[tag >> 3] << value
  end
  result
end

def scalar(bytes)
  value = fields(bytes)
  raise 'expected INT32' unless value[1] == [2] && value[2].size == 1
  encoded = value[2].first
  (encoded >> 1) ^ -(encoded & 1)
end

directory = ARGV.fetch(0)
counts = CSV.read(File.join(directory, 'event-counts.csv'), headers: true).first
expected, received, dropped, retained = %w[expected received agent_dropped retained].map { |key| counts[key].to_i }
raise 'unaccounted transport events' unless expected == received + dropped
bytes = File.binread(File.join(directory, 'events.pb'))
events = []
until bytes.empty?
  raise 'short frame header' if bytes.bytesize < 4
  size = bytes.slice!(0, 4).unpack1('V')
  body = bytes.slice!(0, size)
  raise 'short frame body' unless body && body.bytesize == size
  events << fields(body)
end
raise 'retained event count' unless events.size == retained
raise 'server cursor/order' unless events.map { |e| e[6].first } == ((received - retained + 1)..received).to_a
raise 'duplicate probe sequence' unless events.map { |e| e[7].first }.uniq.size == retained
pairs = {}
stack_frames = 0
events.each do |event|
  args = event[10].map { |value| scalar(value) }
  raise 'arguments' unless args.size == 2
  phase, call = event[14].first, event[15].first
  snapshot = event[19].first
  if snapshot
    stack = fields(snapshot)
    raise 'stack mode/status' unless stack[1] == [1] && stack[2] == [24] && stack[5] == [1]
    raise 'stack accounting' unless stack[3].size == 5 && stack[6] == [3] && stack[7] == [8]
    stack[3].each do |bytes|
      frame = fields(bytes)
      raise 'frame metadata' unless frame[9] == [1] && frame[8].fetch(0, 0) > 0 && [4, 6, 7].all? { |key| !frame[key].fetch(0, '').empty? }
      stack_frames += 1
    end
  end
  if phase == 1
    raise 'PRE has result' unless event[16].empty?
    pairs[call] = [args, snapshot]
  elsif phase == 2
    raise 'original result changed' unless scalar(event[16].first) == args.sum
    pre = pairs.delete(call)
    raise 'PRE/POST arguments/stack changed' if pre && pre != [args, snapshot]
  else
    raise 'invalid phase'
  end
end
if dropped.zero?
  raise 'missing paired event' unless pairs.empty? && events.size == expected
  raise 'probe sequence gaps' unless events.map { |e| e[7].first } == (1..expected).to_a
  raise 'unexpected loss counter' unless events.all? { |e| e[12].fetch(0, 0).zero? && e[13].fetch(0, 0).zero? }
end
raise 'history accounting' unless events.last[13].fetch(0, 0) == received - retained
puts "#{directory}: #{received} received, #{dropped} agent drops, #{retained} retained; #{stack_frames} complete application frames; every recorded original result correct; cursors and history accounting match"
