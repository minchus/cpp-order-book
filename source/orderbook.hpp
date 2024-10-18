#pragma once

#include <list>
#include <map>
#include <numeric>
#include <vector>

#include "order.hpp"
#include "orderbook_level_infos.hpp"
#include "order_modify.hpp"
#include "trade.hpp"


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
