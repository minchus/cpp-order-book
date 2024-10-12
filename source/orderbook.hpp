#pragma once

#include <list>

#include <map>
#include <numeric>
#include <format>
#include <vector>


enum class OrderType
{
  good_till_cancel,
  fill_and_kill
};

enum class Side
{
  buy,
  sell
};

using Price = std::int32_t;
using Quantity = std::uint32_t;
using OrderId = std::uint64_t;

struct LevelInfo
{
  Price price_;
  Quantity quantity_;
};

using LevelInfos = std::vector<LevelInfo>;

class OrderbookLevelInfos
{
public:
  OrderbookLevelInfos(const LevelInfos& bids, const LevelInfos& asks)
      : bids_ {bids}
      , asks_ {asks}
  {
  }

  [[nodiscard]] const LevelInfos& GetBids() const { return bids_; }
  [[nodiscard]] const LevelInfos& GetAsks() const { return asks_; }

private:
  LevelInfos bids_;
  LevelInfos asks_;
};

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
      remaining_quantity_ -= quantity;
    }
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

struct TradeInfo
{
  OrderId order_id_;
  Price price_;
  Quantity quantity_;
};

class Trade
{
public:
  Trade(const TradeInfo& bid_trade, const TradeInfo& ask_trade)
      : bid_trade_ {bid_trade}
      , ask_trade_ {ask_trade}
  {
  }
  [[nodiscard]] const TradeInfo& GetBidTrade() const { return bid_trade_; }
  [[nodiscard]] const TradeInfo& GetAskTrade() const { return ask_trade_; }

private:
  TradeInfo bid_trade_;
  TradeInfo ask_trade_;
};

using Trades = std::vector<Trade>;

class Orderbook
{
public:
  Trades AddOrder(OrderPointer order)
  {
    if (orders_.contains(order->GetOrderId())) {
      return {};
    }

    if (order->GetOrderType() == OrderType::fill_and_kill
        && !CanMatch(order->GetSide(), order->GetPrice()))
    {
      return {};
    }

    OrderPointers::iterator it;

    if (order->GetSide() == Side::buy) {
      auto& orders = bids_[order->GetPrice()];
      orders.push_back(order);
      it = std::next(orders.begin(), orders.size() - 1);
    } else {
      auto& orders = asks_[order->GetPrice()];
      orders.push_back(order);
      it = std::next(orders.begin(), orders.size() - 1);
    }

    orders_.insert({order->GetOrderId(), OrderEntry {order, it}});
    return MatchOrders();
  }

  void CancelOrder(OrderId order_id)
  {
    if (!orders_.contains(order_id)) {
      return;
    }

    const auto& [order, it] = orders_.at(order_id);

    if (order->GetSide() == Side::sell) {
      auto price = order->GetPrice();
      auto& orders = asks_.at(price);
      orders.erase(it);
      if (orders.empty()) {
        asks_.erase(price);
      }
    } else {
      auto price = order->GetPrice();
      auto& orders = bids_.at(price);
      orders.erase(it);
      if (orders.empty()) {
        bids_.erase(price);
      }
    }

    orders_.erase(order_id);
  }

  Trades ModifyOrder(OrderModify order)
  {
    if (!orders_.contains(order.GetOrderId())) {
      return {};
    }

    const auto& [existingOrder, _] = orders_.at(order.GetOrderId());
    CancelOrder(existingOrder->GetOrderId());
    return AddOrder(order.ToOrderPointer(existingOrder->GetOrderType()));
  }

  [[nodiscard]] std::size_t Size() const { return orders_.size(); }

  [[nodiscard]] OrderbookLevelInfos GetOrderInfos() const
  {
    LevelInfos bidInfos, askInfos;
    bidInfos.reserve(orders_.size());
    askInfos.reserve(orders_.size());

    auto CreateLevelInfos = [](Price price, const OrderPointers& orders)
    {
      return LevelInfo {
          price,
          std::accumulate(
              orders.begin(),
              orders.end(),
              (Quantity)0,
              [](Quantity running_sum, const OrderPointer& order)
              { return running_sum + order->GetRemainingQuantity(); })};
    };

    for (const auto& [price, orders] : bids_) {
      bidInfos.push_back(CreateLevelInfos(price, orders));
    }

    for (const auto& [price, orders] : asks_) {
      askInfos.push_back(CreateLevelInfos(price, orders));
    }

    return {bidInfos, askInfos};
  }

private:
  struct OrderEntry
  {
    OrderPointer order_ {nullptr};
    OrderPointers::iterator location_;
  };

  std::map<Price, OrderPointers, std::greater<>> bids_;
  std::map<Price, OrderPointers, std::less<>> asks_;
  std::unordered_map<OrderId, OrderEntry> orders_;

  [[nodiscard]] bool CanMatch(Side side, Price price) const
  {
    if (side == Side::buy) {
      if (asks_.empty()) {
        return false;
      }

      const auto& [best_ask, _] = *asks_.begin();
      return price >= best_ask;
    } else {
      if (bids_.empty()) {
        return false;
      }

      const auto& [best_bid, _] = *asks_.begin();
      return price <= best_bid;
    }
  }

  Trades MatchOrders()
  {
    Trades trades;
    trades.reserve(orders_.size());

    while (true) {
      if (bids_.empty() || asks_.empty()) {
        break;
      }

      auto& [bid_price, bids] = *bids_.begin();
      auto& [ask_price, asks] = *asks_.begin();

      if (bid_price < ask_price) {
        break;
      }

      while (!bids.empty() && !asks.empty()) {
        auto& bid = bids.front();
        auto& ask = asks.front();

        Quantity quantity =
            std::min(bid->GetRemainingQuantity(), ask->GetRemainingQuantity());

        bid->Fill(quantity);
        ask->Fill(quantity);

        if (bid->IsFilled()) {
          bids.pop_front();
          orders_.erase(bid->GetOrderId());
        }

        if (ask->IsFilled()) {
          asks.pop_front();
          orders_.erase(ask->GetOrderId());
        }

        if (bids.empty()) {
          bids_.erase(bid_price);
        }

        if (asks.empty()) {
          asks_.erase(ask_price);
        }

        trades.emplace_back(
            TradeInfo {bid->GetOrderId(), bid->GetPrice(), quantity},
            TradeInfo {ask->GetOrderId(), ask->GetPrice(), quantity});
      }
    }

    if (!bids_.empty()) {
      auto& [_, bids] = *bids_.begin();
      auto& order = bids.front();
      if (order->GetOrderType() == OrderType::fill_and_kill) {
        CancelOrder(order->GetOrderId());
      }
    }

    if (!asks_.empty()) {
      auto& [_, asks] = *asks_.begin();
      if (auto& order = asks.front();
          order->GetOrderType() == OrderType::fill_and_kill)
      {
        CancelOrder(order->GetOrderId());
      }
    }

    return trades;
  }

};  // class Orderbook
