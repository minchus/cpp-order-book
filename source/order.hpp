#pragma once

#include "types.hpp"


class Order
{
public:
  Order(OrderType order_type,
        OrderId order_id,
        Side side,
        Price price,
        Quantity quantity)
      : order_type_ {order_type}
      , order_id_ {order_id}
      , side_ {side}
      , price_ {price}
      , initial_quantity_ {quantity}
      , remaining_quantity_ {quantity}
  {
  }

  [[nodiscard]] OrderId GetOrderId() const { return order_id_; }
  [[nodiscard]] Side GetSide() const { return side_; }
  [[nodiscard]] Price GetPrice() const { return price_; }
  [[nodiscard]] OrderType GetOrderType() const { return order_type_; }
  [[nodiscard]] Quantity GetInitialQuantity() const
  {
    return initial_quantity_;
  }
  [[nodiscard]] Quantity GetRemainingQuantity() const
  {
    return remaining_quantity_;
  }
  [[nodiscard]] Quantity GetFilledQuantity() const
  {
    return GetInitialQuantity() - GetRemainingQuantity();
  }

  void Fill(Quantity quantity)
  {
    if (quantity > GetRemainingQuantity()) {
      throw std::logic_error(std::format(
          "Order ({}) cannot be filled for more than it's remaining quantity",
          GetOrderId()));
    }
    remaining_quantity_ -= quantity;
  }

  [[nodiscard]] bool IsFilled() const { return remaining_quantity_ == 0; }

private:
  OrderType order_type_;
  OrderId order_id_;
  Side side_;
  Price price_;
  Quantity initial_quantity_;
  Quantity remaining_quantity_;
};

using OrderPointer = std::shared_ptr<Order>;
using OrderPointers = std::list<OrderPointer>;

