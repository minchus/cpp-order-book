#include <iostream>

#include "orderbook.hpp"

auto main() -> int
{
  Orderbook orderbook;
  const OrderId order_id = 1;
  orderbook.AddOrder(std::make_shared<Order>(
      OrderType::good_till_cancel, order_id, Side::buy, 100, 10));
  std::cout << orderbook.Size() << std::endl;
  orderbook.CancelOrder(order_id);
  std::cout << orderbook.Size() << std::endl;
  return 0;
}
