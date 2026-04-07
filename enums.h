#pragma once

#include <cstdint>
#include <ostream>

namespace order_book {

enum class OrderType : std::uint8_t {
    // Limit order
    GoodTilCanceled,
    ImmediateOrCancel,
    FillOrKill,
    Day,

    // Market order
    Market
};

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