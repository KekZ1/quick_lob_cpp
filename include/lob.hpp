#pragma once
#include <level.hpp>
#include <vector>

struct AssetInfo {
  double ticksize = 0;
};

template <bool Shown = true, uint32_t MaxLevels = 20, uint32_t MaxOrds = 6,
          bool BinarySearch = false>
class Lob {
  // from worst level to best
  boost::circular_buffer<Level<Side::Ask, Shown, MaxOrds>> m_ask_levels;
  boost::circular_buffer<Level<Side::Bid, Shown, MaxOrds>> m_bid_levels;

  template <Side S> __attribute__((always_inline)) inline auto &levels() {
    if constexpr (S == Side::Ask)
      return m_ask_levels;
    else
      return m_bid_levels;
  }

  template <Side S>
  auto find_level(Price price) -> std::optional<decltype(levels<S>().begin())> {
    if constexpr (BinarySearch) {
      auto cmp = [](auto const &a, auto const &b) {
        if constexpr (S == Side::Ask)
          return a > b;
        else
          return a < b;
      };
      return std::lower_bound(levels<S>().begin(), levels<S>().end(), price,
                              cmp);
    } else {
      auto rit = std::find(levels<S>().rbegin(), levels<S>().rend(), price);
      if (rit == levels<S>().rend())
        return std::nullopt;
      return to_regular(rit);
    }
  }

public:
  Lob() {}

  template <typename U>
  Lob(U &&_info)
      : m_ask_levels(MaxLevels), m_bid_levels(MaxLevels){

                                 };

  template <Side S>
  std::optional<Level<S, Shown, MaxOrds> &> find_level(Price price) {
    if (auto it = find_level<S>(price); it != levels<S>().end())
      return *it;
    return std::nullopt;
  }

  template <Side S> std::optional<Order<S>> cancel_id(Id id) {
    auto rit = levels().rbegin();
    while (rit != levels().rend()) {
      if (const auto &ord = rit->cancel_id(id))
        return ord;
    }
    return std::nullopt;
  }

  template <Side S> std::optional<Order<S>> cancel_id(Price price, Id id) {
    if (auto it = find_level<S>(price); it != levels<S>().end()) {
      if (const auto &ord = it->cancel_id(id))
        return ord;
    }
    return std::nullopt;
  }

  // template <typename U, Side S, OrderType T>
  // std::optional<std::vector<Order<Opp<S>>>> fill_until_lifted()

  // template <typename U, Side S, OrderType T>
  // std::optional<std::vector<Order<Opp<S>>>> transaction(U&& order) {
  //   // asserting the type in perfect forwarding
  //   static_assert(std::is_same_v<std::decay_t<U>, Order<S, T>>,
  //                 "Order must be of type Order<S,T>");

  //   if constexpr (T == OrderType::Market) {
  //     // doesn't rely on prices, walk lob until finished the size

  //   } else if constexpr (T == OrderType::Limit) {
  //     // fill if possible, leave in the lob if reached the price
  //     /*
  //       Walk the opposite side of the lob
  //       then add the order into the correct side of the lob
  //     */
  //   } else if constexpr (T == OrderType::FOK) {
  //     // fully filled or just cancel
  //     if (auto pr_lvl = find_level<S>(order.price);
  //         pr_lvl != levels<S>().rend()) {
  //       Size avail_sz = std::accumulate(
  //           to_reverse(it_lvl), levels<S>().rend(), 0,
  //           [](auto roll, const auto& lvl) { return roll + lvl.size(); });

  //       if (avail_sz >= order.size) {  // can be filled

  //         while ()
  //       }
  //     }
  //     return std::nullopt;
  //   } else {
  //     // FAK, similar to Limit, but not left in the lob
  //   }
  // }
};