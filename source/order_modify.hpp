#pragma once

#include "order.hpp"
#include "types.hpp"

class OrderModify
{
public:
  OrderModify(OrderId order_id, Side side, Price price, Quantity quantity)
      : order_id_ {order_id}
      , side_ {side}
      , price_ {price}
      , quantity_ {quantity}
  {
  }

  [[nodiscard]] OrderId GetOrderId() const { return order_id_; }
  [[nodiscard]] Price GetPrice() const { return price_; }
  [[nodiscard]] Side GetSide() const { return side_; }
  [[nodiscard]] Quantity GetQuantity() const { return quantity_; }

  [[nodiscard]] OrderPointer ToOrderPointer(OrderType type) const
  {
    return std::make_shared<Order>(
        type, GetOrderId(), GetSide(), GetPrice(), GetQuantity());
  }

private:
  OrderId order_id_;
  Side side_;
  Price price_;
  Quantity quantity_;
};
