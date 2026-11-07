#pragma once

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>

namespace prepared_demo {

struct Adjustment {
  int delta;
  std::optional<int> limit;

  const Adjustment *operator&() const noexcept { return nullptr; }
};

using Choice = std::variant<std::monostate, int, std::string, int>;

struct Receipt {
  std::string label;
  std::vector<int> parts;
  int total;
};

class Ledger {
public:
  int ledger_id = 7;

protected:
  int ledger_version = 3;
  Choice ledger_choice{std::in_place_index<3>, 17};

private:
  std::string ledger_owner = "inherited private";
  signed int ledger_flags : 4;
  std::unique_ptr<Adjustment> ledger_credit =
      std::make_unique<Adjustment>(Adjustment{6, 60});
  std::unique_ptr<int> empty_credit;
  std::shared_ptr<int> ledger_total = std::make_shared<int>(81);
  std::weak_ptr<int> ledger_total_ref = ledger_total;
  std::weak_ptr<int> expired_total_ref;
  Choice ledger_empty_choice;

public:
  Ledger() : ledger_flags(-3) {}
  const Ledger *operator&() const noexcept { return nullptr; }
};

struct Audit {
  int audit_id = 19;
  int account_id = 8;
};

class Account : private Ledger, protected Audit {
public:
  int account_id = 42;
  Receipt apply(const std::string &label, std::vector<int> parts,
                Adjustment adjustment, std::unique_ptr<int> units,
                const std::shared_ptr<int> &factor,
                const std::weak_ptr<int> &observer, Choice choice);

protected:
  std::optional<int> reserve = 25;

private:
  int balance = 100;
  Adjustment last_adjustment{0, std::nullopt};
  std::pair<int, std::string> limits{10, "ceiling"};
  std::tuple<int, std::string> description{14, "tuple"};
  int recent[2]{9, 12};
  const int *balance_address = &balance;
  std::string_view category = "savings";
};

} // namespace prepared_demo
