#pragma once

#include <cstdint>
#include <format>
#include <stdexcept>
#include <string>

namespace order_book {

enum class OrderType {
    GOOD_TILL_CANCELLED,
};

enum class Side {
    BUY,
    SELL,
};

using OrderId = std::uint64_t;
using Price = std::int32_t;
using Quantity = std::uint64_t;

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
    Price getPrice() const;
    Quantity getRemainingQuantity() const;

    bool isFilled() const;

    /// @throws std::invalid_argument if quantity > d_remainingQuantity
    void Fill(const Quantity quantity);
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

inline Price Order::getPrice() const {
    return d_price;
}

inline Quantity Order::getRemainingQuantity() const {
    return d_remainingQuantity;
}

inline bool Order::isFilled() const {
    return getRemainingQuantity() == 0;
}

/// @throws std::invalid_argument if quantityToFill > d_remainingQuantity
inline void Order::Fill(const Quantity quantityToFill) {
    if (quantityToFill > d_remainingQuantity) {
        throw std::invalid_argument(std::format(
            "Cannot fill order with more than its remaining quantity. "
            "orderId={}, remainingQuantity={}, quantityToFil={}",
            d_id, d_remainingQuantity, quantityToFill));
    }
    d_remainingQuantity -= quantityToFill;
}

} // namespace order_book