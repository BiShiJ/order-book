#include <string>

namespace order_book {

enum class OrderType {
    GOOD_TILL_CANCELLED,
    IMMEDIATE_OR_CANCELLED,
};

enum class Side {
    BUY,
    SELL,
};

using OrderId = std::string;
using Price = int;
using Quantity = unsigned int;

class Order {
  private:
    OrderId d_id;
    OrderType d_type;
    Side d_side;
    Price d_price;
    Quantity d_initialQuantity;
    Quantity d_remainingQuantity;

  public:
    Order(OrderId id, OrderType type, Side side, Price price, Quantity initialQuantity);
    OrderId id() const;
    OrderType type() const;
    Side side() const;
    Price price() const;
    Quantity initialQuantity() const;
    Quantity remainingQuantity() const;
};

inline Order::Order(OrderId id, OrderType type, Side side, Price price, Quantity initialQuantity) :
    d_id(id),
    d_type(type),
    d_side(side),
    d_price(price),
    d_initialQuantity(initialQuantity),
    d_remainingQuantity(initialQuantity) {}

inline OrderId Order::id() const {
    return d_id;
}

inline OrderType Order::type() const {
    return d_type;
}

inline Side Order::side() const {
    return d_side;
}

inline Price Order::price() const {
    return d_price;
}

inline Quantity Order::initialQuantity() const {
    return d_initialQuantity;
}

inline Quantity Order::remainingQuantity() const {
    return d_remainingQuantity;
}

}