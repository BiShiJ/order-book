#pragma once

#include "aliases.h"
#include "order.h"

namespace order_book {

class Trade {
  private:
    OrderId d_bidOrderId;
    OrderId d_askOrderId;
    Price d_price;
    Quantity d_quantity;

  public:
    Trade(const OrderId bidOrderId,
          const OrderId askOrderId,
          const Price price,
          const Quantity quantity);
};

inline Trade::Trade(const OrderId bidOrderId,
                    const OrderId askOrderId,
                    const Price price,
                    const Quantity quantity) :
    d_bidOrderId(bidOrderId), d_askOrderId(askOrderId), d_price(price), d_quantity(quantity) {}

} // namespace order_book