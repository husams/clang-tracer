#pragma once

#include <cstddef>
#include <optional>
#include <string>

struct Product {
  int id;
  std::string name;
  int price_cents;
};

class ProductStore {
public:
  virtual std::optional<Product> load(int id);
  std::optional<Product> find_cached(int id);

  int queries() const { return queries_; }

private:
  int queries_ = 0;
  Product cache_{42, "Desk lamp", 1999};
};

struct Quote {
  Quote(int product, int price)
      : product(product)
      , price(price) {}

  virtual int amount() const;
  virtual ~Quote();
  int product;
  int price;
};

class Pricer {
public:
  explicit Pricer(int test_id)
      : test_id_(test_id) {}

  virtual Quote* make_quote(int product);

  int original_calls() const { return original_calls_; }

private:
  bool is_test_product(int product) const;
  int test_id_;
  int original_calls_ = 0;
};

Quote* quote_create(int product, int price);
void quote_destroy(Quote* quote);
char* status_message();
void* target_allocate(std::size_t size);
void target_release(void* storage);
int nesting(int depth);
