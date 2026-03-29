#pragma once

#include <ostream>

namespace order_book {

enum class OrderType {
    // Limit order
    GoodTillCancelled,
    ImmediateOrCancel,

    // Market order
    Market
};

enum class Side {
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