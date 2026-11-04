#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <type_traits>

#include "events.hpp"

using namespace tracer_repl;
namespace api = clangtracer::v1;

namespace {
void require(bool ok, const char *message) {
  if (!ok)
    throw std::runtime_error(message);
}

template <class Error, class Call>
void rejects(Call call, const char *message) {
  try {
    call();
  } catch (const Error &) {
    return;
  }
  throw std::runtime_error(message);
}

Event snapshot(const api::TraceEvent &message) {
  EventBuffer buffer(1, 16384);
  buffer.push(message);
  return buffer.snapshot().at(0);
}

void native_values() {
  api::TraceEvent message;
  auto *argument = message.add_arguments();
  auto *result = message.mutable_result();
  argument->set_type(api::BOOL);
  result->set_type(api::BOOL);
  for (unsigned truth : {0u, 1u}) {
    argument->set_unsigned_value(truth);
    result->set_signed_value(truth);
    auto event = snapshot(message);
    static_assert(std::is_same_v<decltype(event.result_as<bool>()), bool>);
    static_assert(std::is_same_v<decltype(event.argument_as<int>(0)), int>);
    require(event.has_result() && event.result_as<bool>() == bool(truth),
            "boolean result is a bool");
    require(event.argument_as<bool>(0) == bool(truth),
            "boolean argument is a bool");
    require(std::get<bool>(event.result_value()) == bool(truth),
            "variant retains boolean type");
    require(event.result() == (truth ? "true" : "false"),
            "text accessors stay compatible");
    rejects<std::invalid_argument>([&] { event.result_as<int>(); },
                                   "boolean cannot silently become a number");
  }
  argument->set_type(api::INT32);
  argument->set_signed_value(-21);
  result->set_type(api::INT64);
  result->set_signed_value(42);
  auto event = snapshot(message);
  require(event.argument_as<int>(0) + event.result_as<int>() == 21,
          "integers participate directly in arithmetic");
  require(event.result_as<unsigned>() == 42,
          "checked positive integer conversion");
  rejects<std::out_of_range>([&] { event.argument_as<unsigned>(0); },
                             "negative cannot become unsigned");
  rejects<std::invalid_argument>([&] { event.result_as<bool>(); },
                                 "integer cannot silently become a bool");
  rejects<std::invalid_argument>([&] { event.result_as<double>(); },
                                 "integer is not a floating value");
  rejects<std::out_of_range>([&] { event.argument_as<int>(1); },
                             "argument bounds are checked");
  for (auto edge : {std::numeric_limits<std::int64_t>::min(),
                    std::numeric_limits<std::int64_t>::max()}) {
    result->set_signed_value(edge);
    event = snapshot(message);
    require(event.result_as<std::int64_t>() == edge,
            "signed 64-bit endpoints stay exact");
    rejects<std::out_of_range>([&] { event.result_as<std::int32_t>(); },
                               "narrowing overflow is rejected");
  }
  result->set_type(api::UINT64);
  result->set_unsigned_value(std::numeric_limits<std::uint64_t>::max());
  event = snapshot(message);
  require(event.result_as<std::uint64_t>() ==
              std::numeric_limits<std::uint64_t>::max(),
          "unsigned 64-bit maximum stays exact");
  rejects<std::out_of_range>([&] { event.result_as<std::int64_t>(); },
                             "unsigned overflow cannot become signed");
  rejects<std::out_of_range>([&] { event.result_as<std::uint32_t>(); },
                             "unsigned narrowing is checked");
  result->set_type(api::FLOAT64);
  result->set_real_value(1.25);
  argument->set_type(api::FLOAT32);
  argument->set_real_value(2.5);
  event = snapshot(message);
  require(event.result_as<double>() + event.argument_as<float>(0) == 3.75,
          "floating arithmetic uses native numbers");
  rejects<std::invalid_argument>([&] { event.result_as<int>(); },
                                 "floating values cannot silently truncate");
  result->set_real_value(std::numeric_limits<double>::max());
  event = snapshot(message);
  rejects<std::out_of_range>([&] { event.result_as<float>(); },
                             "floating overflow is rejected");
  result->set_real_value(std::numeric_limits<double>::infinity());
  require(std::isinf(snapshot(message).result_as<float>()),
          "floating infinity is preserved");
  result->set_real_value(std::numeric_limits<double>::quiet_NaN());
  require(std::isnan(snapshot(message).result_as<double>()),
          "floating NaN is preserved");
  result->set_type(api::POINTER);
  result->set_unsigned_value(42);
  event = snapshot(message);
  rejects<std::invalid_argument>([&] { event.result_as<std::uint64_t>(); },
                                 "addresses are not scalar integers");
  result->set_type(api::VOID);
  result->clear_data();
  event = snapshot(message);
  rejects<std::invalid_argument>([&] { event.result_as<int>(); },
                                 "void has no numeric value");
  result->set_type(api::INT32);
  result->set_real_value(42);
  event = snapshot(message);
  rejects<std::invalid_argument>([&] { event.result_as<int>(); },
                                 "payload/type mismatch is rejected");
  result->clear_data();
  event = snapshot(message);
  rejects<std::invalid_argument>([&] { event.result_as<int>(); },
                                 "missing numeric payload is rejected");
  result->set_type(api::BOOL);
  result->set_unsigned_value(2);
  event = snapshot(message);
  rejects<std::invalid_argument>([&] { event.result_as<bool>(); },
                                 "invalid boolean payload is rejected");
  message.clear_result();
  event = snapshot(message);
  require(!event.has_result(), "missing result is discoverable");
  rejects<std::runtime_error>([&] { event.result_as<int>(); },
                              "missing result does not become zero");
}

void context_values() {
  api::TraceEvent message;
  auto *context = message.mutable_context()->mutable_event();
  auto *argument = context->add_arguments();
  argument->set_index(3);
  argument->mutable_value()->set_value_type(api::BOOL);
  argument->mutable_value()->set_unsigned_value(1);
  context->mutable_result()->set_value_type(api::FLOAT64);
  context->mutable_result()->set_real_value(3.5);
  message.mutable_result()->set_type(api::INT32);
  message.mutable_result()->set_signed_value(99);
  auto event = snapshot(message);
  require(
      event.has_result() && event.argument_as<bool>(3) &&
          event.result_as<double>() == 3.5,
      "shared context uses recorded argument indices and its own typed result");
  rejects<std::out_of_range>([&] { event.argument_as<bool>(0); },
                             "sparse context indices are not vector positions");
  argument->mutable_value()->set_value_type(api::INT64);
  argument->mutable_value()->set_signed_value(-17);
  context->mutable_result()->set_value_type(api::UINT64);
  context->mutable_result()->set_unsigned_value(42);
  event = snapshot(message);
  require(event.argument_as<int>(3) == -17 && event.result_as<unsigned>() == 42,
          "context integer values stay numeric");
  context->mutable_result()->set_value_type(api::BOOL);
  context->mutable_result()->set_signed_value(0);
  require(!snapshot(message).result_as<bool>(), "context false is a bool");
  context->mutable_result()->set_value_type(api::FLOAT32);
  context->mutable_result()->set_real_value(1.5);
  require(snapshot(message).result_as<float>() == 1.5f,
          "context float32 is supported");
  context->clear_result();
  message.clear_result();
  event = snapshot(message);
  require(!event.has_result(), "context missing result is discoverable");
  rejects<std::runtime_error>([&] { event.result_as<bool>(); },
                              "missing context result does not become false");
}
} // namespace

int main() {
  try {
    native_values();
    context_values();
    std::cout << "typed event value checks passed\n";
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
