#include <chrono>
#include <ctime>

#include "orderbook.hpp"

Orderbook::Orderbook()
    : orders_prune_thread_([this] { PruneGoodForDayOrders(); }) {};

Orderbook::~Orderbook()
{
  shutdown_.store(true, std::memory_order_release);
  shutdown_cv_.notify_one();
  orders_prune_thread_.join();
}

Trades Orderbook::AddOrder(OrderPointer order)
{
  std::scoped_lock ordersLock {orders_mutex_};

  if (orders_.contains(order->GetOrderId())) {
    return {};
  }

  if (order->GetOrderType() == OrderType::market) {
    if (order->GetSide() == Side::buy && !asks_.empty()) {
      const auto& [worst_ask, _] = *asks_.rbegin();
      order->ToGoodTillCancel(worst_ask);
    } else if (order->GetSide() == Side::sell && !bids_.empty()) {
      const auto& [worst_bid, _] = *bids_.rbegin();
      order->ToGoodTillCancel(worst_bid);
    } else {
      return {};
    }
  }

  if (order->GetOrderType() == OrderType::fill_and_kill
      && !CanMatch(order->GetSide(), order->GetPrice()))
  {
    return {};
  }

  if (order->GetOrderType() == OrderType::fill_or_kill
      && !CanFullyFill(
          order->GetSide(), order->GetPrice(), order->GetInitialQuantity()))
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
  OnOrderAdded(order);
  return MatchOrders();
}

void Orderbook::CancelOrder(OrderId order_id)
{
  std::scoped_lock orders_lock(orders_mutex_);
  CancelOrderInternal(order_id);
}

Trades Orderbook::ModifyOrder(OrderModify order)
{
  OrderType order_type;

  {
    std::scoped_lock orders_lock(orders_mutex_);

    if (!orders_.contains(order.GetOrderId())) {
      return {};
    }
    const auto& [existingOrder, _] = orders_.at(order.GetOrderId());
    order_type = existingOrder->GetOrderType();
  }

  CancelOrder(order.GetOrderId());
  return AddOrder(order.ToOrderPointer(order_type));
}

OrderbookLevelInfos Orderbook::GetOrderInfos() const
{
  LevelInfos bidInfos, askInfos;
  bidInfos.reserve(orders_.size());
  askInfos.reserve(orders_.size());

  auto CreateLevelInfos = [](Price price, const OrderPointers& orders)
  {
    return LevelInfo {
        price,
        std::accumulate(orders.begin(),
                        orders.end(),
                        (Quantity)0,
                        [](Quantity running_sum, const OrderPointer& order) {
                          return running_sum + order->GetRemainingQuantity();
                        })};
  };

  for (const auto& [price, orders] : bids_) {
    bidInfos.push_back(CreateLevelInfos(price, orders));
  }

  for (const auto& [price, orders] : asks_) {
    askInfos.push_back(CreateLevelInfos(price, orders));
  }

  return {bidInfos, askInfos};
}

void Orderbook::OnOrderCancelled(OrderPointer order)
{
  UpdateLevelData(order->GetPrice(),
                  order->GetRemainingQuantity(),
                  LevelData::Action::Remove);
}

void Orderbook::OnOrderAdded(OrderPointer order)
{
  UpdateLevelData(
      order->GetPrice(), order->GetInitialQuantity(), LevelData::Action::Add);
}

void Orderbook::OnOrderMatched(Price price,
                               Quantity quantity,
                               bool isFullyFilled)
{
  UpdateLevelData(
      price,
      quantity,
      isFullyFilled ? LevelData::Action::Remove : LevelData::Action::Match);
}

void Orderbook::UpdateLevelData(Price price,
                                Quantity quantity,
                                LevelData::Action action)
{
  auto& data = data_[price];

  data.count_ += action == LevelData::Action::Remove ? -1
      : action == LevelData::Action::Add             ? 1
                                                     : 0;
  if (action == LevelData::Action::Remove || action == LevelData::Action::Match)
  {
    data.quantity_ -= quantity;
  } else {
    data.quantity_ += quantity;
  }

  if (data.count_ == 0) {
    data_.erase(price);
  }
}

void Orderbook::CancelOrderInternal(OrderId order_id)
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
  OnOrderCancelled(order);
}

void Orderbook::CancelOrders(OrderIds order_ids)
{
  std::scoped_lock orders_lock(orders_mutex_);
  for (const auto order_id : order_ids) {
    CancelOrderInternal(order_id);
  }
}

void Orderbook::PruneGoodForDayOrders()
{
  while (true) {
    // Get current time
    auto now = std::chrono::system_clock::now();
    std::time_t now_time_t = std::chrono::system_clock::to_time_t(now);
    std::tm local_tm = *std::localtime(&now_time_t);

    // Set the time for 4 PM (16:00)
    std::tm four_pm_tm = local_tm;
    four_pm_tm.tm_hour = 16;  // 4 PM = 16:00 in 24-hour format
    four_pm_tm.tm_min = 0;
    four_pm_tm.tm_sec = 0;

    // Convert 4 PM time to time_t and then to a time_point
    std::time_t four_pm_time_t = std::mktime(&four_pm_tm);
    auto four_pm_time_point =
        std::chrono::system_clock::from_time_t(four_pm_time_t);

    // If 4 PM is in the past, we calculate for tomorrow's 4 PM
    if (four_pm_time_point < now) {
      four_pm_tm.tm_mday += 1;  // Move to the next day
      four_pm_time_t = std::mktime(&four_pm_tm);
      four_pm_time_point =
          std::chrono::system_clock::from_time_t(four_pm_time_t);
    }

    // Calculate the difference in milliseconds
    auto till = std::chrono::duration_cast<std::chrono::milliseconds>(
        four_pm_time_point - now + std::chrono::milliseconds(100));

    {
      std::unique_lock orders_lock(orders_mutex_);

      if (shutdown_.load(std::memory_order_acquire)
          || shutdown_cv_.wait_for(orders_lock, till)
              == std::cv_status::no_timeout)
      {
        return;
      }
    }

    OrderIds order_ids;

    {
      std::scoped_lock ordersLock {orders_mutex_};

      for (const auto& [_, entry] : orders_) {
        const auto& [order, order_iter] = entry;

        if (order->GetOrderType() != OrderType::good_for_day)
          continue;

        order_ids.push_back(order->GetOrderId());
      }
    }

    CancelOrders(order_ids);
  }
}

bool Orderbook::CanFullyFill(Side side, Price price, Quantity quantity) const
{
  if (!CanMatch(side, price))
    return false;

  std::optional<Price> threshold;

  if (side == Side::buy) {
    const auto [ask_price, _] = *asks_.begin();
    threshold = ask_price;
  } else {
    const auto [bid_price, _] = *bids_.begin();
    threshold = bid_price;
  }

  for (const auto& [level_price, level_data] : data_) {
    if (threshold.has_value()
        && ((side == Side::buy && threshold.value() < level_price)
            || (side == Side::sell && threshold.value() > level_price)))
    {
      continue;
    }

    if ((side == Side::buy && level_price > price)
        || (side == Side::sell && level_price < price))
    {
      continue;
    }

    if (quantity <= level_data.quantity_) {
      return true;
    }

    quantity -= level_data.quantity_;
  }

  return false;
}

bool Orderbook::CanMatch(Side side, Price price) const
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

Trades Orderbook::MatchOrders()
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

      OnOrderMatched(bid->GetPrice(), quantity, bid->IsFilled());
      OnOrderMatched(ask->GetPrice(), quantity, ask->IsFilled());
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
