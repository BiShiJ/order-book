// trade.h                                                                                                     -*-C++-*-
#pragma once

//@PURPOSE: Record a single execution between a bid order and an ask order.
//
//@CLASSES:
//  Trade: immutable description of one matched quantity at one price
//
//@MACROS:
//
//@DESCRIPTION: A trade ties bid and ask order ids to the price and size that cleared in one step of matching.

#include "aliases.h"
#include "order.h"

namespace order_book {

/// Value-semantic type describing one matched trade.
class Trade {
  private:
    OrderId d_bidOrderId;
    OrderId d_askOrderId;
    Price d_price;
    Quantity d_quantity;

  public:
    /// Aggregate used to construct a `Trade` with named fields.
    struct Args {
        OrderId bidOrderId;
        OrderId askOrderId;
        Price price;
        Quantity quantity;
    };

    /// Construct a trade from bid id, ask id, price, and quantity.
    explicit Trade(Args args);
};

inline Trade::Trade(const Args args) :
    d_bidOrderId(args.bidOrderId), d_askOrderId(args.askOrderId), d_price(args.price), d_quantity(args.quantity) {}

} // namespace order_book
