#pragma once
#include <level.hpp>
#include <vector>

template <bool Shown = true,
          uint32_t MaxLevels = 20,
          uint32_t MaxOrds = 6,
          bool BinarySearch = false>
class Lob {
  // from most expensive to the cheapest
  bc::static_vector<Level<Side::Ask, Shown, MaxOrds>, MaxLevels> m_ask_levels;
  // from cheapest to most expensive
  bc::static_vector<Level<Side::Bid, Shown, MaxOrds>, MaxLevels> m_bid_levels;

  template <Side S>
  __attribute__((always_inline)) inline auto& levels() {
    if constexpr (S == Side::Ask)
      return m_ask_levels;
    else
      return m_bid_levels;
  }

  template <Side S>

  auto find_level(Price price) -> decltype(levels<S>().begin()) {
    if constexpr (BinarySearch) {
      auto cmp = [](auto const& a, auto const& b) {
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
        return levels<S>().end();
      return std::next(rit).base();
    }
  }

 public:
  Lob() {}

  template <Side S>
  std::optional<Level<S, Shown, MaxOrds>&> find_level(Price price) {
    if (auto it = find_level<S>(price); it != levels<S>().end())
      return *it;
    return std::nullopt;
  }
  template <Side S>
  std::optional<Order<S>> cancel_id(Id id) {
    auto rit = levels().rbegin();
    while (rit != levels().rend()) {
      if (const auto& ord = rit->cancel_id(id))
        return ord;
    }
    return std::nullopt;
  }
  template <Side S>
  std::optional<Order<S>> cancel_id(Price price, Id id) {
    if (auto it = find_level<S>(price); it != levels<S>().end()) {
      if (const auto& ord = it->cancel_id(id))
        return ord;
    }
    return std::nullopt;
  }

  template <typename U, Side S, OrderType T>
  std::vector<Order<Opp<S>>> send_order(U&& order) {
    // asserting the type in perfect forwarding
    static_assert(std::is_same_v<std::decay_t<U>, Order<S, T>>,
                  "Order must be of type Order<S,T>");

    if constexpr (T == OrderType::Market) {
      // doesn't rely on prices, walk lob until finished the size

    } else if constexpr (T == OrderType::Limit) {
      // fill if possible, leave in the lob if reached the price
    } else if constexpr (T == OrderType::FOK) {
      // fully filled or just cancel
    } else {
      // FAK, similar to Limit, but not left in the lob
    }
  }
};