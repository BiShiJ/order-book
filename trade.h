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
    struct Args {
        OrderId bidOrderId;
        OrderId askOrderId;
        Price price;
        Quantity quantity;
    };

    explicit Trade(Args args);
};

inline Trade::Trade(const Args args) :
    d_bidOrderId(args.bidOrderId), d_askOrderId(args.askOrderId), d_price(args.price), d_quantity(args.quantity) {}

} // namespace order_book