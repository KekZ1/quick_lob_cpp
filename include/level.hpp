#pragma once
#include <absl/container/inlined_vector.h>
#include <algorithm>
#include <boost/circular_buffer.hpp>
#include <compare>
#include <format>
#include <numeric>
#include <order.hpp>
#include <utility>
#include <vector>

/*
Memory is allocated by 8 bytes: 2 ints

cacheline is 64: 16 ints

General  8 B:  2 int
XMM     16 B:  4 int
YMM     32 B:  8 int -- 1 order
ZMM     64 B: 16 int -- 2 orders
XMM Register 16 * 8 bytes: 32 ints, 4 orders
SSE Register 32 * 8 bytes: 64 ints, 8 orders
AVX Register 64 * 8 bytes: 128 ints

L1 Cache 32 * 128 * 8 bytes: 8,192 ints
*/
template <Side S, uint32_t MaxOrds = 6>
using TradeResult = absl::InlinedVector<Order<S, OrderType::Limit>, MaxOrds>;

/* 1 level is 40 + 32 * MaxOrds Bytes */
template <Side S, bool Shown = true, uint32_t MaxOrds = 6> class Level {
  boost::circular_buffer<Order<S, OrderType::Limit>> m_orders;
  Price m_price; // 4 bytes
  Size m_size;   // 4 bytes

private:
  __attribute__((always_inline)) inline void
  adjust_queues(decltype(m_orders.begin()) it, const Size size) {
    while (it != m_orders.end()) {
      it->second.queue -= std::min(it->second.queue, size);
      ++it;
    }
  }

  std::optional<Order<S>> cancel_order(decltype(m_orders.begin()) it) {
    if (it != m_orders.end()) {
      const auto ord = std::move(it->first); // move out the order
      if constexpr (Shown)
        m_size -= ord.size;    // adjust level size
      it = m_orders.erase(it); // remove order
      // update queue sizes for remaining orders
      adjust_queues(it, ord.size);
      return ord;
    }
    return std::nullopt;
  }

public:
  Level(Price _price) : m_orders(MaxOrds), m_price(_price), m_size(0) {}

  template <Side U>
  std::strong_ordering operator<=>(const Level<U> &rhs) const {
    return m_price <=> rhs.price();
  }
  // Compare with any numeric type
  template <typename T>
    requires(std::totally_ordered<T> &&
             (!std::is_same_v<T, Level<Side::Ask>>) &&
             (!std::is_same_v<T, Level<Side::Bid>>))
  std::strong_ordering operator<=>(const T &rhs) const {
    return m_price <=> rhs;
  }

  __attribute__((always_inline)) inline Price price() const { return m_price; }
  __attribute__((always_inline)) inline Size size() const { return m_size; }
  __attribute__((always_inline)) inline const auto &orders() const {
    return m_orders;
  }
  __attribute__((always_inline)) inline void add_liquidity(Size incr) {
    m_size += incr;
  }

  template <typename U> bool add_order(U &&order);

  auto find_id(Id id) -> std::optional<const Order<S> &>;

  auto cancel_id(Id id) -> std::optional<const Order<S> &>;

  auto cancel_all() -> decltype(m_orders);

  auto reduce_front(const Size size) -> TradeResult<S, MaxOrds>;

  auto walk_until_lifted(const Size size) -> TradeResult<S, MaxOrds>;

  // add method to lift liquidity l and our orders m, resolve incosistencies
};

template <Side S, bool Shown, uint32_t MaxOrds>
template <typename U>
bool Level<S, Shown, MaxOrds>::add_order(U &&order) {
  // add our order to the back
  static_assert(std::is_same_v<std::decay_t<U>, Order<S, OrderType::Limit>>,
                "Order must be of type Order<S, OrderType::Limit>");
  /* Exceeding the maximum allowed number of orders */
  if (m_orders.size() == MaxOrds) [[unlikely]]
    return false;

  m_orders.emplace_back(std::forward<U>(order));
  m_orders.back().cold->original_queue = m_size;
  m_orders.back().queue = m_size;

  if constexpr (Shown)
    m_size += m_orders.back().size;
  return true; // success
}
template <Side S, bool Shown, uint32_t MaxOrds>
std::optional<const Order<S> &> Level<S, Shown, MaxOrds>::find_id(Id id) {
  auto it = std::find_if(
      m_orders.begin(), m_orders.end(),
      [id](const auto &ord_pair) { return ord_pair.first.id == id; });
  if (it != m_orders.end())
    return it->first;
  return std::nullopt;
}

template <Side S, bool Shown, uint32_t MaxOrds>
auto Level<S, Shown, MaxOrds>::cancel_all() -> decltype(m_orders) {
  if constexpr (Shown) {
    m_size -= std::accumulate(
        m_orders.begin(), m_orders.end(), 0,
        [](auto acc, const auto &ord_pr) { return acc + ord_pr.first.size; });
  }
  auto ret = std::move(m_orders);
  m_orders.clear();
  return ret;
}
template <Side S, bool Shown, uint32_t MaxOrds>
std::optional<const Order<S> &> Level<S, Shown, MaxOrds>::cancel_id(Id id) {
  auto it = std::find_if(m_orders.begin(), m_orders.end(),
                         [id](const auto &ord) { return ord.id == id; });
  return cancel_order(it);
}
template <Side S, bool Shown, uint32_t MaxOrds>
TradeResult<S, MaxOrds>
Level<S, Shown, MaxOrds>::reduce_front(const Size size) {
  // processes the market transaction volume on the level
  // adjust for the shown orders
  TradeResult<S, MaxOrds> out;
  if (size >= m_size) { // lifted completely
    out.total_order_consumed = m_size;
    out.our_lifted_orders = std::move(m_orders);
    m_size = 0;
    m_orders.clear();
  } else { // lifted partially
    out.total_order_consumed = size;
    m_size -= size;

    auto it = m_orders.begin();
    if constexpr (Shown) { // allows partial fills
      while (it != m_orders.end() && (it->queue + it->size <= size)) {
        out.lifted_orders.emplace_back(std::move(*it));
        it = m_orders.erase(it);
      }
      if (it != m_orders.end() && it->queue < size) {
        const auto lifted_sz = size - it->queue;
        out.our_lifted_orders.push_back(*it);
        out.our_lifted_orders.back().size = lifted_sz;
        it->size -= lifted_sz;
        it->queue = 0;
        ++it;
      }
    } else { // no partial fills
      while (it != m_orders.end() && it->queue < size) {
        out.our_lifted_orders.emplace_back(std::move(*it));
        it = m_orders.erase(it);
      }
    }
    for (; it != m_orders.end(); ++it) {
      it->queue -= std::min(it->queue, size);
    }
  }
  return out;
}

template <Side S, bool Shown, uint32_t MaxOrds>
auto Level<S, Shown, MaxOrds>::walk_until_lifted(const Size size)
    -> TradeResult<S, MaxOrds> {
  // walks the order book until the privided size of our orders is executed
  TradeResult<S, MaxOrds> out;
  Size self_lifted = 0, traded_volume = 0;

  auto it = m_orders.begin();
  while (it != m_orders.end() && self_lifted < size) {
    const Size lift = std::min(it->size, size - self_lifted);
    self_lifted += lift;
    traded_volume = it->queue + lift * Shown;

    if (it->size == lift) [[likely]] { // lift fully
      out.lifted_orders.emplace_back(*it);
      it = m_orders.erase(it);
    } else { // lift partially
      out.lifted_orders.push_back(*it);
      out.lifted_orders.back.size = lift;

      it->size -= lift;
      it->queue = 0;
      ++it;
    }
  }
  if (self_lifted < size) { // not enough orders in the level
    out.market_volume = m_size;
    m_size = 0;
    return out;
  }
  // updating queues of the residual orders
  for (; it != m_orders.end(); ++it) {
    it->queue -= std::min(it->queue, traded_volume);
  }
  out.market_volume = traded_volume;
  m_size -= traded_volume;
  return out;
}