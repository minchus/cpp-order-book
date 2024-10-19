#pragma once

#include <limits>


enum class Side
{
  buy,
  sell
};


enum class OrderType
{
  good_till_cancel,
  fill_and_kill,
  fill_or_kill,
  good_for_day,
  market,
};


using Price = std::int32_t;
using Quantity = std::uint32_t;
using OrderId = std::uint64_t;
using OrderIds = std::vector<OrderId>;


struct LevelInfo
{
  Price price_;
  Quantity quantity_;
};
using LevelInfos = std::vector<LevelInfo>;


struct Constants
{
  static const Price invalid_price = std::numeric_limits<Price>::quiet_NaN();
};
