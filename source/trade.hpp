#pragma once

#include "trade_info.hpp"


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

