#pragma once

#include <cstdint>
#include <ostream>
#include <stdexcept>

namespace order_book {

using OrderId = std::uint64_t;
using Price = std::int32_t;
using Quantity = std::uint64_t;

enum class OrderType {
    GOOD_TILL_CANCELLED,
    IMMEDIATE_OR_CANCELLED,
};

enum class Side {
    BUY,
    SELL,
};

inline std::ostream& operator<<(std::ostream& os, const Side side) {
    switch (side) {
        case Side::BUY:
            os << "BUY";
            break;
        case Side::SELL:
            os << "SELL";
            break;
    }
    return os;
}

class Order {
  private:
    OrderId d_id;
    OrderType d_type;
    Side d_side;
    Price d_price;
    Quantity d_initialQuantity;
    Quantity d_remainingQuantity;

  public:
    Order(const OrderId id,
          const OrderType type,
          const Side side,
          const Price price,
          const Quantity initialQuantity);

    OrderId getId() const;
    Side getSide() const;
    Price getPrice() const;
    Quantity getRemainingQuantity() const;

    bool isFilled() const;

    /// @pre quantityToFill must be less than or equal to d_remainingQuantity
    void Fill(const Quantity quantityToFill);
};

inline Order::Order(const OrderId id,
                    const OrderType type,
                    const Side side,
                    const Price price,
                    const Quantity initialQuantity) :
    d_id(id),
    d_type(type),
    d_side(side),
    d_price(price),
    d_initialQuantity(initialQuantity),
    d_remainingQuantity(initialQuantity) {}

inline OrderId Order::getId() const {
    return d_id;
}

inline Side Order::getSide() const {
    return d_side;
}

inline Price Order::getPrice() const {
    return d_price;
}

inline Quantity Order::getRemainingQuantity() const {
    return d_remainingQuantity;
}

inline bool Order::isFilled() const {
    return getRemainingQuantity() == 0;
}

inline void Order::Fill(const Quantity quantityToFill) {
    d_remainingQuantity -= quantityToFill;
}

} // namespace order_book