#pragma once


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


struct LevelInfo
{
  Price price_;
  Quantity quantity_;
};
using LevelInfos = std::vector<LevelInfo>;
