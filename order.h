// order.h                                                                                                     -*-C++-*-
#pragma once

//@PURPOSE: Represent a single order with type, side, optional limit price, and remaining quantity.
//
//@CLASSES:
//  Order: value-like order state for matching and lifecycle
//
//@MACROS:
//
//@DESCRIPTION: Limit orders carry a price; market orders use `std::nullopt` until the book assigns a synthetic price.
// Callers must respect documented preconditions when reading price or applying fills.

#include <iostream>
#include <optional>

#include "aliases.h"
#include "enums.h"

namespace order_book {

/// Mechanism class holding mutable order state used by `OrderBook` during matching.
class Order {
  private:
    OrderId d_id;
    OrderType d_type;
    Side d_side;
    std::optional<Price> d_price;
    Quantity d_initialQuantity;
    Quantity d_remainingQuantity;

  public:
    /// Create an order with the given id, type, side, optional limit price, and size.
    /// @param[in] price absent for market orders until `setPrice` assigns the synthetic limit.
    Order(OrderId id,
          OrderType type,
          Side side,
          std::optional<Price> price,
          Quantity initialQuantity);

    /// Return this order's id.
    [[nodiscard]] OrderId getId() const;

    /// Return limit vs market classification.
    [[nodiscard]] OrderType getOrderType() const;

    /// Return buy or sell side.
    [[nodiscard]] Side getSide() const;

    /// Return the working limit price.
    /// @pre Limit price is set (not `std::nullopt`). For market orders, only after `setPrice` before book insertion.
    [[nodiscard]] Price getPrice() const;

    /// Set the working limit price (including synthetic price for market orders).
    void setPrice(Price price);

    /// Return quantity not yet matched or canceled.
    [[nodiscard]] Quantity getRemainingQuantity() const;

    /// Return whether remaining quantity is zero.
    [[nodiscard]] bool isFilled() const;

    /// Reduce remaining quantity by the fill amount.
    /// @pre `quantityToFill` must not exceed `d_remainingQuantity`.
    void Fill(Quantity quantityToFill);
};

inline Order::Order(const OrderId id,
                    const OrderType type,
                    const Side side,
                    const std::optional<Price> price,
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

inline OrderType Order::getOrderType() const {
    return d_type;
}

inline Side Order::getSide() const {
    return d_side;
}

inline Price Order::getPrice() const {
    if (d_price.has_value()) {
        return d_price.value();
    }

    std::cerr << "d_price is std::nullopt. Trying to access market order d_price before it is set.\n";
    return Price{};
}

inline void Order::setPrice(const Price price) {
    d_price = price;
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
