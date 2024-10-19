#pragma once

#include <list>
#include <map>
#include <numeric>
#include <thread>
#include <vector>

#include "order.hpp"
#include "order_modify.hpp"
#include "orderbook_level_infos.hpp"
#include "trade.hpp"

class Orderbook
{
public:
  Orderbook();
  ~Orderbook();
  Trades AddOrder(OrderPointer order);
  void CancelOrder(OrderId order_id);
  Trades ModifyOrder(OrderModify order);
  [[nodiscard]] std::size_t Size() const { return orders_.size(); }
  [[nodiscard]] OrderbookLevelInfos GetOrderInfos() const;

private:
  struct OrderEntry
  {
    OrderPointer order_ {nullptr};
    OrderPointers::iterator location_;
  };

  struct LevelData
  {
    Quantity quantity_{};
    Quantity count_{};

    enum class Action
    {
      Add,
      Remove,
      Match,
    };
  };

  std::unordered_map<Price, LevelData> data_;
  std::map<Price, OrderPointers, std::greater<>> bids_;
  std::map<Price, OrderPointers, std::less<>> asks_;
  std::unordered_map<OrderId, OrderEntry> orders_;

  mutable std::mutex orders_mutex_;
  std::thread orders_prune_thread_;
  std::condition_variable shutdown_cv_;
  std::atomic_bool shutdown_ {false};

  void OnOrderCancelled(OrderPointer order);
  void OnOrderAdded(OrderPointer order);
  void OnOrderMatched(Price price, Quantity quantity, bool isFullyFilled);
  void UpdateLevelData(Price price, Quantity quantity, LevelData::Action action);

  void CancelOrderInternal(OrderId order_id);
  void CancelOrders(OrderIds order_ids);
  void PruneGoodForDayOrders();
  [[nodiscard]] bool CanFullyFill(Side side, Price price, Quantity quantity) const;
  [[nodiscard]] bool CanMatch(Side side, Price price) const;
  [[nodiscard]] Trades MatchOrders();
};  // class Orderbook
