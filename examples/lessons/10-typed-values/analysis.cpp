#include "types.hpp"
#include <iostream>

auto sample = tracer.batch("orders").at(1); // the POST event
auto order = sample.argument_as<shop::Order>(0);
auto coupon = sample.argument_as<std::string>(1);
auto fees = sample.argument_as<std::vector<int>>(2);
auto stock = sample.argument_as<std::map<std::string, int>>(3);
std::cout << "order " << order.id << " qty=" << order.quantity
          << " unit_price=" << order.unit_price << " coupon=" << coupon
          << " fees=" << fees.size() << " stock=" << stock.size()
          << " result=" << sample.result_as<double>() << std::endl;
