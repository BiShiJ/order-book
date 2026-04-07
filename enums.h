// enums.h                                                                                                     -*-C++-*-
#pragma once

//@PURPOSE: Define enumerations for order types and buy/sell side.
//
//@CLASSES:
//
//@MACROS:
//
//@DESCRIPTION: These enumerations classify how orders participate in matching and whether they are bids or offers.

#include <cstdint>
#include <ostream>

namespace order_book {

/// Enumeration used to classify limit and market order behaviors.
enum class OrderType : std::uint8_t {
    // Limit order
    GoodTilCanceled,
    ImmediateOrCancel,
    FillOrKill,
    Day,

    // Market order
    Market
};

/// Enumeration used to distinguish buy and sell interest.
enum class Side : std::uint8_t {
    Buy,
    Sell,
};

inline std::ostream& operator<<(std::ostream& os, const Side side) {
    switch (side) {
        case Side::Buy:
            os << "BUY";
            break;
        case Side::Sell:
            os << "SELL";
            break;
    }
    return os;
}

} // namespace order_book
