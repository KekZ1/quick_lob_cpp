#pragma once
#include <boost/container/static_vector.hpp>
#include <cstdint>
#include <memory>
#include <optional>
#include <type_traits>
namespace bc = boost::container;

using Price = int32_t;
using Size = int32_t;
using Queue = int32_t;
using Time = uint32_t;
using Id = uint32_t;

static_assert(std::is_integral<Price>::value, "Price must be an integral type");

#define TO_SIGNED(x)                                                           \
  static_cast<typename std::make_signed<decltype(x)>::type>(x)

enum OrderType : int { Limit = 0, FAK = 1, FOK = 2, Market = 3 };
enum Side : int { Ask = 1, Sell = 1, Bid = 0, Buy = 0, Cancel = 2 };
template <Side S> constexpr Side Opp = (S == Side::Ask ? Side::Bid : Side::Ask);

enum Offset : int { Open = 0, CloseTod = 1, CloseYtd = 2 };


template <bool Less, class T1, class T2> constexpr bool comp(T1 l, T2 r) {
  if constexpr (Less)
    return l < r;
  else
    return l > r;
}

template <typename T> auto to_reverse(T it) {
  return std::make_reverse_iterator(it);
}
template <typename T> auto to_regular(T rit) { return std::prev(rit.base()); }