#pragma once
#include <utility.hpp>

// context path info
struct OrderInfo {
  Size original_size = 0;       // 4 bytes
  Queue original_queue = 0;     // 4 bytes
  Offset offset = Offset::Open; // 4 bytes
  Time time = 0;                // 4 bytes
};

/*
    1 order is 32 bytes: 8 ints, 1 YMM Regiter
    2 orders per cache line
*/
static_assert(sizeof(OrderInfo) == 16, "OrderInfo size must be 16 bytes");

template <Side S, OrderType T = OrderType::Limit> struct alignas(32) Order {
  Price price;
  Size size;
  /* 4*2 bytes */
  Queue queue;
  Id id;
  /* 4*4 bytes */
  std::unique_ptr<OrderInfo> context; // never nullptr not to check
  /* 4*6 bytes */

  Order()
      : price(0), size(0), queue(0), id(0),
        context(std::make_unique<OrderInfo>()) {}
  Order(Price p, Size s, Queue q, Id i, Time t, Offset offset)
      : price(p), size(s), queue(q), id(i),
        context(std::make_unique<OrderInfo>(s, queue, offset, t)) {}

  // Copy constructor (deep copy)
  Order(const Order &other);
  // Copy assignment
  Order &operator=(const Order &other);
};

static_assert(sizeof(Order<Side::Ask>) == 32, "Order size must be 32 bytes");

template <Side S, OrderType T>
Order<S, T>::Order(const Order<S, T> &other)
    : price(other.price), size(other.size), queue(other.queue), id(other.id),
      context(other.context ? std::make_unique<OrderInfo>(*other.context)
                            : nullptr) {}

template <Side S, OrderType T>
Order<S, T> &Order<S, T>::operator=(const Order<S, T> &other) {
  if (this != &other) {
    price = other.price;
    size = other.size;
    queue = other.queue;
    id = other.id;
    context =
        other.context ? std::make_unique<OrderInfo>(*other.context) : nullptr;
  }
  return *this;
}