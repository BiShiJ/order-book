#pragma once

#include <iostream>
#include <optional>

#include "aliases.h"
#include "enums.h"

namespace order_book {

class Order {
  private:
    OrderId d_id;
    OrderType d_type;
    Side d_side;
    std::optional<Price> d_price;
    Quantity d_initialQuantity;
    Quantity d_remainingQuantity;

  public:
    /// @param[in] price absent for market orders until @c setPrice() assigns the synthetic limit.
    Order(OrderId id,
          OrderType type,
          Side side,
          std::optional<Price> price,
          Quantity initialQuantity);
    
    /**
     * Getters and setters
     */

    [[nodiscard]] OrderId getId() const;
    [[nodiscard]] OrderType getOrderType() const;
    [[nodiscard]] Side getSide() const;

    /// @pre Limit price is set ( not @c std::nullopt ).
    /// For market orders, only after @c setPrice() before book insertion.
    [[nodiscard]] Price getPrice() const;

    void setPrice(Price price);
    [[nodiscard]] Quantity getRemainingQuantity() const;

    /**
     * Order filling logic
     */

    [[nodiscard]] bool isFilled() const;

    /// @pre The @c quantityToFill value must be less than or equal to @c d_remainingQuantity.
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