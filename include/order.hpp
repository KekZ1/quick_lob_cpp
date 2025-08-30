#pragma once
#include <utility.hpp>

// cold path info
struct OrderInfo {
  Size original_size = 0;    // 4 bytes
  Queue original_queue = 0;  // 4 bytes
};

/*
    1 order is 32 bytes: 8 ints, 1 YMM Regiter
    2 orders per cache line
*/
template <Side S, OrderType T = OrderType::Limit>
struct alignas(32) Order {
  Price price;
  Size size;
  Queue queue;
  Id id;  // if 0: not initialized
  /* 16 bytes */
  Offset offset;                    // 4 bytes
  Time time;                        // 4 bytes
  std::unique_ptr<OrderInfo> cold;  // never nullptr not to check
  /* 32 bytes */

  Order()
      : price(0),
        size(0),
        queue(0),
        id(0),
        offset(0),
        time(0),
        cold(std::make_unique<OrderInfo>()) {}

  template <OrderType U = T,
            typename = std::enable_if_t<U != OrderType::Market>>
  Order(Price _price, Size _size, Id _id, Time _time)
      : price(_price),
        size(_size),
        queue(0),
        id(_id),
        offset(Offset::Open),
        time(_time),
        cold(std::make_unique<OrderInfo>(OrderInfo{.original_size = _size})) {}
  template <OrderType U = T,
            typename = std::enable_if_t<U == OrderType::Market>>
  Order(Size _size, Id _id, Time _time)
      : price(0),  // no price for market orders
        size(_size),
        queue(0),
        id(_id),
        offset(Offset::Open),
        time(_time),
        cold(std::make_unique<OrderInfo>(OrderInfo{.original_size = _size})) {}

  template <OrderType U = T,
            typename = std::enable_if_t<U != OrderType::Market>>
  Order(Price _price, Size _size, Queue _queue, Id _id, Time _time)
      : price(_price),
        size(_size),
        queue(_queue),
        id(_id),
        offset(Offset::Open),
        time(_time),
        cold(std::make_unique<OrderInfo>(OrderInfo{.original_size = _size})) {}
  

  template <OrderType U = T,
            typename = std::enable_if_t<U != OrderType::Market>>
  Order(Price _price,
        Size _size,
        Id _id,
        Time _time,
        Offset _offset,
        OrderInfo&& _cold)
      : price(_price),
        size(_size),
        queue(0),
        id(_id),
        offset(_offset),
        time(_time),
        cold(std::make_unique<OrderInfo>(std::move(_cold))) {
    cold->original_size = _size;
  }
  template <OrderType U = T,
            typename = std::enable_if_t<U == OrderType::Market>>
  Order(Size _size, Id _id, Time _time, Offset _offset, OrderInfo&& _cold)
      : price(0),  // no price for market orders
        size(_size),
        queue(0),
        id(_id),
        offset(_offset),
        time(_time),
        cold(std::make_unique<OrderInfo>(std::move(_cold))) {
    cold->original_size = _size;
  }
};