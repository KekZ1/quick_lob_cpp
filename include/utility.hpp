#pragma once
#include <boost/container/static_vector.hpp>
#include <cstdint>
#include <memory>
#include <optional>
#include <type_traits>
namespace bc = boost::container;

using Price = uint32_t;
using Size = uint32_t;
using Queue = uint32_t;
using Time = uint32_t;
using Id = uint32_t;

#define SIGNED(x) static_cast<typename std::make_signed<decltype(x)>::type>(x)

enum class OrderType : int { Limit = 0, FAK = 1, FOK = 2, Market = 3 };
enum class Side : bool {
  Ask = true /* also sell */,
  Bid = false /* also buy */
};
template <Side S>
constexpr Side Opp = (S == Side::Ask ? Side::Bid : Side::Ask);

enum class Offset : int { Open, CloseTod, CloseYtd };

template <bool Less, class T1, class T2>
constexpr bool comp(T1 l, T2 r) {
    if constexpr (Less) return l < r;
    else return l > r;
}
