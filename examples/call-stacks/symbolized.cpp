#include <cstdio>
#include <iomanip>
#include <limits>
#include <sstream>

class Interceptor {
  static void write_value(std::ostream &output, const TraceValue &value) {
    switch (value.type) {
    case ValueType::Void:
      output << "void";
      break;
    case ValueType::Int32:
      output << "int32:" << value.signed_value;
      break;
    case ValueType::Int64:
      output << "int64:" << value.signed_value;
      break;
    case ValueType::UInt64:
      output << "uint64:" << value.unsigned_value;
      break;
    case ValueType::Float32:
    case ValueType::Float64:
      output << (value.type == ValueType::Float32 ? "float32:" : "float64:")
             << std::setprecision(
                    value.type == ValueType::Float32
                        ? std::numeric_limits<float>::max_digits10
                        : std::numeric_limits<double>::max_digits10)
             << value.real_value;
      break;
    case ValueType::Pointer:
    case ValueType::Object:
      output << (value.type == ValueType::Object ? "object:0x" : "pointer:0x")
             << std::hex << value.unsigned_value << std::dec;
      break;
    default:
      output << "unknown:?";
      break;
    }
  }

  static void read_stack(const Call &call, const char *phase,
                         bool print_arguments, bool print_result) {
    const auto &stack = call.stack();
    std::ostringstream output;
    if (print_arguments || print_result) {
      output << phase << " probe=" << call.probe_id() << " call=" << call.id();
      if (print_arguments) {
        output << " args=[";
        for (unsigned i = 0; i < call.argument_count(); ++i) {
          if (i)
            output << ", ";
          output << call.argument_name(i) << '=';
          write_value(output, call.argument_value(i));
        }
        output << ']';
      }
      if (print_result) {
        output << " result=";
        write_value(output, call.result_value());
      }
      output << '\n';
    }
    for (unsigned i = 0; i < stack.count; ++i) {
      const auto &frame = stack.frames[i];
      output << phase << " probe=" << call.probe_id() << " call=" << call.id()
             << " frame=" << i << " kind=" << static_cast<unsigned>(frame.kind)
             << " function=" << (frame.function[0] ? frame.function : "??")
             << " | module=" << (frame.module[0] ? frame.module : "-")
             << " | source=" << (frame.file[0] ? frame.file : "-") << ':'
             << frame.line << '\n';
    }
    const auto text = output.str();
    std::fwrite(text.data(), 1, text.size(), stdout);
    std::fflush(stdout);
    call.field("stack_frames", stack.count);
  }

public:
  void pre(const Call &call) {
    if (call.stack().status == tracer::StackStatus::Disabled)
      call.capture_stack(24);
    read_stack(call, "PRE", call.print_options().arguments, false);
  }
  void post(const Call &call) {
    if (call.stack().status == tracer::StackStatus::Disabled)
      call.capture_stack(24);
    read_stack(call, "POST", false, call.print_options().result);
  }
};
