// The callback runs inside the target process. It deliberately reads every
// scalar representation so integration checks can verify the native ABI map.
extern "C" long long trace_callback(const TraceContext &context) {
  long long result = static_cast<long long>(context.argument_count);
  for (unsigned i = 0; i < context.argument_count && i < 8; ++i) {
    const TraceValue &value = context.arguments[i];
    switch (value.type) {
    case ValueType::Int32:
    case ValueType::Int64:
      result += value.signed_value;
      break;
    case ValueType::UInt64:
    case ValueType::Pointer:
      result +=
          static_cast<long long>(value.unsigned_value & 0x7fffffffffffffffULL);
      break;
    case ValueType::Float32:
    case ValueType::Float64:
      result += static_cast<long long>(value.real_value * 100.0);
      break;
    case ValueType::Void:
    case ValueType::Unspecified:
      break;
    }
  }
  if (context.object_address != 0) {
    result += 1;
  }
  return result;
}
