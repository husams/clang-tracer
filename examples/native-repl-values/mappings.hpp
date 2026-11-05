#pragma once
#include "types.hpp"

// Load in clang-tracer-repl, whose prelude supplies tracer_repl's event APIs.
template <> struct tracer_repl::EventRecord<value_examples::Address> {
  static constexpr const char *type_name = "value_examples::Address";
  static auto fields() {
    return std::make_tuple(
        tracer_repl::event_member("city", &value_examples::Address::city),
        tracer_repl::event_member("postal_code",
                                  &value_examples::Address::postal_code));
  }
};

template <> struct tracer_repl::EventRecord<value_examples::Box<int>> {
  static constexpr const char *type_name = "value_examples::Box<int>";
  static auto fields() {
    return std::make_tuple(
        tracer_repl::event_member("value", &value_examples::Box<int>::value));
  }
};

template <> struct tracer_repl::EventRecord<value_examples::Box<double>> {
  static constexpr const char *type_name = "value_examples::Box<double>";
  static auto fields() {
    return std::make_tuple(
        tracer_repl::event_member("value", &value_examples::Box<double>::value));
  }
};

template <> struct tracer_repl::EventRecord<value_examples::Account> {
  static constexpr const char *type_name = "value_examples::Account";
  static auto fields() {
    return std::make_tuple(
        tracer_repl::event_member("id", &value_examples::Account::id),
        tracer_repl::event_member("owner", &value_examples::Account::owner),
        tracer_repl::event_member("address",
                                  &value_examples::Account::address),
        tracer_repl::event_member("priority", &value_examples::Account::priority),
        tracer_repl::event_member("score", &value_examples::Account::score));
  }
};
