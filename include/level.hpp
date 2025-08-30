#pragma once
#include <boost/circular_buffer.hpp>
#include <compare>
#include <format>
#include <numeric>
#include <order.hpp>
#include <utility>
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
struct TransactionResult {
  bc::static_vector<Order<S, OrderType::Limit>, MaxOrds> lifted_orders;
  Size lifted_volume = 0;    // our orders
  Size executed_volume = 0;  // in total
};

/* 1 level is 40 + 32 * MaxOrds Bytes */
template <Side S, bool Shown = true, uint32_t MaxOrds = 6>
class Level {
  Order<S, OrderType::Limit> buffer[MaxOrds];  // stack buffer
  /* MaxOrds * 32 bytes */
  boost::circular_buffer<Order<S, OrderType::Limit>> m_orders;
  /* 32 bytes */
  Price m_price;  // 4 bytes
  Size m_size;    // 4 bytes

 private:
  inline void adjust_queues(decltype(m_orders.begin()) it, const Size size) {
    while (it != m_orders.end()) {
      it->second.queue -= std::min(it->second.queue, size);
      ++it;
    }
  }

  std::optional<Order<S>> cancel_order(decltype(m_orders.begin()) it) {
    if (it != m_orders.end()) {
      const auto ord = std::move(it->first);  // move out the order
      if constexpr (Shown)
        m_size -= ord.size;     // adjust level size
      it = m_orders.erase(it);  // remove order
      // update queue sizes for remaining orders
      adjust_queues(it, ord.size);
      return ord;
    }
    return std::nullopt;
  }

 public:
  Level(Price _price) : m_orders(buffer, MaxOrds), m_price(_price), m_size(0) {}

  std::strong_ordering operator<=>(const Level<S>& rhs) const {
    return m_price <=> rhs.price();
  }
  std::strong_ordering operator<=>(const Level<Opp<S>>& rhs) const {
    return m_price <=> rhs.price();
  }
  std::strong_ordering operator<=>(Price rhs) const { return m_price <=> rhs; }

  __attribute__((always_inline)) inline Price price() const { return m_price; }
  __attribute__((always_inline)) inline Size size() const { return m_size; }
  void increase_size(Size incr) { m_size += incr; }

  const auto& orders() const { return m_orders; }

  template <typename U>
  bool add_order(U&& order) {
    static_assert(std::is_same_v<std::decay_t<U>, Order<S, OrderType::Limit>>,
                  "Order must be of type Order<S, OrderType::Limit>");
    /* Exceeding the maximum allowed number of orders */
    if (m_orders.size() == MaxOrds) [[unlikely]]
      return false;

    m_orders.cold->original_queue = m_size;

    m_orders.emplace_back(std::forward<U>(order));
    if constexpr (Shown)
      m_size += m_orders.back().size;
    return true;  // success
  }

  std::optional<Order<S>> cancel_id(Id id) {
    auto it = std::find_if(
        m_orders.begin(), m_orders.end(),
        [id](const auto& ord_pair) { return ord_pair.first.id == id; });
    cancel_order(it);
  }

  std::optional<const Order<S>&> find_id(Id id) {
    auto it = std::find_if(
        m_orders.begin(), m_orders.end(),
        [id](const auto& ord_pair) { return ord_pair.first.id == id; });
    if (it != m_orders.end())
      return it->first;
    return std::nullopt;
  }

  auto cancel_all() -> decltype(m_orders) {
    if constexpr (Shown) {
      m_size -= std::accumulate(
          m_orders.begin(), m_orders.end(), 0,
          [](auto acc, const auto& ord_pr) { return acc + ord_pr.first.size; });
    }
    auto ret = std::move(m_orders);
    m_orders.clear();
    return ret;
  }

  auto process_market_size(const Size size) -> TransactionResult<S, MaxOrds> {
    // processes the market transaction volume on the level
    // adjust for the shown orders
    TransactionResult<S, MaxOrds> out;
    auto it = m_orders.begin();
    while (out.lifted_volume < size && it != m_orders.end()) {
      ;
    }
  }

  /*
    auto walk_until_lifted(const Size size)
        -> bc::static_vector<Order<Opp<S>>, MaxOrds> {
      // walks the level until lifted `size` of our orders
      auto rit = m_orders.rbegin();
      decltype(m_orders) out;
      Size lifted_sz = 0;
      while (rit != m_orders.rend()) {
        if (lifted_sz < size) {
          // rit->
        }
      }
      while (rit != m_orders.rend()) {
        if (rit->size <= resid) {
          rit->queue = 0;
          resid -= rit->size;
          out.push_back(std::move(*rit));
          rit =
    std::make_reverse_iterator(m_orders.erase(std::next(rit).base())); } else {
          if (resid != 0) {
            out.push_back(*rit);
            out.back().size = resid;
            out.back().queue = 0;
          }
          rit->size -= resid;
          rit->queue -= std::min<Size>(rit->queue, size);
          resid = 0;
          ++rit;
        }
      }
      return out;
    }
  */

  // bc::static_vector<Order<Opp<S>>, MaxOrds> walk(Size size) {
  //   auto rit = m_orders.rbegin();
  //   bc::static_vector<Order<Opp<S>>, MaxOrds> out;

  //   Size resid = size;
  //   m_size -= std::min<Size>(m_size, resid);
  //   while (rit != m_orders.rend()) {
  //     if (rit->size <= resid) {
  //       rit->queue = 0;
  //       resid -= rit->size;
  //       out.push_back(std::move(*rit));
  //       rit =
  //       std::make_reverse_iterator(m_orders.erase(std::next(rit).base()));
  //     } else {
  //       if (resid != 0) {
  //         out.push_back(*rit);
  //         out.back().size = resid;
  //         out.back().queue = 0;
  //       }
  //       rit->size -= resid;
  //       rit->queue -= std::min<Size>(rit->queue, size);
  //       resid = 0;
  //       ++rit;
  //     }
  //   }
  //   return out;
  // }
  // transaction with capped lift
};